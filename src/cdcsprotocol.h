//
//  cdcsprotocol.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 07/11/2015.
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

#ifndef cdcsprotocol_h
#define cdcsprotocol_h

#include <map>
#include <cstring>
#include "ctimepoint.h"
#include "cprotocol.h"
#include "cdvheaderpacket.h"
#include "cdvframepacket.h"
#include "cdvlastframepacket.h"
#include "cdcspeer.h"
#include "cdcspeerclient.h"
#include "cpeercallsignlist.h"
#include <vector>
#include <mutex>

////////////////////////////////////////////////////////////////////////////////////////
// define

// Client cache refresh interval in seconds - balances freshness vs lock overhead
#define DCS_CLIENT_CACHE_REFRESH_INTERVAL   1.0

////////////////////////////////////////////////////////////////////////////////////////
// pending ack structure for delayed connection acknowledgments

struct CDcsPendingAck
{
    CIp         m_Ip;
    CCallsign   m_Callsign;
    char        m_ToLinkModule;
    CTimePoint  m_CreatedTime;
    double      m_DelaySeconds;     // random delay before sending ack
    bool        m_bAccepted;        // true = send ack, false = send nack
};

////////////////////////////////////////////////////////////////////////////////////////
// class

class CDcsStreamCacheItem
{
public:
    CDcsStreamCacheItem()
    {
        m_iSeqCounter = 0;
        m_uiOutboundStreamId = 0;
        m_bHasOwner = false;
        m_bHasDcsTail = false;
        ::memset(m_uiDcsTail, 0, sizeof(m_uiDcsTail));
        m_uiDcsTail[3] = 0x01;     // DCS format marker
        m_uiDcsTail[5] = 0x21;     // DCS text field separator
    }
    ~CDcsStreamCacheItem()    {}

    std::mutex      m_Mutex;        // protects all fields from RX/TX thread races
    CDvHeaderPacket m_dvHeader;
    uint32          m_iSeqCounter;
    uint16          m_uiOutboundStreamId;
    CIp             m_OwnerIp;      // stream owner's IP — send-time exclusion skips packets to owner
    bool            m_bHasOwner;
    bool            m_bHasDcsTail;  // true only when tail captured from a DCS source
    uint8           m_uiDcsTail[42]; // DCS tail (bytes 58-99: seq counter + marker + text field)
};

// Client cache item - stores IPs of DCS clients linked to a module
class CDcsClientCacheItem
{
public:
    CDcsClientCacheItem()     { m_bInitialized = false; }
    ~CDcsClientCacheItem()    {}

    std::vector<CIp>    m_ClientIps;        // IPs of non-master clients on this module
    CTimePoint          m_LastRefresh;      // When cache was last refreshed
    std::mutex          m_Mutex;            // Protects cache access from race conditions
    bool                m_bInitialized;     // Track if cache has ever been populated
};

class CDcsProtocol : public CProtocol
{
public:
    // constructor
    CDcsProtocol() {};
    
    // destructor
    virtual ~CDcsProtocol() {};
    
    // initialization
    bool Init(void);
    
    // split-thread mode
    bool UsesSplitThreads(void) const { return true; }
    void RxTask(void);
    void TxTask(void);
    
protected:
    // queue helper
    void HandleQueue(void);

    // client cache helpers
    void RefreshClientCache(int iModId);
    void SendToModuleClients(int iModId, const CBuffer &buffer, bool hasOwner, const CIp &ownerIp);

    // pending ack helpers
    void HandlePendingAcks(void);

    // keepalive helpers
    void HandleKeepalives(void);

    // DCS peer helpers
    void HandleDcsPeerLinks(void);
    void HandleDcsPeerKeepalives(void);
    void HandleDcsPeerConnectionStates(void);
    bool IsDcsPeerCallsign(const CCallsign &) const;
    void EncodePeerConnectPacket(CBuffer *, const CCallsign &, char, char);
    bool IsValidPeerConnectAckPacket(const CBuffer &);

    // stream helpers
    bool OnDvHeaderPacketIn(CDvHeaderPacket *, const CIp &);
    
    // packet decoding helpers
    bool IsValidConnectPacket(const CBuffer &, CCallsign *, char *);
    bool IsValidDisconnectPacket(const CBuffer &, CCallsign *);
    bool IsValidKeepAlivePacket(const CBuffer &, CCallsign *);
    bool IsValidDvPacket(const CBuffer &, CDvHeaderPacket **, CDvFramePacket **);
    bool IsIgnorePacket(const CBuffer &);
    
    // packet encoding helpers
    void EncodeKeepAlivePacket(CBuffer *);
    void EncodePeerKeepAlivePacket(CBuffer *, CDcsPeer *);
    void EncodeKeepAlivePacket(CBuffer *, CClient *);
    void EncodeConnectAckPacket(const CCallsign &, char, CBuffer *);
    void EncodeConnectNackPacket(const CCallsign &, char, CBuffer *);
    void EncodeDisconnectPacket(CBuffer *, CClient *);
    void EncodeDvPacket(const CDvHeaderPacket &, const CDvFramePacket &, uint32, CBuffer *) const;
    void EncodeDvLastPacket(const CDvHeaderPacket &, const CDvFramePacket &, uint32, CBuffer *) const;
    
protected:
    // for keep alive
    CTimePoint          m_LastKeepaliveTime;

    // for DCS peer handling
    CTimePoint          m_LastDcsPeerLinkTime;
    CTimePoint          m_LastDcsPeerKeepaliveTime;

    // for queue header caches
    std::array<CDcsStreamCacheItem, NB_OF_MODULES>    m_StreamsCache;

    // periodic cleanup timer
    CTimePoint                                        m_LastCleanupCheck;

    // for client caching - avoids repeated GetClients() lock/unlock per packet
    std::array<CDcsClientCacheItem, NB_OF_MODULES>    m_ClientCache;

    // pending connection acks (for staggered ack timing)
    std::vector<CDcsPendingAck>   m_PendingAcks;
    std::mutex                    m_PendingAcksMutex;

    // progressive ignore for persistent invalid connect attempts
    struct CRejectTracker
    {
        int         m_nCount;           // attempts since last escalation
        int         m_nStrike;          // escalation level (0=log+nack, 1=5min, 2=30min, 3+=1hr)
        double      m_dIgnoreDuration;  // seconds to ignore
        CTimePoint  m_IgnoreStart;      // when ignore period began
        CRejectTracker() : m_nCount(0), m_nStrike(0), m_dIgnoreDuration(0.0) {}
    };
    std::map<uint32, CRejectTracker>  m_RejectTrackers;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cdcsprotocol_h */
