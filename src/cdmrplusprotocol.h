//
//  cdmrplusprotocol.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 10/01/2016.
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

#ifndef cdmrplusprotocol_h
#define cdmrplusprotocol_h

#include "ctimepoint.h"
#include "cprotocol.h"
#include "cdvheaderpacket.h"
#include "cdvframepacket.h"
#include "cdvlastframepacket.h"

////////////////////////////////////////////////////////////////////////////////////////
// define

// DMR Plus Module ID
#define DMRPLUS_MODULE_ID       'B'

////////////////////////////////////////////////////////////////////////////////////////
// class

class CDmrplusStreamCacheItem
{
public:
    CDmrplusStreamCacheItem()     { m_uiSeqId = 0x77; }
    ~CDmrplusStreamCacheItem()    {}
    
    CDvHeaderPacket m_dvHeader;
    CDvFramePacket  m_dvFrame0;
    CDvFramePacket  m_dvFrame1;
    
    uint8   m_uiSeqId;
};


class CDmrplusProtocol : public CProtocol
{
public:
    // constructor
    CDmrplusProtocol() {};
    
    // destructor
    virtual ~CDmrplusProtocol() {};
    
    // initialization
    bool Init(void);
    
    // task
    void Task(void);
    
protected:
    // queue helper
    void HandleQueue(void);
    void SendBufferToClients(const CBuffer &, uint8);
    
    // keepalive helpers
    void HandleKeepalives(void);
    
    // stream helpers
    bool OnDvHeaderPacketIn(CDvHeaderPacket *, const CIp &);
    
    // packet decoding helpers
    bool IsValidConnectPacket(const CBuffer &, CCallsign *, char *, const CIp &);
    bool IsValidDisconnectPacket(const CBuffer &, CCallsign *, char *);
    bool IsValidDvHeaderPacket(const CIp &, const CBuffer &, CDvHeaderPacket **);
    bool IsValidDvFramePacket(const CIp &, const CBuffer &, CDvFramePacket **);
    
    // packet encoding helpers
    void EncodeConnectAckPacket(CBuffer *);
    void EncodeConnectNackPacket(CBuffer *);
    bool EncodeDvHeaderPacket(const CDvHeaderPacket &, CBuffer *) const;
    void EncodeDvPacket(const CDvHeaderPacket &, const CDvFramePacket &, const CDvFramePacket &, const CDvFramePacket &, uint8, CBuffer *) const;
    void EncodeDvLastPacket(const CDvHeaderPacket &, const CDvFramePacket &, const CDvFramePacket &, const CDvFramePacket &, uint8, CBuffer *) const;
    void SwapEndianess(uint8 *, int) const;
    
    // dmr SeqId helper
    uint8 GetNextSeqId(uint8) const;
    
    // dmr DstId to Module helper
    char DmrDstIdToModule(uint32) const;
    uint32 ModuleToDmrDestId(char) const;
    
    // uiStreamId helpers
    uint32 IpToStreamId(const CIp &) const;
    
    // Buffer & LC helpers
    void AppendVoiceLCToBuffer(CBuffer *, uint32) const;
    void AppendTerminatorLCToBuffer(CBuffer *, uint32) const;
    void ReplaceEMBInBuffer(CBuffer *, uint8) const;

    
protected:
    // for keep alive
    CTimePoint          m_LastKeepaliveTime;
    
    // for queue header caches
    std::array<CDmrplusStreamCacheItem, NB_OF_MODULES>    m_StreamsCache;
};

////////////////////////////////////////////////////////////////////////////////////////


#endif /* cdmrplusprotocol_h */
