//
//  cdextraprotocol.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 01/11/2015.
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

#ifndef cdextraprotocol_h
#define cdextraprotocol_h

#include <map>
#include <mutex>
#include "ctimepoint.h"
#include "cprotocol.h"
#include "cdvheaderpacket.h"
#include "cdvframepacket.h"
#include "cdvlastframepacket.h"
#include "cdextrapeer.h"
#include "cdextrapeerclient.h"
#include "cpeercallsignlist.h"
#include <vector>

////////////////////////////////////////////////////////////////////////////////////////
// defines

#define DEXTRA_CLIENT_CACHE_REFRESH_INTERVAL   1.0    // seconds between cache refresh

////////////////////////////////////////////////////////////////////////////////////////
// pending ack structure for delayed connection acknowledgments

struct CDextraPendingAck
{
    CIp         m_Ip;
    CCallsign   m_Callsign;
    char        m_ToLinkModule;
    int         m_ProtRev;
    CTimePoint  m_CreatedTime;
    double      m_DelaySeconds;     // random delay before sending ack
    bool        m_bAccepted;        // true = send ack, false = send nack
};

////////////////////////////////////////////////////////////////////////////////////////

// note on protocol revisions:
//
//  rev 0:
//      this is standard protocol implementation
//
//  rev 1:
//      this is specific UP4DAR umplementation
//      the protocol is detected using byte(10) of connect packet (value is 11)
//      the protocol require a specific non-standard disconnect acqknowleding packet
//
//  rev 2:
//      this is specific to KI4KLF dxrfd reflector
//      the protocol is detected by looking at "XRF" in connect packet callsign
//      the protocol require a specific connect ack packet
//      the protocol also implement a workaround for detecting stream's module
//          as dxrfd soes not set DV header RPT2 properly.
//      the protocol assumes that a dxrfd can only be linked to one module at a time


////////////////////////////////////////////////////////////////////////////////////////
// class

class CDextraStreamCacheItem
{
public:
    CDextraStreamCacheItem()     { m_uiOutboundStreamId = 0; m_bHasOwner = false; }
    ~CDextraStreamCacheItem()    {}

    std::mutex      m_Mutex;        // protects fields from RX/TX thread races
    CDvHeaderPacket m_dvHeader;
    uint16          m_uiOutboundStreamId;
    CIp             m_OwnerIp;
    bool            m_bHasOwner;
};

// Client cache item for reducing lock contention in HandleQueue
class CDextraClientCacheItem
{
public:
    CDextraClientCacheItem()     { m_bInitialized = false; }
    ~CDextraClientCacheItem()    {}

    std::vector<CIp>    m_ClientIps;        // IPs of non-master clients on this module
    CTimePoint          m_LastRefresh;      // When cache was last refreshed
    std::mutex          m_Mutex;            // Protects cache access from race conditions
    bool                m_bInitialized;     // Track if cache has ever been populated
};

class CDextraProtocol : public CProtocol
{
public:
    // constructor
    CDextraProtocol() {};
    
    // destructor
    virtual ~CDextraProtocol() {};
    
    // initialization
    bool Init(void);
    
    // split-thread mode
    bool UsesSplitThreads(void) const { return true; }
    void RxTask(void);
    void TxTask(void);

protected:
    // queue helper
    void HandleQueue(void);
    void RefreshClientCache(int iModId);
    void SendToModuleClients(int iModId, const CBuffer &buffer, int repeatCount, bool hasOwner, const CIp &ownerIp);

    // pending ack helpers
    void HandlePendingAcks(void);

    // keepalive helpers
    void HandleKeepalives(void);

    // XRF peer helpers
    void HandleDextraPeerLinks(void);
    void HandleDextraPeerKeepalives(void);
    void HandleDextraPeerConnectionStates(void);
    bool IsXrfPeerCallsign(const CCallsign &) const;
    void EncodeConnectPacket(CBuffer *, const CCallsign &, char, char);
    bool IsValidConnectAckPacket(const CBuffer &);

    // echo detection helper
    bool IsEchoOfOutboundStream(uint16 streamId, const CIp &ip);

    // stream helpers
    bool OnDvHeaderPacketIn(CDvHeaderPacket *, const CIp &);

    // packet decoding helpers
    bool                IsValidConnectPacket(const CBuffer &, CCallsign *, char *, int *);
    bool                IsValidDisconnectPacket(const CBuffer &, CCallsign *);
    bool                IsValidKeepAlivePacket(const CBuffer &, CCallsign *);
    CDvHeaderPacket     *IsValidDvHeaderPacket(const CBuffer &);
    CDvFramePacket      *IsValidDvFramePacket(const CBuffer &);
    CDvLastFramePacket  *IsValidDvLastFramePacket(const CBuffer &);

    // packet encoding helpers
    void                EncodeKeepAlivePacket(CBuffer *);
    void                EncodeConnectAckPacket(CBuffer *, int);
    void                EncodeConnectNackPacket(CBuffer *);
    void                EncodeDisconnectPacket(CBuffer *);
    void                EncodeDisconnectedPacket(CBuffer *);
    bool                EncodeDvHeaderPacket(const CDvHeaderPacket &, CBuffer *) const;
    bool                EncodeDvFramePacket(const CDvFramePacket &, CBuffer *) const;
    bool                EncodeDvLastFramePacket(const CDvLastFramePacket &, CBuffer *) const;

protected:
    // time
    CTimePoint          m_LastKeepaliveTime;

    // for XRF peer handling
    CTimePoint          m_LastDextraPeerLinkTime;
    CTimePoint          m_LastDextraPeerKeepaliveTime;

    // for queue header caches
    std::array<CDextraStreamCacheItem, NB_OF_MODULES>    m_StreamsCache;

    // periodic cleanup timer
    CTimePoint                                           m_LastCleanupCheck;

    // client cache for reducing lock contention
    std::array<CDextraClientCacheItem, NB_OF_MODULES>    m_ClientCache;

    // pending connection acks (for staggered ack timing)
    std::vector<CDextraPendingAck>   m_PendingAcks;
    std::mutex                       m_PendingAcksMutex;

    // progressive ignore for persistent invalid connect attempts
    struct CRejectTracker
    {
        int         m_nCount;
        int         m_nStrike;
        double      m_dIgnoreDuration;
        CTimePoint  m_IgnoreStart;
        CRejectTracker() : m_nCount(0), m_nStrike(0), m_dIgnoreDuration(0.0) {}
    };
    std::map<uint32, CRejectTracker>  m_RejectTrackers;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cdextraprotocol_h */
