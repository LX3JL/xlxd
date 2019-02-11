//
//  cdvheaderpacket.cpp
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
#include <cstdio>
#include "cdmriddir.h"
#include "cdvheaderpacket.h"

////////////////////////////////////////////////////////////////////////////////////////
// constructor

CDvHeaderPacket::CDvHeaderPacket()
{
    m_uiFlag1 = 0;
    m_uiFlag2 = 0;
    m_uiFlag3 = 0;
    m_uiCrc = 0;
}

// dstar constructor

CDvHeaderPacket::CDvHeaderPacket(const struct dstar_header *buffer, uint16 sid, uint8 pid)
    : CPacket(sid, pid)
{
    m_uiFlag1 = buffer->Flag1;
    m_uiFlag2 = buffer->Flag2;
    m_uiFlag3 = buffer->Flag3;
    m_csUR.SetCallsign(buffer->UR, CALLSIGN_LEN);
    m_csRPT1.SetCallsign(buffer->RPT1, CALLSIGN_LEN);
    m_csRPT2.SetCallsign(buffer->RPT2, CALLSIGN_LEN);
    m_csMY.SetCallsign(buffer->MY, CALLSIGN_LEN);
    m_csMY.SetSuffix(buffer->SUFFIX, CALLSUFFIX_LEN);
    m_uiCrc = buffer->Crc;
}

// dmr constructor

CDvHeaderPacket::CDvHeaderPacket(uint32 my, const CCallsign &ur, const CCallsign &rpt1, const CCallsign &rpt2, uint16 sid, uint8 pid, uint8 spid)
    : CPacket(sid, pid, spid)
{
    m_uiFlag1 = 0;
    m_uiFlag2 = 0;
    m_uiFlag3 = 0;
    m_uiCrc = 0;
    m_csUR = ur;
    m_csRPT1 = rpt1;
    m_csRPT2 = rpt2;
    m_csMY = CCallsign("", my);
}

// copy constructor

CDvHeaderPacket::CDvHeaderPacket(const CDvHeaderPacket &Header)
: CPacket(Header)
{
    m_uiFlag1 = Header.m_uiFlag1;
    m_uiFlag2 = Header.m_uiFlag2;
    m_uiFlag3 = Header.m_uiFlag3;
    m_csUR = Header.m_csUR;
    m_csRPT1 = Header.m_csRPT1;
    m_csRPT2 = Header.m_csRPT2;
    m_csMY = Header.m_csMY;
    m_uiCrc = Header.m_uiCrc;
}


////////////////////////////////////////////////////////////////////////////////////////
// virtual duplication

CPacket *CDvHeaderPacket::Duplicate(void) const
{
    return new CDvHeaderPacket(*this);
}

////////////////////////////////////////////////////////////////////////////////////////
// conversion

void CDvHeaderPacket::ConvertToDstarStruct(struct dstar_header *buffer, uint8 CodecOut) const
{
    ::memset(buffer, 0, sizeof(struct dstar_header));
    buffer->Flag1 = m_uiFlag1;
    buffer->Flag2 = m_uiFlag2;
    buffer->Flag3 = (CodecOut == CODEC_AMBEPLUS) ? 0x00 : 0x01 ;
    m_csUR.GetCallsign(buffer->UR);
    m_csRPT1.GetCallsign(buffer->RPT1);
    m_csRPT2.GetCallsign(buffer->RPT2);
    m_csMY.GetCallsign(buffer->MY);
    m_csMY.GetSuffix(buffer->SUFFIX);
    buffer->Crc = m_uiCrc;
}


////////////////////////////////////////////////////////////////////////////////////////
// get valid

bool CDvHeaderPacket::IsValid(void) const
{
    bool valid = CPacket::IsValid();
    
    valid &= m_csRPT1.IsValid();
    valid &= m_csRPT2.IsValid();
    valid &= m_csMY.IsValid();
    
    return valid;
}


////////////////////////////////////////////////////////////////////////////////////////
// get codec

uint8 CDvHeaderPacket::GetCodec(void) const
{
    // The D-STAR vocoder extension by SV9OAN uses Flag 3 of the header
    // to identify whether the voice data payload is in Codec 2 format.
    // Two Codec 2 modes are allowed (3200 or 2400) and optional FEC.
    // Only support 3200 mode and no FEC here.
    if ( m_uiFlag3 == 0x01 )
    {
        return CODEC_CODEC2;
    }

    return CODEC_AMBEPLUS;
}


////////////////////////////////////////////////////////////////////////////////////////
// operators

bool CDvHeaderPacket::operator ==(const CDvHeaderPacket &Header) const
{
    return ( (m_uiFlag1 == Header.m_uiFlag1) &&
             (m_uiFlag2 == Header.m_uiFlag2) &&
             (m_uiFlag3 == Header.m_uiFlag3) &&
             (m_csUR == Header.m_csUR) &&
             (m_csRPT1 == Header.m_csRPT1) &&
             (m_csRPT2 == Header.m_csRPT2) &&
             (m_csMY == Header.m_csMY) );
}

#ifdef IMPLEMENT_CDVHEADERPACKET_CONST_CHAR_OPERATOR
CDvHeaderPacket::operator const char *() const
{
	char *sz = (char *)(const char *)m_sz;

    std::sprintf(sz, "%02X %02X %02X\n%s\n%s\n%s\n%s",
        m_uiFlag1, m_uiFlag2, m_uiFlag3,
        (const char *)m_csUR,
        (const char *)m_csRPT1,
        (const char *)m_csRPT2,
        (const char *)m_csMY);
        
    return m_sz;
}
#endif
