//
//  cvoicepacket.cpp
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
#include <math.h>
#include "cvoicepacket.h"


////////////////////////////////////////////////////////////////////////////////////////
// constructor

CVoicePacket::CVoicePacket()
{
    m_iSize = 0;
    ::memset(m_uiVoice, 0, sizeof(m_uiVoice));
}

CVoicePacket::CVoicePacket(const uint8 *voice, int size)
{
    m_iSize = MIN(size, sizeof(m_uiVoice));
    ::memset(m_uiVoice, 0, sizeof(m_uiVoice));
    ::memcpy(m_uiVoice, voice, m_iSize);
}

CVoicePacket::CVoicePacket(const CVoicePacket &packet)
    : CPacket(packet)
{
    m_iSize = packet.m_iSize;
    ::memcpy(m_uiVoice, packet.m_uiVoice, sizeof(m_uiVoice));
}


////////////////////////////////////////////////////////////////////////////////////////
// destructor

CVoicePacket::~CVoicePacket()
{
}

////////////////////////////////////////////////////////////////////////////////////////
// Set

void CVoicePacket::SetVoice(const uint8 *voice, int size)
{
    m_iSize = MIN(size, sizeof(m_uiVoice));
    ::memset(m_uiVoice, 0, sizeof(m_uiVoice));
    ::memcpy(m_uiVoice, voice, m_iSize);
}


////////////////////////////////////////////////////////////////////////////////////////
// gain

void CVoicePacket::ApplyGain(int dB)
{
    if ( dB == 0 )
    {
        return;
    }

    float mult = pow(10, dB/20.0);
    for ( int i = 0; i < m_iSize; i += 2 )
    {
        float smp = (float)(short)MAKEWORD(m_uiVoice[i+1], m_uiVoice[i]);
        smp *= mult;
        m_uiVoice[i] = HIBYTE((short)smp);
        m_uiVoice[i+1] = LOBYTE((short)smp);
    }
}
