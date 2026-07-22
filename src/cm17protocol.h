//
//  cm17protocol.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 28/11/2025.
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

#ifndef cm17protocol_h
#define cm17protocol_h

#include <cstdint>
#include <cstring>
#include <vector>
#include <mutex>
#include <random>
#include "ctimepoint.h"
#include "cprotocol.h"
#include "cdvheaderpacket.h"
#include "cdvframepacket.h"
#include "cdvlastframepacket.h"

// Type alias for 64-bit unsigned
typedef uint64_t uint64;

////////////////////////////////////////////////////////////////////////////////////////
// define

// M17 frame type
#define M17_FRAMETYPE_VOICE     0x05

// M17 callsign encoding
#define M17_CALLSIGN_ALPHABET   " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-/."

// Client cache refresh interval in seconds
#define M17_CLIENT_CACHE_REFRESH_INTERVAL   1.0

////////////////////////////////////////////////////////////////////////////////////////
// class

class CM17StreamCacheItem
{
public:
    CM17StreamCacheItem() : m_uiOutboundStreamId(0), m_uiInternalStreamId(0), m_uiNextStreamId(1),
                            m_uiFrameSequence(0), m_bHasPendingCodec2(false),
                            m_bHasOwner(false), m_bStreamActive(false), m_uiPacketsIn(0), m_uiPacketsTranscoded(0)
    {
        ::memset(m_uiPendingCodec2, 0, sizeof(m_uiPendingCodec2));
        // seed with a random starting value so different modules don't start at the same ID
        try {
            std::random_device rd;
            m_uiNextStreamId = (uint16)(rd() & 0xFFFF);
            if ( m_uiNextStreamId == 0 ) m_uiNextStreamId = 1;
        } catch (...) {
            m_uiNextStreamId = 1;
        }
    }
    ~CM17StreamCacheItem()    {}

    std::mutex      m_Mutex;        // protects fields from RX/TX thread races

    // Reset stats for new stream
    void ResetStats(void)
    {
        m_uiPacketsIn = 0;
        m_uiPacketsTranscoded = 0;
    }

    CDvHeaderPacket m_dvHeader;
    uint16          m_uiOutboundStreamId;   // M17 wire stream ID (generated, for encoding only)
    uint16          m_uiInternalStreamId;   // reflector internal stream ID (for owner exclusion / EOT checks)
    uint16          m_uiNextStreamId;       // counter for generating unique wire IDs
    uint16          m_uiFrameSequence;      // M17 frame sequence number
    // Codec2 frame buffering: M17 uses 40ms packets (2 x 8-byte frames)
    // XLXd internally uses 20ms frames, so we buffer one frame
    uint8           m_uiPendingCodec2[8];   // buffered Codec2 frame
    bool            m_bHasPendingCodec2;    // true if we have a buffered frame
    CIp             m_OwnerIp;
    bool            m_bHasOwner;
    bool            m_bStreamActive;        // true after first voice frame sent, false after EOT

    // Timer tracking last upstream DV-frame / last-frame received for this
    // module's M17 output path. Used by CheckForMissedEOT to detect
    // abandoned streams without touching cross-thread stream state.
    //
    // NOT "last M17 wire packet sent" — that was the previous (broken)
    // semantic which conflated "M17 emitted wire bytes" with "upstream
    // still alive." When the ambed pipeline starves (Codec2 responses
    // don't arrive in time), M17 emits no wire bytes for those packets
    // but the upstream stream is still very much alive — using wire-emit
    // as the liveness proxy then fired a spurious synthetic EOT mid-
    // transmission, tearing down the stream while listeners were still
    // expecting audio. See HandleQueue for the corrected update site.
    CTimePoint      m_LastUpstreamActivityTime;

    // Stats for on-demand transcoding (AMBE -> Codec2 for M17 listeners)
    uint32          m_uiPacketsIn;          // total DV frames received
    uint32          m_uiPacketsTranscoded;  // packets transcoded to Codec2
};

class CM17ClientCacheItem
{
public:
    CM17ClientCacheItem()     { m_bInitialized = false; }
    ~CM17ClientCacheItem()    {}

    std::vector<CIp>    m_ClientIps;
    CTimePoint          m_LastRefresh;
    std::mutex          m_Mutex;
    bool                m_bInitialized;
};

class CM17Protocol : public CProtocol
{
public:
    // constructor
    CM17Protocol() : m_nActiveStreams(0) {};

    // destructor
    virtual ~CM17Protocol() {};

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

    // client cache helpers
    void RefreshClientCache(int iModId);
    void SendToModuleClients(int iModId, const CBuffer &buffer, uint16 streamId, bool hasOwner, const CIp &ownerIp);

    // M17 EOT synthesis — sends EOT when a stream ends without a proper last frame
    void CheckForMissedEOT(void);
    void SendSyntheticEOT(int iModId);

    // keepalive helpers
    void HandleKeepalives(void);

    // stream helpers
    bool OnDvHeaderPacketIn(CDvHeaderPacket *, const CIp &);

    // packet decoding helpers
    bool IsValidConnectPacket(const CBuffer &, CCallsign *, char *, bool *);
    bool IsValidDisconnectPacket(const CBuffer &, CCallsign *);
    bool IsValidKeepAlivePacket(const CBuffer &, CCallsign *);
    bool IsValidDvHeaderPacket(const CBuffer &, CDvHeaderPacket **);
    bool IsValidDvFramePacket(const CBuffer &, CDvFramePacket **);
    bool IsValidDvLastFramePacket(const CBuffer &, CDvLastFramePacket **);

    // packet encoding helpers
    void EncodeConnectAckPacket(CBuffer *);
    void EncodeConnectNackPacket(CBuffer *);
    void EncodeDisconnectPacket(CBuffer *, char);
    void EncodeDisconnectedPacket(CBuffer *);
    void EncodePingPacket(CBuffer *);
    bool EncodeDvFramePacket(const CDvFramePacket &, uint16, CBuffer *);
    bool EncodeDvLastFramePacket(const CDvLastFramePacket &, uint16, CBuffer *);

    // M17 callsign encoding/decoding helpers
    void EncodeCallsign(const CCallsign &, uint8 *) const;
    bool DecodeCallsign(const uint8 *, CCallsign *) const;
    uint64 CallsignToBase40(const char *) const;
    void Base40ToCallsign(uint64, char *) const;
    bool IsValidM17Destination(const CCallsign &) const;

protected:
    // for keep alive
    CTimePoint          m_LastKeepaliveTime;

    // for stream cache
    std::array<CM17StreamCacheItem, NB_OF_MODULES>    m_StreamsCache;

    // for client caching - avoids repeated GetClients() lock/unlock per packet
    std::array<CM17ClientCacheItem, NB_OF_MODULES>    m_ClientCache;

    // active stream count — when 0, CheckForMissedEOT skips all mutex acquisitions
    std::atomic<int>    m_nActiveStreams;
};

////////////////////////////////////////////////////////////////////////////////////////

#endif /* cm17protocol_h */
