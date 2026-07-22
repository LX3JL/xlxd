//
//  cnxdnprotocol.h
//  xlxd
//
//  Created for NXDN Reflector peering support
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

#ifndef cnxdnprotocol_h
#define cnxdnprotocol_h

#include <cstring>
#include <map>
#include <mutex>
#include "ctimepoint.h"
#include "cprotocol.h"
#include "cdvheaderpacket.h"
#include "cdvframepacket.h"
#include "cdvlastframepacket.h"
#include "nxdndefines.h"
#include "cnxdnpeer.h"
#include "cpeercallsignlist.h"

////////////////////////////////////////////////////////////////////////////////////////
// class

class CNxdnStreamCacheItem
{
public:
    CNxdnStreamCacheItem()      { m_uiSrcId = 0; m_uiDstId = 0; m_uiFrameCount = 0; m_uiPacketCount = 0; m_uiYsfSubId = 0; m_bHasOwner = false; ::memset(m_ambeFrames, 0, sizeof(m_ambeFrames)); }
    ~CNxdnStreamCacheItem()     {}

    // m_Mutex protects fields shared between RX and TX threads:
    //   m_uiSrcId, m_uiDstId, m_uiYsfSubId, m_bHasOwner, m_OwnerIp, m_dvHeader
    // TX-only fields (no lock needed — only written/read by HandleQueue):
    //   m_ambeFrames, m_uiFrameCount, m_uiPacketCount
    std::mutex      m_Mutex;

    CDvHeaderPacket m_dvHeader;         // shared: written by TX header, read by TX voice/last
    uint16_t m_uiSrcId;                // shared: written by RX+TX, read by TX
    uint16_t m_uiDstId;                // shared: written by RX+TX, read by TX

    // TX-only: frame buffer for NXDN output (need 4 AMBE frames per NXDN packet)
    uint8_t m_ambeFrames[4][9];
    uint8_t m_uiFrameCount;            // TX-only: outgoing frame buffer counter (0-3)
    uint8_t m_uiPacketCount;           // TX-only: outgoing packet counter (SACCH cycling)

    uint8_t m_uiYsfSubId;              // shared: incoming YSF subpacket ID (0-4)
    CIp     m_OwnerIp;                 // shared: stream owner IP for echo prevention
    bool    m_bHasOwner;               // shared: owner flag
};

class CNxdnProtocol : public CProtocol
{
public:
    // constructor
    CNxdnProtocol();

    // destructor
    virtual ~CNxdnProtocol() {};

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

    // stream helpers
    bool OnDvHeaderPacketIn(CDvHeaderPacket *, const CIp &, uint16_t srcId, uint16_t dstId);

    // packet decoding helpers
    bool IsValidPollPacket(const CBuffer &, CCallsign *, uint16_t *);
    bool IsValidDataPacket(const CBuffer &, uint16_t *, uint16_t *, uint8_t *, uint8_t *);

    // packet encoding helpers
    void EncodePollPacket(CBuffer *, const CCallsign &, uint16_t) const;
    void EncodeDataPacket(CBuffer *, uint16_t, uint16_t, uint8_t, const uint8_t *) const;

    // peer helpers
    bool IsNxdnPeerCallsign(const CCallsign &) const;
    CCallsignListItem *FindNxdnPeerByIp(CPeerCallsignList *, const CIp &);

    // NXDN ID helpers
    uint16_t CallsignToNxdnId(const CCallsign &) const;

    // uiStreamId helpers
    //
    // NXDN derives the reflector-side stream-id from the source endpoint.
    // The stateless deterministic IpToStreamId() collides on rapid
    // re-keys: when the same hotspot keys up before the previous stream's
    // CloseStream() drain has finished (up to MAX_DRAIN_WAIT_MS = 2000ms),
    // both transmissions share an internal id and the wire ends up
    // carrying voice for the second over under the first stream-id with
    // its EoT already emitted — multi-EoT bug. Fix: each new header
    // allocates a fresh per-protocol stream-id and stores the IP→sid
    // mapping; voice / terminator frames look up the active sid for
    // their source. The deterministic IpToStreamId() is kept as a
    // fallback for orphan mid-stream packets so existing late-entry
    // buffering still works. See cysfprotocol.cpp for the same pattern.
    //
    // CProtocol::CheckStreamsTimeout does NOT clean m_SourceStates on
    // stream timeout — only an explicit trailer drops the mapping. See
    // cysfprotocol.h for the full discussion (same caveat applies).
    uint32 AllocateNewStreamIdForSource(const CIp &);
    uint32 LookupStreamIdForSource(const CIp &) const;
    void   ReleaseStreamIdForSource(const CIp &);
    uint32 IpToStreamId(const CIp &) const;

protected:
    // for peer connections
    CTimePoint          m_LastPeerLinkTime;
    CTimePoint          m_LastPeerKeepaliveTime;

    // for queue header caches
    std::array<CNxdnStreamCacheItem, NB_OF_MODULES>    m_StreamsCache;

    // per-source stream-id allocation (see comment above the helpers).
    // sid values come from the process-wide
    // CProtocol::AllocateGlobalStreamIdNonce.
    mutable std::mutex                m_SourceStatesMutex;
    std::map<uint64_t, uint32>        m_SourceStates;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cnxdnprotocol_h */
