//
//  cysfprotocol.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 20/05/2018.
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


#ifndef cysfprotocol_h
#define cysfprotocol_h


#include "ctimepoint.h"
#include "cprotocol.h"
#include "cdvheaderpacket.h"
#include "cdvframepacket.h"
#include "cdvlastframepacket.h"
#include "ysfdefines.h"
#include "cysffich.h"
#include "cwiresxinfo.h"
#include "cwiresxcmdhandler.h"

////////////////////////////////////////////////////////////////////////////////////////
// define


// Wires-X commands
#define WIRESX_CMD_UNKNOWN          0
#define WIRESX_CMD_DX_REQ           1
#define WIRESX_CMD_ALL_REQ          2
#define WIRESX_CMD_SEARCH_REQ       3
#define WIRESX_CMD_CONN_REQ         4
#define WIRESX_CMD_DISC_REQ         5

// YSF Module ID
#define YSF_MODULE_ID             'Y'

////////////////////////////////////////////////////////////////////////////////////////
// class

class CYsfStreamCacheItem
{
public:
    CYsfStreamCacheItem()     {}
    ~CYsfStreamCacheItem()    {}
    
    CDvHeaderPacket m_dvHeader;
    CDvFramePacket  m_dvFrames[5];

    //uint8  m_uiSeqId;
};

class CYsfProtocol : public CProtocol
{
public:
    // constructor
    CYsfProtocol();
    
    // destructor
    virtual ~CYsfProtocol() {};
    
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
    bool IsValidConnectPacket(const CBuffer &, CCallsign *);
    //bool IsValidDisconnectPacket(const CBuffer &, CCallsign *);
    bool IsValidDvPacket(const CBuffer &, CYSFFICH *);
    bool IsValidDvHeaderPacket(const CIp &, const CYSFFICH &, const CBuffer &, CDvHeaderPacket **, CDvFramePacket **);
    bool IsValidDvFramePacket(const CIp &, const CYSFFICH &, const CBuffer &, CDvFramePacket **);
    bool IsValidDvLastFramePacket(const CIp &, const CYSFFICH &, const CBuffer &, CDvFramePacket **);
    
    // DV packet encoding helpers
    void EncodeConnectAckPacket(CBuffer *) const;
    //void EncodeConnectNackPacket(const CCallsign &, char, CBuffer *);
    //void EncodeDisconnectPacket(CBuffer *, CClient *);
    bool EncodeDvHeaderPacket(const CDvHeaderPacket &, CBuffer *) const;
    bool EncodeDvPacket(const CDvHeaderPacket &, const CDvFramePacket *, CBuffer *) const;
    bool EncodeDvLastPacket(const CDvHeaderPacket &, CBuffer *) const;

    // Wires-X packet decoding helpers
    bool IsValidwirexPacket(const CBuffer &, CYSFFICH *, CCallsign *, int *, int*);
    
    // server status packet decoding helpers
    bool IsValidServerStatusPacket(const CBuffer &) const;
    uint32 CalcHash(const uint8 *, int) const;
    
    // server status packet encoding helpers
    bool EncodeServerStatusPacket(CBuffer *) const;
    
    // uiStreamId helpers
    uint32 IpToStreamId(const CIp &) const;
    
    // debug
    bool DebugTestDecodePacket(const CBuffer &);
    bool DebugDumpHeaderPacket(const CBuffer &);
    bool DebugDumpDvPacket(const CBuffer &);
    bool DebugDumpLastDvPacket(const CBuffer &);
    
protected:
    // for keep alive
    CTimePoint          m_LastKeepaliveTime;
    
    // for queue header caches
    std::array<CYsfStreamCacheItem, NB_OF_MODULES>    m_StreamsCache;
    
    // for wires-x
    CWiresxCmdHandler   m_WiresxCmdHandler;
    unsigned char m_seqNo;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cysfprotocol_h */
