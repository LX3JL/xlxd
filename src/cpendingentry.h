//
//  cpendingentry.h
//  xlxd
//
//  Created for late-entry stream support.
//  Copyright © 2025. All rights reserved.
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

#ifndef cpendingentry_h
#define cpendingentry_h

#include "main.h"
#include "cdvheaderpacket.h"
#include "cdvframepacket.h"
#include "cpacket.h"
#include "ctimepoint.h"
#include <deque>

// Forward declarations
class CClient;
class CPacketStream;

////////////////////////////////////////////////////////////////////////////////////////
// defines

#define PENDING_TIMEOUT             2.0     // seconds to hold pending entry
#define PENDING_BUFFER_MAX_FRAMES   100     // ~2s at 20ms/frame

////////////////////////////////////////////////////////////////////////////////////////
// class

class CPendingEntry
{
public:
    // state machine
    enum State { INACTIVE, BUFFERING, HEADER_ONLY, PROMOTED };

    // constructor / destructor
    CPendingEntry();
    ~CPendingEntry();

    // lifecycle
    void Set(CDvHeaderPacket *header, CClient *client);
    void Clear(void);

    // state queries
    State GetState(void) const              { return m_State; }
    bool  IsActive(void) const              { return m_State != INACTIVE; }
    bool  IsBuffering(void) const           { return m_State == BUFFERING; }
    bool  IsPromoted(void) const            { return m_State == PROMOTED; }
    bool  IsExpired(void) const;
    bool  IsBufferFull(void) const          { return (int)m_Frames.size() >= PENDING_BUFFER_MAX_FRAMES; }

    // buffer a frame (takes ownership). Returns false if not buffering.
    bool BufferFrame(CPacket *frame);

    // stop buffering, discard frames, keep header (buffer full transition)
    void TransitionToHeaderOnly(void);

    // promote: set the stream pointer for frame forwarding
    void Promote(CPacketStream *stream);

    // replay buffered frames into a stream (transfers ownership)
    void ReplayInto(CPacketStream *stream);

    // accessors
    CDvHeaderPacket *GetHeader(void)        { return m_Header; }
    CClient         *GetClient(void)        { return m_Client; }
    uint16          GetStreamId(void) const { return m_uiStreamId; }
    CPacketStream   *GetPromotedStream(void){ return m_PromotedStream; }

private:
    // cleanup helper
    void DiscardFrames(void);

    // data
    State                   m_State;
    CDvHeaderPacket        *m_Header;           // owned, heap-allocated
    CClient                *m_Client;           // NOT owned, borrowed pointer
    uint16                  m_uiStreamId;
    CTimePoint              m_CreatedTime;
    std::deque<CPacket *>   m_Frames;           // owned, heap-allocated
    CPacketStream          *m_PromotedStream;   // NOT owned, set during PROMOTED
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cpendingentry_h */
