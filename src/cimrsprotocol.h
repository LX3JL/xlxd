//
//  cimrsprotocol..h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 29/10/2019.
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

#ifndef cimrsprotocol__h
#define cimrsprotocol__h
#include "ctimepoint.h"
#include "cprotocol.h"
#include "cdvheaderpacket.h"
#include "cdvframepacket.h"
#include "cdvlastframepacket.h"
//#include "ysfdefines.h"
//#include "cysffich.h"
//#include "cwiresxinfo.h"
//#include "cwiresxcmdhandler.h"

////////////////////////////////////////////////////////////////////////////////////////
// define



// IMRS Module ID
#define IMRS_MODULE_ID             'B'

////////////////////////////////////////////////////////////////////////////////////////
// class

class CImrsStreamCacheItem
{
public:
    CImrsStreamCacheItem()     {}
    ~CImrsStreamCacheItem()    {}
    
    CDvHeaderPacket m_dvHeader;
    CDvFramePacket  m_dvFrames[5];

    //uint8  m_uiSeqId;
};

class CImrsProtocol : public CProtocol
{
public:
    // constructor
    CImrsProtocol() {};
    
    // destructor
    virtual ~CImrsProtocol() {};
    
    // initialization
    bool Init(void);
    void Close(void);
    
    // task
    void Task(void);
    
protected:
    // queue helper
    void HandleQueue(void);
    
    // keepalive helpers
    void HandleKeepalives(void);
    
    // stream helpers
    bool OnDvHeaderPacketIn(CDvHeaderPacket *, const CIp &);
    
    // DV packet decoding helpers
    bool IsValidPingPacket(const CBuffer &);
    bool IsValidConnectPacket(const CBuffer &, CCallsign *, uint32 *);
    bool IsValidDvHeaderPacket(const CIp &, const CBuffer &, CDvHeaderPacket **);
    bool IsValidDvLastFramePacket(const CIp &, const CBuffer &, CDvFramePacket **);
    bool IsValidDvFramePacket(const CIp &, const CBuffer &, CDvFramePacket **);
    //bool IsValidDvLastFramePacket(const CIp &, const CYSFFICH &, const CBuffer &, CDvFramePacket **);
    
    // DV packet encoding helpers
    void EncodePingPacket(CBuffer *) const;
    void EncodePongPacket(CBuffer *) const;
    bool EncodeDvHeaderPacket(const CDvHeaderPacket &, CBuffer *) const;
    bool EncodeDvPacket(const CDvHeaderPacket &, const CDvFramePacket *, CBuffer *) const;
    bool EncodeDvLastPacket(const CDvHeaderPacket &, const CDvLastFramePacket &, CBuffer *) const;

    // uiStreamId helpers
    uint32 IpToStreamId(const CIp &) const;
    
    // DG-ID helper
    char DgidToModule(uint8 uiDgid) const;
    uint8 ModuleToDgid(char cModule) const;
        
protected:
    // for keep alive
    CTimePoint          m_LastKeepaliveTime;
    
    // for queue header caches
    std::array<CImrsStreamCacheItem, NB_OF_MODULES>    m_StreamsCache;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cimrsprotocol__h */
