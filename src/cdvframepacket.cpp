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
    m_bHasAmbe = false;
    ::memset(m_uiAmbePlus, 0, sizeof(m_uiAmbePlus));
    ::memset(m_uiDvSync, 0, sizeof(m_uiDvSync));
    m_bHasAmbePlus = false;
    ::memset(m_uiCodec2, 0, sizeof(m_uiCodec2));
    m_bHasCodec2 = false;
    ::memset(m_uiImbe, 0, sizeof(m_uiImbe));
    m_bHasImbe = false;
};

// dstar constructor

CDvFramePacket::CDvFramePacket(const struct dstar_dvframe *dvframe, uint16 sid, uint8 pid)
    : CPacket(sid, pid)
{
    ::memcpy(m_uiAmbe, dvframe->AMBE, sizeof(m_uiAmbe));
    ::memcpy(m_uiDvData, dvframe->DVDATA, sizeof(m_uiDvData));
    m_bHasAmbe = true;          // D-Star native — real AMBE+ data
    ::memset(m_uiAmbePlus, 0, sizeof(m_uiAmbePlus));
    ::memset(m_uiDvSync, 0, sizeof(m_uiDvSync));
    m_bHasAmbePlus = false;
    ::memset(m_uiCodec2, 0, sizeof(m_uiCodec2));
    m_bHasCodec2 = false;
    ::memset(m_uiImbe, 0, sizeof(m_uiImbe));
    m_bHasImbe = false;
}

// dmr constructor

CDvFramePacket::CDvFramePacket(const uint8 *ambe, const uint8 *sync, uint16 sid, uint8 pid, uint8 spid)
    : CPacket(sid, pid, spid)
{
    ::memcpy(m_uiAmbePlus, ambe, sizeof(m_uiAmbePlus));
    ::memcpy(m_uiDvSync, sync, sizeof(m_uiDvSync));
    m_bHasAmbePlus = true;      // DMR native — real AMBE+2 data
    ::memset(m_uiAmbe, 0, sizeof(m_uiAmbe));
    ::memset(m_uiDvData, 0, sizeof(m_uiDvData));
    m_bHasAmbe = false;
    ::memset(m_uiCodec2, 0, sizeof(m_uiCodec2));
    m_bHasCodec2 = false;
    ::memset(m_uiImbe, 0, sizeof(m_uiImbe));
    m_bHasImbe = false;
}

// ysf constructor

CDvFramePacket::CDvFramePacket(const uint8 *ambe, uint16 sid, uint8 pid, uint8 spid, uint8 fid)
: CPacket(sid, pid, spid, fid)
{
    ::memcpy(m_uiAmbePlus, ambe, sizeof(m_uiAmbePlus));
    ::memset(m_uiDvSync, 0, sizeof(m_uiDvSync));
    m_bHasAmbePlus = true;      // YSF/NXDN native — real AMBE+2 data
    ::memset(m_uiAmbe, 0, sizeof(m_uiAmbe));
    ::memset(m_uiDvData, 0, sizeof(m_uiDvData));
    m_bHasAmbe = false;
    ::memset(m_uiCodec2, 0, sizeof(m_uiCodec2));
    m_bHasCodec2 = false;
    ::memset(m_uiImbe, 0, sizeof(m_uiImbe));
    m_bHasImbe = false;
}

// imrs constructor

CDvFramePacket::CDvFramePacket(const uint8 *ambe, uint16 sid, uint8 pid, uint8 spid, uint16 fid)
: CPacket(sid, pid, spid, fid)
{
    ::memcpy(m_uiAmbePlus, ambe, sizeof(m_uiAmbePlus));
    ::memset(m_uiDvSync, 0, sizeof(m_uiDvSync));
    m_bHasAmbePlus = true;      // IMRS native — real AMBE+2 data
    ::memset(m_uiAmbe, 0, sizeof(m_uiAmbe));
    ::memset(m_uiDvData, 0, sizeof(m_uiDvData));
    m_bHasAmbe = false;
    ::memset(m_uiCodec2, 0, sizeof(m_uiCodec2));
    m_bHasCodec2 = false;
    ::memset(m_uiImbe, 0, sizeof(m_uiImbe));
    m_bHasImbe = false;
}

// xlx constructor

CDvFramePacket::CDvFramePacket
    (uint16 sid,
     uint8 dstarpid, const uint8 *dstarambe, const uint8 *dstardvdata,
     uint8 dmrpid, uint8 dprspid, const uint8 *dmrambe, const uint8 *dmrsync)
: CPacket(sid, dstarpid, dmrpid, dprspid, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFFFF)
{
    // XLX interlink — wire-format-aware but NOT source-aware. The rev
    // 2/3 wire frame reserves slots for AMBE+ (D-Star) and AMBE+2
    // (DMR), but whether those slots carry real codec data depends on
    // the SENDER, not the wire format:
    //   - Native CXlxPeer: both slots authoritative.
    //   - CXlxDmrPeer (BM, FreeDMR, FreeStar, DVSPH): AMBE+2 only;
    //     dstar slot is reserved but empty/stale.
    // The constructor therefore leaves m_bHasAmbe and m_bHasAmbePlus
    // at their default false. CXlxProtocol::OnDvFramePacketIn calls
    // ConformAvailableCodecs() with the source client's actual codec
    // set immediately after parsing, which is the authoritative step.
    // The rev-3 Codec2 slot is populated by a subsequent SetCodec2()
    // call (which sets m_bHasCodec2); Conform then sets it from the
    // source mask, which may clear it for non-Codec2 sources.
    ::memcpy(m_uiAmbe, dstarambe, sizeof(m_uiAmbe));
    ::memcpy(m_uiDvData, dstardvdata, sizeof(m_uiDvData));
    m_bHasAmbe = false;
    ::memcpy(m_uiAmbePlus, dmrambe, sizeof(m_uiAmbePlus));
    ::memcpy(m_uiDvSync, dmrsync, sizeof(m_uiDvSync));
    m_bHasAmbePlus = false;
    ::memset(m_uiCodec2, 0, sizeof(m_uiCodec2));
    m_bHasCodec2 = false;
    ::memset(m_uiImbe, 0, sizeof(m_uiImbe));
    m_bHasImbe = false;
}

// m17 constructor

CDvFramePacket::CDvFramePacket(const uint8 *codec2, uint16 sid, uint8 pid)
: CPacket(sid, pid)
{
    ::memcpy(m_uiCodec2, codec2, sizeof(m_uiCodec2));
    m_bHasCodec2 = true;
    ::memset(m_uiAmbe, 0, sizeof(m_uiAmbe));
    ::memset(m_uiDvData, 0, sizeof(m_uiDvData));
    m_bHasAmbe = false;
    ::memset(m_uiAmbePlus, 0, sizeof(m_uiAmbePlus));
    ::memset(m_uiDvSync, 0, sizeof(m_uiDvSync));
    m_bHasAmbePlus = false;
    ::memset(m_uiImbe, 0, sizeof(m_uiImbe));
    m_bHasImbe = false;
}

// p25 constructor

CDvFramePacket::CDvFramePacket(uint16 sid, const uint8 *imbe)
: CPacket(sid, 0xFF)
{
    ::memcpy(m_uiImbe, imbe, sizeof(m_uiImbe));
    m_bHasImbe = true;
    ::memset(m_uiAmbe, 0, sizeof(m_uiAmbe));
    ::memset(m_uiDvData, 0, sizeof(m_uiDvData));
    m_bHasAmbe = false;
    ::memset(m_uiAmbePlus, 0, sizeof(m_uiAmbePlus));
    ::memset(m_uiDvSync, 0, sizeof(m_uiDvSync));
    m_bHasAmbePlus = false;
    ::memset(m_uiCodec2, 0, sizeof(m_uiCodec2));
    m_bHasCodec2 = false;
}

// copy constructor

CDvFramePacket::CDvFramePacket(const CDvFramePacket &DvFrame)
    : CPacket(DvFrame)
{
    ::memcpy(m_uiAmbe, DvFrame.m_uiAmbe, sizeof(m_uiAmbe));
    ::memcpy(m_uiDvData, DvFrame.m_uiDvData, sizeof(m_uiDvData));
    m_bHasAmbe = DvFrame.m_bHasAmbe;
    ::memcpy(m_uiAmbePlus, DvFrame.m_uiAmbePlus, sizeof(m_uiAmbePlus));
    ::memcpy(m_uiDvSync, DvFrame.m_uiDvSync, sizeof(m_uiDvSync));
    m_bHasAmbePlus = DvFrame.m_bHasAmbePlus;
    ::memcpy(m_uiCodec2, DvFrame.m_uiCodec2, sizeof(m_uiCodec2));
    m_bHasCodec2 = DvFrame.m_bHasCodec2;
    ::memcpy(m_uiImbe, DvFrame.m_uiImbe, sizeof(m_uiImbe));
    m_bHasImbe = DvFrame.m_bHasImbe;
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
        case CODEC_CODEC2:      return m_uiCodec2;
        case CODEC_IMBE:        return m_uiImbe;
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
    // Each byte-write also sets the matching Has* flag. The flag
    // assignment is bound to the same case branch as the memcpy so
    // there's no way a slot can be populated without flagging it.
    // See cdvframepacket.h Has* accessor comments for why this
    // matters (codec-stream Phase 1 overwrite protection).
    switch (uiCodec)
    {
        case CODEC_AMBEPLUS:
            ::memcpy(m_uiAmbe, Ambe, sizeof(m_uiAmbe));
            m_bHasAmbe = true;
            break;
        case CODEC_AMBE2PLUS:
            ::memcpy(m_uiAmbePlus, Ambe, sizeof(m_uiAmbePlus));
            m_bHasAmbePlus = true;
            break;
        case CODEC_CODEC2:
            ::memcpy(m_uiCodec2, Ambe, sizeof(m_uiCodec2));
            m_bHasCodec2 = true;
            break;
        case CODEC_IMBE:
            ::memcpy(m_uiImbe, Ambe, sizeof(m_uiImbe));
            m_bHasImbe = true;
            break;
    }
}

void CDvFramePacket::SetCodec2(const uint8 *codec2)
{
    ::memcpy(m_uiCodec2, codec2, sizeof(m_uiCodec2));
    m_bHasCodec2 = true;
}

void CDvFramePacket::ConformAvailableCodecs(uint32 mask)
{
    m_bHasAmbe     = (mask & (1U << CODEC_AMBEPLUS))  != 0U;
    m_bHasAmbePlus = (mask & (1U << CODEC_AMBE2PLUS)) != 0U;
    m_bHasCodec2   = (mask & (1U << CODEC_CODEC2))    != 0U;
    m_bHasImbe     = (mask & (1U << CODEC_IMBE))      != 0U;
}


////////////////////////////////////////////////////////////////////////////////////////
// operators

bool CDvFramePacket::operator ==(const CDvFramePacket &DvFrame) const
{
    return ( (::memcmp(m_uiAmbe, DvFrame.m_uiAmbe, sizeof(m_uiAmbe)) == 0) &&
             (::memcmp(m_uiDvData, DvFrame.m_uiDvData, sizeof(m_uiDvData)) == 0) &&
             (m_bHasAmbe == DvFrame.m_bHasAmbe) &&
             (::memcmp(m_uiAmbePlus, DvFrame.m_uiAmbePlus, sizeof(m_uiAmbePlus)) == 0) &&
             (::memcmp(m_uiDvSync, DvFrame.m_uiDvSync, sizeof(m_uiDvSync)) == 0) &&
             (m_bHasAmbePlus == DvFrame.m_bHasAmbePlus) &&
             (::memcmp(m_uiCodec2, DvFrame.m_uiCodec2, sizeof(m_uiCodec2)) == 0) &&
             (m_bHasCodec2 == DvFrame.m_bHasCodec2) &&
             (::memcmp(m_uiImbe, DvFrame.m_uiImbe, sizeof(m_uiImbe)) == 0) &&
             (m_bHasImbe == DvFrame.m_bHasImbe) );
}
