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

#include "ctimepoint.h"
#include "cprotocol.h"
#include "cdvheaderpacket.h"
#include "cdvframepacket.h"
#include "cdvlastframepacket.h"

////////////////////////////////////////////////////////////////////////////////////////
// define

////////////////////////////////////////////////////////////////////////////////////////
// class

class CDcsStreamCacheItem
{
public:
    CDcsStreamCacheItem()     { m_iSeqCounter = 0; }
    ~CDcsStreamCacheItem()    {}
    
    CDvHeaderPacket m_dvHeader;
    uint32          m_iSeqCounter;
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
    
    // task
    void Task(void);
    
protected:
    // queue helper
    void HandleQueue(void);
    
    // keepalive helpers
    void HandleKeepalives(void);
    
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
    void EncodeKeepAlivePacket(CBuffer *, CClient *);
    void EncodeConnectAckPacket(const CCallsign &, char, CBuffer *);
    void EncodeConnectNackPacket(const CCallsign &, char, CBuffer *);
    void EncodeDisconnectPacket(CBuffer *, CClient *);
    void EncodeDvPacket(const CDvHeaderPacket &, const CDvFramePacket &, uint32, CBuffer *) const;
    void EncodeDvLastPacket(const CDvHeaderPacket &, const CDvFramePacket &, uint32, CBuffer *) const;
    
protected:
    // for keep alive
    CTimePoint          m_LastKeepaliveTime;
    
    // for queue header caches
    std::array<CDcsStreamCacheItem, NB_OF_MODULES>    m_StreamsCache;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cdcsprotocol_h */
