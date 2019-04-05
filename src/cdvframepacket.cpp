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
    ::memset(m_uiCodec2, 0, sizeof(m_uiCodec2));
    ::memset(m_uiDvData, 0, sizeof(m_uiDvData));
    ::memset(m_uiAmbePlus, 0, sizeof(m_uiAmbePlus));
    ::memset(m_uiDvSync, 0, sizeof(m_uiDvSync));
};

// dstar constructor

CDvFramePacket::CDvFramePacket(const struct dstar_dvframe *dvframe, uint16 sid, uint8 pid)
    : CPacket(sid, pid)
{
    ::memcpy(m_uiAmbe, dvframe->AMBE, sizeof(m_uiAmbe));
    ::memset(m_uiCodec2, 0, sizeof(m_uiCodec2));
    ::memcpy(m_uiDvData, dvframe->DVDATA, sizeof(m_uiDvData));
    ::memset(m_uiAmbePlus, 0, sizeof(m_uiAmbePlus));
    ::memset(m_uiDvSync, 0, sizeof(m_uiDvSync));
}

// dmr constructor

CDvFramePacket::CDvFramePacket(const uint8 *ambe, const uint8 *sync, uint16 sid, uint8 pid, uint8 spid)
    : CPacket(sid, pid, spid)
{
    ::memcpy(m_uiAmbePlus, ambe, sizeof(m_uiAmbePlus));
    ::memcpy(m_uiDvSync, sync, sizeof(m_uiDvSync));
    ::memset(m_uiAmbe, 0, sizeof(m_uiAmbe));
    ::memset(m_uiCodec2, 0, sizeof(m_uiCodec2));
    ::memset(m_uiDvData, 0, sizeof(m_uiDvData));
}

// dstar + codec 2 + dmr constructor

CDvFramePacket::CDvFramePacket
    (uint16 sid,
     uint8 dstarpid, const uint8 *dstarambe, const uint8 *dstarcodec2, const uint8 *dstardvdata,
     uint8 dmrpid, uint8 dprspid, const uint8 *dmrambe, const uint8 *dmrsync)
: CPacket(sid, dstarpid, dmrpid, dprspid)
{
    ::memcpy(m_uiAmbe, dstarambe, sizeof(m_uiAmbe));
    ::memcpy(m_uiCodec2, dstarcodec2, sizeof(m_uiCodec2));
    ::memcpy(m_uiDvData, dstardvdata, sizeof(m_uiDvData));
    ::memcpy(m_uiAmbePlus, dmrambe, sizeof(m_uiAmbePlus));
    ::memcpy(m_uiDvSync, dmrsync, sizeof(m_uiDvSync));
}

// copy constructor

CDvFramePacket::CDvFramePacket(const CDvFramePacket &DvFrame)
    : CPacket(DvFrame)
{
    ::memcpy(m_uiAmbe, DvFrame.m_uiAmbe, sizeof(m_uiAmbe));
    ::memcpy(m_uiCodec2, DvFrame.m_uiCodec2, sizeof(m_uiCodec2));
    ::memcpy(m_uiDvData, DvFrame.m_uiDvData, sizeof(m_uiDvData));
    ::memcpy(m_uiAmbePlus, DvFrame.m_uiAmbePlus, sizeof(m_uiAmbePlus));
    ::memcpy(m_uiDvSync, DvFrame.m_uiDvSync, sizeof(m_uiDvSync));
}

////////////////////////////////////////////////////////////////////////////////////////
// virtual duplication

CPacket *CDvFramePacket::Duplicate(void) const
{
    return new CDvFramePacket(*this);
}

////////////////////////////////////////////////////////////////////////////////////////
// get

const uint8 *CDvFramePacket::GetAmbe(uint8 uiCodec) const
{
    switch (uiCodec)
    {
        case CODEC_AMBEPLUS:    return m_uiAmbe;
        case CODEC_AMBE2PLUS:   return m_uiAmbePlus;
        case CODEC_CODEC2_3200:
        case CODEC_CODEC2_2400: return m_uiCodec2;
        default:                return NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// set

void CDvFramePacket::SetDvData(uint8 *DvData)
{
    ::memcpy(m_uiDvData, DvData, sizeof(m_uiDvData));
}

void CDvFramePacket::SetAmbe(uint8 uiCodec, uint8 *Ambe)
{
    switch (uiCodec)
    {
        case CODEC_AMBEPLUS:
            ::memcpy(m_uiAmbe, Ambe, sizeof(m_uiAmbe));
            break;
        case CODEC_AMBE2PLUS:
            ::memcpy(m_uiAmbePlus, Ambe, sizeof(m_uiAmbePlus));
            break;
        case CODEC_CODEC2_3200:
        case CODEC_CODEC2_2400:
            ::memcpy(m_uiCodec2, Ambe, sizeof(m_uiCodec2));
            break;
    }
}

void CDvFramePacket::ClearAmbe(uint8 uiCodec)
{
    switch (uiCodec)
    {
        case CODEC_AMBEPLUS:
            ::memset(m_uiAmbe, 0, sizeof(m_uiAmbe));
            break;
        case CODEC_AMBE2PLUS:
            ::memset(m_uiAmbePlus, 0, sizeof(m_uiAmbePlus));
            break;
        case CODEC_CODEC2_3200:
        case CODEC_CODEC2_2400:
            ::memset(m_uiCodec2, 0, sizeof(m_uiCodec2));
            break;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// operators

bool CDvFramePacket::operator ==(const CDvFramePacket &DvFrame) const
{
    return ( (::memcmp(m_uiAmbe, DvFrame.m_uiAmbe, sizeof(m_uiAmbe)) == 0) &&
             (::memcmp(m_uiCodec2, DvFrame.m_uiCodec2, sizeof(m_uiCodec2)) == 0) &&
             (::memcmp(m_uiDvData, DvFrame.m_uiDvData, sizeof(m_uiDvData)) == 0) &&
             (::memcmp(m_uiAmbePlus, DvFrame.m_uiAmbePlus, sizeof(m_uiAmbePlus)) == 0) &&
             (::memcmp(m_uiDvSync, DvFrame.m_uiDvSync, sizeof(m_uiDvSync)) == 0) );
}
