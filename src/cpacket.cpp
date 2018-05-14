//
//  cpacket.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 04/11/2015.
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

#include "main.h"
#include "cpacket.h"



////////////////////////////////////////////////////////////////////////////////////////
// constructor

CPacket::CPacket()
{
    m_uiStreamId = 0;
    m_uiDstarPacketId = 0;
    m_uiDmrPacketId = 0;
    m_uiModuleId = ' ';
    m_uiOriginId = ORIGIN_LOCAL;
};

CPacket::CPacket(uint16 sid, uint8 dstarpid)
{
    m_uiStreamId = sid;
    m_uiDstarPacketId = dstarpid;
    m_uiDmrPacketId = 0xFF;
    m_uiDmrPacketSubid  = 0xFF;
    m_uiModuleId = ' ';
    m_uiOriginId = ORIGIN_LOCAL;
};

CPacket::CPacket(uint16 sid, uint8 dmrpid, uint8 dmrspid)
{
    m_uiStreamId = sid;
    m_uiDmrPacketId = dmrpid;
    m_uiDmrPacketSubid = dmrspid;
    m_uiDstarPacketId = 0xFF;
    m_uiModuleId = ' ';
    m_uiOriginId = ORIGIN_LOCAL;
};

CPacket::CPacket(uint16 sid, uint8 dstarpid, uint8 dmrpid, uint8 dmrsubpid)
{
    m_uiStreamId = sid;
    m_uiDstarPacketId = dstarpid;
    m_uiDmrPacketId = dmrpid;
    m_uiDmrPacketSubid  = dmrsubpid;
    m_uiModuleId = ' ';
    m_uiOriginId = ORIGIN_LOCAL;
}

////////////////////////////////////////////////////////////////////////////////////////
// virtual duplication

CPacket *CPacket::Duplicate(void) const
{
    return new CPacket(*this);
}


////////////////////////////////////////////////////////////////////////////////////////
// pid conversion

void CPacket::UpdatePids(uint32 pid)
{
    // called while phusing this packet in a stream queue
    // so now packet sequence number is known and undefined pids can be updated
    // this is needed as dtsar & dmt pids are different and cannot be
    // derived from each other
    
    // dstar pid needs update ?
    if (  m_uiDstarPacketId ==  0xFF )
    {
        m_uiDstarPacketId = (pid % 21);
    }
    // dmr pids need update ?
    if ( m_uiDmrPacketId == 0xFF )
    {
        m_uiDmrPacketId = ((pid / 3) % 6);
        m_uiDmrPacketSubid = ((pid % 3) + 1);
    }
}
