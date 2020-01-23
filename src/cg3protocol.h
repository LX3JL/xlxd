//
//  cg3protocol.h
//  xlxd
//
//  Created by Marius Petrescu (YO2LOJ) on 03/06/2019.
//  Copyright © 2019 Marius Petrescu (YO2LOJ). All rights reserved.
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

#ifndef cg3protocol_h
#define cg3protocol_h

#include <string>
#include "ctimepoint.h"
#include "cprotocol.h"
#include "cdvheaderpacket.h"
#include "cdvframepacket.h"
#include "cdvlastframepacket.h"

////////////////////////////////////////////////////////////////////////////////////////

// note on the G3 terminal/AP protocol:
//
// There are 3 steps in handling an incoming connection
//
// 1 - Notification of terminal call on port UDP 12346
//      - Call will be rejected if in blacklisted
//
// 2 - Destination request on port UDP 12345
//      - Calls to specific callsigns will be accepted for a default module
//      - Repeater calls will be accepted for local modules
//      - All other calls are rehected
//
// 3 - Actual D-star flow like in Dextra to/from port 40000 (2 distint sockets)
//
// Alive monitoring is done via a "PING" to remote port 40000. We will get an
// ICMP unreachable on terminal mode close or on station shut down if routing is done
// correctly. Otherwise a long timeout is used (e.g. 1h)


////////////////////////////////////////////////////////////////////////////////////////
// class

class CG3Protocol : public CProtocol
{
public:
    // constructor
    CG3Protocol() : m_GwAddress(0u), m_Modules("*"), m_LastModTime(0) {};
    
    // destructor
    virtual ~CG3Protocol() {};
    
    // initialization
    bool Init(void);

    // close
    void Close(void);
    
    // task
    void Task(void);

protected:
    // threads
    static void PresenceThread(CG3Protocol *);
    static void ConfigThread(CG3Protocol *);
    static void IcmpThread(CG3Protocol *);

    // helper tasks
    void PresenceTask(void);
    void ConfigTask(void);
    void IcmpTask(void);

    // config
    void ReadOptions(void);

    // helper
    char *TrimWhiteSpaces(char *);
    void NeedReload(void);

    // queue helper
    void HandleQueue(void);

    // keepalive helpers
    void HandleKeepalives(void);

    // stream helpers
    bool OnDvHeaderPacketIn(CDvHeaderPacket *, const CIp &);

    // packet decoding helpers
    CDvHeaderPacket     *IsValidDvHeaderPacket(const CBuffer &);
    CDvFramePacket      *IsValidDvFramePacket(const CBuffer &);
    CDvLastFramePacket  *IsValidDvLastFramePacket(const CBuffer &);

    // packet encoding helpers
    bool                EncodeDvHeaderPacket(const CDvHeaderPacket &, CBuffer *) const;
    bool                EncodeDvFramePacket(const CDvFramePacket &, CBuffer *) const;
    bool                EncodeDvLastFramePacket(const CDvLastFramePacket &, CBuffer *) const;

protected:
    std::thread         *m_pPresenceThread;
    std::thread         *m_pConfigThread;
    std::thread         *m_pIcmpThread;

    // time
    CTimePoint          m_LastKeepaliveTime;

    // sockets
    CUdpSocket          m_DvOutSocket;
    CUdpSocket          m_PresenceSocket;
    CUdpSocket          m_ConfigSocket;
    int                 m_IcmpRawSocket;

    // optional params
    uint32              m_GwAddress;
    std::string         m_Modules;
    time_t              m_LastModTime;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cg3protocol_h */
