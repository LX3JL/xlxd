//
//  cp25protocol.h
//  xlxd
//
//  Created for P25 Reflector peering support
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

#ifndef cp25protocol_h
#define cp25protocol_h

#include <cstring>
#include <vector>
#include <map>
#include <mutex>
#include "ctimepoint.h"
#include "cprotocol.h"
#include "cdvheaderpacket.h"
#include "cdvframepacket.h"
#include "cdvlastframepacket.h"
#include "p25defines.h"
#include "cp25peer.h"
#include "cpeercallsignlist.h"

////////////////////////////////////////////////////////////////////////////////////////
// class

class CP25StreamCacheItem
{
public:
    CP25StreamCacheItem()      { m_uiSrcId = 0; m_uiDstTg = 0; m_uiFrameCount = 0; m_bPendingHeader = false; m_bHasOwner = false; }
    ~CP25StreamCacheItem()     { ClearPendingFrames(); }

    std::mutex      m_Mutex;        // protects fields from RX/TX thread races

    void ClearPendingFrames(void)
    {
        for (size_t i = 0; i < m_PendingFrames.size(); i++)
            delete m_PendingFrames[i];
        m_PendingFrames.clear();
        m_bPendingHeader = false;
    }

    CDvHeaderPacket m_dvHeader;
    uint32_t m_uiSrcId;
    uint32_t m_uiDstTg;
    unsigned int m_uiFrameCount;      // Counter for outgoing frame position (0-17)

    // Deferred header: buffer voice frames until source ID arrives
    bool m_bPendingHeader;
    uint32_t m_uiPendingStreamId;
    CIp m_PendingIp;
    std::vector<CDvFramePacket *> m_PendingFrames;
    CIp     m_OwnerIp;
    bool    m_bHasOwner;
};

class CP25Protocol : public CProtocol
{
public:
    // constructor
    CP25Protocol();

    // destructor
    virtual ~CP25Protocol() {};

    // initialization
    bool Init(void);
    void Close(void);

    // split-thread mode
    bool UsesSplitThreads(void) const { return true; }
    void RxTask(void);
    void TxTask(void);

protected:
    // queue helper
    void HandleQueue(void);

    // keepalive helpers
    void HandlePeerLinks(void);
    void HandlePeerKeepalives(void);

    // packet handlers
    void OnLduPacketIn(const CBuffer &, const CIp &, bool);
    void OnTerminatorPacketIn(const CIp &);

    // stream helpers
    bool OnDvHeaderPacketIn(CDvHeaderPacket *, const CIp &);

    // packet decoding helpers
    bool IsValidPollPacket(const CBuffer &, CCallsign *);
    bool ExtractImbeFromRecord(const CBuffer &, uint8 *);

    // packet encoding helpers
    void EncodePollPacket(CBuffer *, const CCallsign &) const;
    void EncodeVoicePacket(CBuffer *, unsigned int, const uint8 *, uint32_t, uint32_t) const;
    void EncodeTerminatorPacket(CBuffer *) const;

    // peer helpers
    bool IsP25PeerCallsign(const CCallsign &) const;
    CCallsignListItem *FindP25PeerByIp(CPeerCallsignList *, const CIp &);

    // P25 ID helpers
    uint32_t CallsignToP25Id(const CCallsign &) const;
    bool P25IdToCallsign(uint32_t p25Id, CCallsign *callsign) const;

    // uiStreamId helpers
    //
    // P25 derives the reflector-side stream-id from the source endpoint.
    // The stateless deterministic IpToStreamId() collides on rapid
    // re-keys: when the same source keys up before the previous stream's
    // CloseStream() drain has finished (up to MAX_DRAIN_WAIT_MS = 2000ms),
    // both transmissions share an internal id and the wire ends up
    // carrying voice for the second over under the first stream-id with
    // its EoT already emitted — multi-EoT bug. Fix: each new header
    // (HDU / first LDU) allocates a fresh per-protocol stream-id and
    // stores the IP→sid mapping; voice / terminator (TDU) frames look
    // up the active sid. Deterministic IpToStreamId() retained as
    // fallback for orphan mid-stream packets so late-entry buffering
    // still works. See cysfprotocol.cpp / cnxdnprotocol.cpp for the
    // same pattern.
    //
    // CProtocol::CheckStreamsTimeout does NOT clean m_SourceStates on
    // stream timeout — only an explicit TDU drops the mapping. See
    // cysfprotocol.h for the full discussion (same caveat applies).
    uint32 AllocateNewStreamIdForSource(const CIp &);
    uint32 LookupStreamIdForSource(const CIp &) const;
    bool   HasActiveStreamIdForSource(const CIp &) const;
    void   ReleaseStreamIdForSource(const CIp &);
    uint32 IpToStreamId(const CIp &) const;

protected:
    // for peer connections
    CTimePoint          m_LastPeerLinkTime;
    CTimePoint          m_LastPeerKeepaliveTime;

    // for queue header caches
    std::array<CP25StreamCacheItem, NB_OF_MODULES>    m_StreamsCache;

    // per-source stream-id allocation (see comment above the helpers).
    // sid values come from the process-wide
    // CProtocol::AllocateGlobalStreamIdNonce.
    mutable std::mutex                m_SourceStatesMutex;
    std::map<uint64_t, uint32>        m_SourceStates;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cp25protocol_h */
