//
//  cdplusprotocol.h
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

#ifndef cdplusprotocol_h
#define cdplusprotocol_h

#include <map>
#include <mutex>
#include "ctimepoint.h"
#include "cprotocol.h"
#include "cdvheaderpacket.h"
#include "cdvframepacket.h"
#include "cdvlastframepacket.h"
#include "cdpluspeer.h"
#include "cdpluspeerclient.h"
#include "cpeercallsignlist.h"
#include <vector>

////////////////////////////////////////////////////////////////////////////////////////
// define

#define DPLUS_CLIENT_CACHE_REFRESH_INTERVAL   1.0    // seconds between cache refresh

////////////////////////////////////////////////////////////////////////////////////////
// pending ack structure for delayed connection acknowledgments

struct CDplusPendingAck
{
    CIp         m_Ip;
    CCallsign   m_Callsign;
    CTimePoint  m_CreatedTime;
    double      m_DelaySeconds;     // random delay before sending ack
    bool        m_bAccepted;        // true = send ack, false = send nack
};

////////////////////////////////////////////////////////////////////////////////////////
// class

class CDplusClient;

class CDPlusStreamCacheItem
{
public:
    CDPlusStreamCacheItem()     { m_iSeqCounter = 0; m_uiOutboundStreamId = 0; m_bHasOwner = false; }
    ~CDPlusStreamCacheItem()    {}

    std::mutex      m_Mutex;        // protects fields from RX/TX thread races
    CDvHeaderPacket m_dvHeader;
    uint8           m_iSeqCounter;
    uint16          m_uiOutboundStreamId;
    CIp             m_OwnerIp;
    bool            m_bHasOwner;
};

// Cached client info for DPlus - includes dongle status for header handling
struct CDplusCachedClient
{
    CIp     m_Ip;
    bool    m_bIsDextraDongle;
    bool    m_bHasModule;
};

// Client cache for reducing lock contention in HandleQueue
class CDplusClientCache
{
public:
    CDplusClientCache()          { m_bInitialized = false; }
    ~CDplusClientCache()         {}

    std::vector<CDplusCachedClient>  m_Clients;     // All non-master DPlus clients
    CTimePoint                       m_LastRefresh; // When cache was last refreshed
    std::mutex                       m_Mutex;       // Protects cache access
    bool                             m_bInitialized;
};

class CDplusProtocol : public CProtocol
{
public:
    // constructor
    CDplusProtocol() {};
    
    // destructor
    virtual ~CDplusProtocol() {};
    
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

    // echo detection helper
    bool IsEchoOfOutboundStream(uint16 streamId, const CIp &ip);

    void SendDvHeaderCached(CDvHeaderPacket *, const CDplusCachedClient &);

    // pending ack helpers
    void HandlePendingAcks(void);

    // keepalive helpers
    void HandleKeepalives(void);

    // REF peer helpers
    void HandleDplusPeerLinks(void);
    void HandleDplusPeerKeepalives(void);
    void HandleDplusPeerConnectionStates(void);
    bool IsRefPeerCallsign(const CCallsign &) const;
    CCallsignListItem *FindRefPeerByIp(CPeerCallsignList *, const CIp &);
    void EncodeConnectPacket(CBuffer *);
    void EncodeLoginPacket(CBuffer *, const CCallsign &, char localModule, char remoteModule);
    void EncodeLinkPacket(CBuffer *, char module);
    bool IsValidLoginAckPacket(const CBuffer &);
    bool IsValidLoginNackPacket(const CBuffer &);

    // stream helpers
    bool OnDvHeaderPacketIn(CDvHeaderPacket *, const CIp &);
    
    // packet decoding helpers
    bool                IsValidConnectPacket(const CBuffer &);
    bool                IsValidLoginPacket(const CBuffer &, CCallsign *);
    bool                IsValidDisconnectPacket(const CBuffer &);
    bool                IsValidKeepAlivePacket(const CBuffer &);
    CDvHeaderPacket     *IsValidDvHeaderPacket(const CBuffer &);
    CDvFramePacket      *IsValidDvFramePacket(const CBuffer &);
    CDvLastFramePacket  *IsValidDvLastFramePacket(const CBuffer &);
    
    // packet encoding helpers
    void                EncodeKeepAlivePacket(CBuffer *);
    void                EncodeLoginAckPacket(CBuffer *);
    void                EncodeLoginNackPacket(CBuffer *);
    void                EncodeDisconnectPacket(CBuffer *);
    bool                EncodeDvHeaderPacket(const CDvHeaderPacket &, CBuffer *) const;
    bool                EncodeDvFramePacket(const CDvFramePacket &, CBuffer *) const;
    bool                EncodeDvLastFramePacket(const CDvLastFramePacket &, CBuffer *) const;

    // peer link helpers
    void                SendPeerLinkAnnouncement(CDplusPeer *peer);

    
protected:
    // for keep alive
    CTimePoint          m_LastKeepaliveTime;

    // for REF peer handling
    CTimePoint          m_LastDplusPeerLinkTime;
    CTimePoint          m_LastDplusPeerKeepaliveTime;

    // for queue header caches
    std::array<CDPlusStreamCacheItem, NB_OF_MODULES>    m_StreamsCache;

    // periodic cleanup timer
    CTimePoint                                          m_LastCleanupCheck;

    // per-module client cache for reducing lock contention
    std::array<CDplusClientCache, NB_OF_MODULES>    m_ClientCache;

    // pending connection acks (for staggered ack timing)
    std::vector<CDplusPendingAck>   m_PendingAcks;
    std::mutex                      m_PendingAcksMutex;

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
#endif /* cdplusprotocol_h */
