//
//  cdvframepacket.cpp
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


#include "main.h"
#include <string.h>
#include "cdvframepacket.h"


////////////////////////////////////////////////////////////////////////////////////////
// constructor

CDvFramePacket::CDvFramePacket()
{
    ::memset(m_uiAmbe, 0, sizeof(m_uiAmbe));
    ::memset(m_uiDvData, 0, sizeof(m_uiDvData));
}

CDvFramePacket::CDvFramePacket(const struct dstar_dvframe *dvframe, uint16 sid, uint8 pid)
    : CPacket(sid, pid)
{
    ::memcpy(m_uiAmbe, dvframe->AMBE, sizeof(m_uiAmbe));
    ::memcpy(m_uiDvData, dvframe->DVDATA, sizeof(m_uiDvData));
}


CDvFramePacket::CDvFramePacket(const CDvFramePacket &DvFrame)
    : CPacket(DvFrame)
{
    ::memcpy(m_uiAmbe, DvFrame.m_uiAmbe, sizeof(m_uiAmbe));
    ::memcpy(m_uiDvData, DvFrame.m_uiDvData, sizeof(m_uiDvData));
}

////////////////////////////////////////////////////////////////////////////////////////
// virtual duplication

CPacket *CDvFramePacket::Duplicate(void) const
{
    return new CDvFramePacket(*this);
}


////////////////////////////////////////////////////////////////////////////////////////
// operators

bool CDvFramePacket::operator ==(const CDvFramePacket &DvFrame) const
{
    return ( (::memcmp(m_uiAmbe, DvFrame.m_uiAmbe, sizeof(m_uiAmbe)) == 0) &&
            (::memcmp(m_uiDvData, DvFrame.m_uiDvData, sizeof(m_uiDvData)) == 0) );
}
