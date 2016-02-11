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
    m_uiPacketId = 0;
    m_uiModuleId = ' ';
    m_uiOriginId = ORIGIN_LOCAL;
};

CPacket::CPacket(uint16 sid, uint8 pid)
{
    m_uiStreamId = sid;
    m_uiPacketId = pid;
    m_uiModuleId = ' ';
    m_uiOriginId = ORIGIN_LOCAL;
};

////////////////////////////////////////////////////////////////////////////////////////
// virtual duplication

CPacket *CPacket::Duplicate(void) const
{
    return new CPacket(*this);
}
