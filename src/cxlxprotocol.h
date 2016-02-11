//
//  cxlxprotocol.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 28/01/2016.
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

#ifndef cxlxprotocol_h
#define cxlxprotocol_h


#include "ctimepoint.h"
#include "cdextraprotocol.h"

////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////
// class

class CXlxProtocol : public CDextraProtocol
{
public:
    // constructor
    CXlxProtocol() {};
    
    // destructor
    virtual ~CXlxProtocol() {};
    
    // initialization
    bool Init(void);
    
    // task
    void Task(void);
    
protected:
    // queue helper
    void HandleQueue(void);
    
    // keepalive helper
    void HandlePeerLinks(void);
    void HandleKeepalives(void);
    
    // stream helpers
    bool OnDvHeaderPacketIn(CDvHeaderPacket *, const CIp &);
    void OnDvFramePacketIn(CDvFramePacket *);
    void OnDvLastFramePacketIn(CDvLastFramePacket *);
    
    // packet decoding helpers
    bool                IsValidKeepAlivePacket(const CBuffer &, CCallsign *);
    bool                IsValidConnectPacket(const CBuffer &, CCallsign *, char *);
    bool                IsValidDisconnectPacket(const CBuffer &, CCallsign *);
    bool                IsValidAckPacket(const CBuffer &, CCallsign *, char *);
    bool                IsValidNackPacket(const CBuffer &, CCallsign *);
    
    // packet encoding helpers
    void                EncodeKeepAlivePacket(CBuffer *);
    void                EncodeConnectPacket(CBuffer *, const char *);
    void                EncodeDisconnectPacket(CBuffer *);
    void                EncodeConnectAckPacket(CBuffer *, const char *);
    void                EncodeConnectNackPacket(CBuffer *);
    
protected:
    // time
    CTimePoint          m_LastKeepaliveTime;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cxlxprotocol_h */
