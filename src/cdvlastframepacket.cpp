//
//  cdvlastframepacket.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 03/11/2015.
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
#include "cdvlastframepacket.h"


////////////////////////////////////////////////////////////////////////////////////////
// constructor

CDvLastFramePacket::CDvLastFramePacket()
{
}

// dstar constructor

CDvLastFramePacket::CDvLastFramePacket(const struct dstar_dvframe *DvFrame, uint16 sid, uint8 pid)
    : CDvFramePacket(DvFrame, sid, pid)
{
}

// dmr constructor

CDvLastFramePacket::CDvLastFramePacket(const uint8 *ambe, const uint8 *sync, uint16 sid, uint8 pid, uint8 spid)
    : CDvFramePacket(ambe, sync, sid, pid, spid)
{
}

// dstar + dmr constructor

CDvLastFramePacket::CDvLastFramePacket
    (uint16 sid,
     uint8 dstarpid, const uint8 *dstarambe, const uint8 *dstarcodec2, const uint8 *dstardvdata,
     uint8 dmrpid, uint8 dprspid, const uint8 *dmrambe, const uint8 *dmrsync)
    : CDvFramePacket(sid, dstarpid, dstarambe, dstarcodec2, dstardvdata, dmrpid, dprspid, dmrambe, dmrsync)
{
}

// copy constructor
    
CDvLastFramePacket::CDvLastFramePacket(const CDvLastFramePacket &DvFrame)
    : CDvFramePacket(DvFrame)
{
}

////////////////////////////////////////////////////////////////////////////////////////
// virtual duplication

CPacket *CDvLastFramePacket::Duplicate(void) const
{
    return new CDvLastFramePacket(*this);
}

