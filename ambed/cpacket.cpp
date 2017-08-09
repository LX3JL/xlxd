//
//  cpacket.cpp
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
#include "cpacket.h"


////////////////////////////////////////////////////////////////////////////////////////
// constructors

CPacket::CPacket()
{
    m_iCh = 0;
    m_uiPid = 0;
};

CPacket::CPacket(uint8 uiPid)
{
    m_iCh = 0;
    m_uiPid = uiPid;
}

CPacket::CPacket(const CPacket &packet)
{
    m_iCh = packet.m_iCh;
    m_uiPid = packet.m_uiPid;
};

