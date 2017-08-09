//
//  cambepacket.cpp
//  ambed
//
//  cpacketqueue.cpp
//  ambed
//
//  Created by Jean-Luc Deltombe (LX3JL) on 28/04/2017.
//  Copyright © 2015 Jean-Luc Deltombe (LX3JL). All rights reserved.
//
// ----------------------------------------------------------------------------
//    This file is part of ambed.
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
#include <string.h>
#include "cambepacket.h"


////////////////////////////////////////////////////////////////////////////////////////
// constructor

CAmbePacket::CAmbePacket()
{
    m_uiCodec = CODEC_NONE;
    ::memset(m_uiAmbe, 0, AMBE_SIZE);
}

CAmbePacket::CAmbePacket(uint8 uiPid, uint8 uiCodec, const uint8 *Ambe)
    : CPacket(uiPid)
{
    m_uiCodec = uiCodec;
    ::memcpy(m_uiAmbe, Ambe, AMBE_SIZE);
}

CAmbePacket::CAmbePacket(const CAmbePacket &packet)
    : CPacket(packet)
{
    m_uiCodec = packet.m_uiCodec;
    ::memcpy(m_uiAmbe, packet.m_uiAmbe, sizeof(m_uiAmbe));
}


////////////////////////////////////////////////////////////////////////////////////////
// destructor

CAmbePacket::~CAmbePacket()
{
}

////////////////////////////////////////////////////////////////////////////////////////
// Set

void CAmbePacket::SetAmbe(const uint8 *p)
{
    ::memcpy(m_uiAmbe, p, AMBE_SIZE);
}

