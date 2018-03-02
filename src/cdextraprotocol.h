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

    // keepalive helpers
    void HandlePeerLinks(void);
    void HandleKeepalives(void);

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
    void                EncodeConnectPacket(CBuffer *, const char *);
    void                EncodeConnectAckPacket(CBuffer *, int);
    void                EncodeConnectNackPacket(CBuffer *);
    void                EncodeDisconnectPacket(CBuffer *, char);
    void                EncodeDisconnectedPacket(CBuffer *);
    bool                EncodeDvHeaderPacket(const CDvHeaderPacket &, CBuffer *) const;
    bool                EncodeDvFramePacket(const CDvFramePacket &, CBuffer *) const;
    bool                EncodeDvLastFramePacket(const CDvLastFramePacket &, CBuffer *) const;
    
protected:
    // time
    CTimePoint          m_LastKeepaliveTime;
    CTimePoint          m_LastPeersLinkTime;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cdextraprotocol_h */
