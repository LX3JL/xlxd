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

#include "ctimepoint.h"
#include "cprotocol.h"
#include "cdvheaderpacket.h"
#include "cdvframepacket.h"
#include "cdvlastframepacket.h"

////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////
// class

class CDextraProtocol : public CProtocol
{
public:
    // constructor
    CDextraProtocol() {};
    
    // destructor
    virtual ~CDextraProtocol() {};
    
    // initialization
    bool Init(void);
    
    // task
    void Task(void);

protected:
    // queue helper
    void HandleQueue(void);
    
    // stream helpers
    bool OnDvHeaderPacketIn(CDvHeaderPacket *, const CIp &);
    
    // packet decoding helpers
    bool                IsValidConnectPacket(const CBuffer &, CCallsign *, char *);
    bool                IsValidDisconnectPacket(const CBuffer &, CCallsign *);
    bool                IsValidKeepAlivePacket(const CBuffer &, CCallsign *);
    CDvHeaderPacket     *IsValidDvHeaderPacket(const CBuffer &);
    CDvFramePacket      *IsValidDvFramePacket(const CBuffer &);
    CDvLastFramePacket  *IsValidDvLastFramePacket(const CBuffer &);
    
    // packet encoding helpers
    void                EncodeKeepAlivePacket(CBuffer *);
    void                EncodeConnectAckPacket(CBuffer *);
    void                EncodeConnectNackPacket(CBuffer *);
    void                EncodeDisconnectPacket(CBuffer *);
    bool                EncodeDvHeaderPacket(const CDvHeaderPacket &, CBuffer *) const;
    bool                EncodeDvFramePacket(const CDvFramePacket &, CBuffer *) const;
    bool                EncodeDvLastFramePacket(const CDvLastFramePacket &, CBuffer *) const;
    
protected:
    // time
    CTimePoint          m_LastKeepaliveTime;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cdextraprotocol_h */
