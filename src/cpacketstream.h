//
//  cpacketstream.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 06/11/2015.
//  Copyright © 2015 Jean-Luc Deltombe (LX3JL). All rights reserved.
//
// ----------------------------------------------------------------------------
//    This file is part of xlxd.
//
//    xlxd is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    xlxd is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with Foobar.  If not, see <http://www.gnu.org/licenses/>. 
// ----------------------------------------------------------------------------

#ifndef cpacketstream_h
#define cpacketstream_h

#include <atomic>
#include <queue>
#include "cpacketqueue.h"
#include "ctimepoint.h"
#include "cdvheaderpacket.h"
#include "ctranscoder.h"

////////////////////////////////////////////////////////////////////////////////////////

//#define STREAM_TIMEOUT      (0.600)
#define STREAM_TIMEOUT      (1.600)

// Maximum number of voice frames held in the pre-codec ring buffer while
// the transcoder stream is being opened. Sized to comfortably cover the
// 1050ms AMBEd handshake window at a 20ms frame cadence: 100 frames × 20ms
// = 2 seconds. Frames are preserved (not dropped) and replayed through
// the transcoder once it attaches so cross-mode listeners receive the full
// transmission including the first second.
#define PRE_CODEC_BUFFER_MAX        100

////////////////////////////////////////////////////////////////////////////////////////
// class

class CPacketStream : public CPacketQueue
{
public:
    // constructor
    CPacketStream();
    
    // destructor
    virtual ~CPacketStream() {};

    // open / close
    // Open() is the "phase 1" of a two-phase open. It reserves the stream
    // and records the owner client, but does NOT negotiate with the
    // transcoder (that happens asynchronously via AttachCodecStream after
    // the caller releases reflector locks). Voice frames arriving between
    // Open() and AttachCodecStream() are held in a pre-codec ring buffer
    // and replayed through the transcoder on attach, so no audio is lost.
    //
    // The DvHeader itself is ALSO deferred — not pushed to the base stream
    // queue at Open() time. Instead m_bHasPendingHeader is set and the
    // header is emitted at the top of AttachCodecStream(), immediately
    // before the pre-codec buffer drains. This keeps header → first-voice
    // timing tight on the wire (~200ms gap, matching native D-Star within
    // the AMBEd handshake window) instead of the ~1-1.3s "orphan header"
    // gap that the old immediate-push behaviour produced. Strict D-Star
    // gateways (e.g. older ircDDBGateway) treat the orphan-header gap as
    // stream-abort; deferring eliminates that failure mode.
    //
    // AttachCodecStream(codecStream, transcodeRequested=true):
    //   - codecStream may be NULL, meaning no transcoder is attached
    //   - transcodeRequested=false disables the "ambed unavailable" warning
    //     for paths that deliberately skip transcoding (e.g. late-entry
    //     promotion, which cannot afford the blocking AMBEd handshake)
    bool Open(const CDvHeaderPacket &, CClient *);
    void AttachCodecStream(CCodecStream *codecStream, bool transcodeRequested = true);
    bool IsCodecPending(void) const                 { return m_bCodecPending; }
    void Close(void);

    // push & pop
    void Push(CPacket *);
    void Tickle(void)                               { m_LastPacketTime.Now(); }
    bool IsEmpty(void);
    size_t GetPipelineDepth(void);

    // close serialization — CReflector::CloseStream uses this to guarantee
    // only one thread runs the close path on a given stream, which in turn
    // lets IsEmpty() / the drain loop safely read m_CodecStream without
    // racing against Close() freeing it.
    bool TryBeginClose(void)                        { bool expected = false; return m_bClosing.compare_exchange_strong(expected, true); }
    void EndClose(void)                             { m_bClosing.store(false); }
    
    // get
    CClient         *GetOwnerClient(void)           { return m_OwnerClient; }
    void            SetOwnerClient(CClient *c)     { m_OwnerClient = c; }
    const CIp       *GetOwnerIp(void);
    bool            IsExpired(void) const           { return (m_LastPacketTime.DurationSinceNow() > STREAM_TIMEOUT); }
    bool            IsOpen(void) const              { return m_bOpen; }
    uint16          GetStreamId(void) const         { return m_uiStreamId; }
    const CCallsign &GetUserCallsign(void) const    { return m_DvHeader.GetMyCallsign(); }
    // Read-only view of the header cached at Open(). CCodecStream needs this
    // to derive the source mode (SUFFIX) and callsigns when synthesising
    // D-Star slow-data for AMBE+ transcoded egress. Do not mutate through
    // this reference — the router is allowed to rewrite RPT2 on the stream's
    // in-flight header copies, but m_DvHeader here is the opening snapshot.
    const CDvHeaderPacket &GetDvHeader(void) const  { return m_DvHeader; }
    uint32          GetPacketCount(void) const      { return m_uiPacketCntr; }
    uint8           GetInputCodec(void) const       { return m_uiInputCodec; }

protected:
    // data
    std::atomic<bool>   m_bOpen;
    std::atomic<bool>   m_bClosing;     // set true for the duration of CReflector::CloseStream
    std::atomic<bool>   m_bCodecPending; // true between Open() and AttachCodecStream()
    // m_bHasPendingHeader: set in Open() alongside m_DvHeader, cleared by
    // AttachCodecStream() after the deferred header push (or on early-close
    // cleanup). Also cleared in Close() for hygiene. Typed as std::atomic<bool>
    // to match the sibling lifecycle flags — normal-path reads/writes occur
    // under the stream lock, but the Close() clear runs without the stream
    // lock held (see creflector.cpp:487 "unlock before closing to avoid
    // double lock in associated codecstream close/thread-join"). Atomic
    // typing makes that unprotected write explicitly race-free and makes
    // the safety self-enforcing against future refactors that might add
    // an AttachCodecStream call-site bypassing the m_bClosing guard.
    std::atomic<bool>   m_bHasPendingHeader;
    std::atomic<uint16> m_uiStreamId;
    uint32              m_uiPacketCntr;
    uint8               m_uiInputCodec;
    CClient             *m_OwnerClient;
    CIp                 m_CachedOwnerIp;
    std::atomic<bool>   m_bHasCachedOwnerIp;
    CTimePoint          m_LastPacketTime;
    CDvHeaderPacket     m_DvHeader;
    CCodecStream        *m_CodecStream;

    // pre-codec ring buffer — holds voice frames arriving between Open()
    // and AttachCodecStream(). Protected by the CPacketStream lock (the
    // same one acquired by Push() callers and by AttachCodecStream()).
    std::queue<CPacket *> m_PreCodecBuffer;
    uint32              m_uiPreCodecDropped;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cpacketstream_h */
