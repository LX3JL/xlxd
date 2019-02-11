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

#include "cversion.h"
#include "ctimepoint.h"
#include "cdextraprotocol.h"
#include "cclients.h"

////////////////////////////////////////////////////////////////////////////////////////

class CPeer;

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
    
    // identity
    int GetProtocol(void) const { return PROTOCOL_XLX; }

protected:
    // queue helper
    void HandleQueue(void);
    
    // keepalive helper
    void HandlePeerLinks(void);
    void HandleKeepalives(void);
    
    // stream helpers
    bool OnDvHeaderPacketIn(CDvHeaderPacket *, const CIp &);
    void OnDvFramePacketIn(CDvFramePacket *, const CIp * = NULL);
    void OnDvLastFramePacketIn(CDvLastFramePacket *, const CIp * = NULL);
    
    // packet decoding helpers
    bool IsValidKeepAlivePacket(const CBuffer &, CCallsign *);
    bool IsValidConnectPacket(const CBuffer &, CCallsign *, char *, CVersion *);
    bool IsValidDisconnectPacket(const CBuffer &, CCallsign *);
    bool IsValidAckPacket(const CBuffer &, CCallsign *, char *, CVersion *);
    bool IsValidNackPacket(const CBuffer &, CCallsign *);
    CDvFramePacket      *IsValidDvFramePacket(const CBuffer &);
    CDvLastFramePacket  *IsValidDvLastFramePacket(const CBuffer &);
    
    // packet encoding helpers
    void EncodeKeepAlivePacket(CBuffer *);
    void EncodeConnectPacket(CBuffer *, const char *);
    void EncodeDisconnectPacket(CBuffer *);
    void EncodeConnectAckPacket(CBuffer *, const char *);
    void EncodeConnectNackPacket(CBuffer *);
    bool EncodeDvFramePacket(const CDvFramePacket &, CBuffer *) const;
    bool EncodeDvLastFramePacket(const CDvLastFramePacket &, CBuffer *) const;
    
    // protocol revision helper
    int GetConnectingPeerProtocolRevision(const CCallsign &, const CVersion &);
    CPeer *CreateNewPeer(const CCallsign &, const CIp &, char *, const CVersion &);
    
protected:
    // time
    CTimePoint          m_LastKeepaliveTime;
    CTimePoint          m_LastPeersLinkTime;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cxlxprotocol_h */
