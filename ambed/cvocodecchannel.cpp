//
//  cvocodecchannel.cpp
//  ambed
//
//  Created by Jean-Luc Deltombe (LX3JL) on 23/04/2017.
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
#include "ctimepoint.h"
#include "cvocodecchannel.h"
#include "cvocodecinterface.h"

////////////////////////////////////////////////////////////////////////////////////////
// constructor

CVocodecChannel::CVocodecChannel(CVocodecInterface *InterfaceIn, int iChIn, CVocodecInterface *InterfaceOut, int iChOut, int iSpeechGain)
: m_AGC((float)iSpeechGain)
{
    m_bOpen = false;
    m_InterfaceIn = InterfaceIn;
    m_iChannelIn = iChIn;
    m_InterfaceOut = InterfaceOut;
    m_iChannelOut = iChOut;
    m_iSpeechGain = iSpeechGain;
}

////////////////////////////////////////////////////////////////////////////////////////
// destructor

CVocodecChannel::~CVocodecChannel()
{
    PurgeAllQueues();
}

////////////////////////////////////////////////////////////////////////////////////////
// open & close

bool CVocodecChannel::Open(void)
{
    bool ok = false;
    if ( !m_bOpen )
    {
        m_bOpen = true;
        ok = true;
        PurgeAllQueues();
        std::cout << "Vocodec channel " <<
            m_InterfaceIn->GetName() << ":" << (int)m_iChannelIn << " -> " <<
            m_InterfaceOut->GetName() << ":" << (int)m_iChannelOut << " open" << std::endl;
    }
    return ok;
}

void CVocodecChannel::Close(void)
{
    if ( m_bOpen )
    {
        m_bOpen = false;
        PurgeAllQueues();
        std::cout << "Vocodec channel " <<
        m_InterfaceIn->GetName() << ":" << (int)m_iChannelIn << " -> " <<
        m_InterfaceOut->GetName() << ":" << (int)m_iChannelOut << " closed" << std::endl;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// get

uint8 CVocodecChannel::GetCodecIn(void) const
{
    return m_InterfaceIn->GetChannelCodec(m_iChannelIn);
}

uint8 CVocodecChannel::GetCodecOut(void) const
{
    return m_InterfaceOut->GetChannelCodec(m_iChannelOut);
}

void CVocodecChannel::ApplyAGC(CVoicePacket& voicePacket)
{
    m_AGC.Apply(voicePacket.GetVoice(), voicePacket.GetVoiceSize());
    std::cout << "Gain : " << m_AGC.GetGain() << "\n";
}

////////////////////////////////////////////////////////////////////////////////////////
// queues helpers

void CVocodecChannel::PurgeAllQueues(void)
{
    GetPacketQueueIn()->Purge();
    ReleasePacketQueueIn();
    GetPacketQueueOut()->Purge();
    ReleasePacketQueueOut();
    GetVoiceQueue()->Purge();
    ReleaseVoiceQueue();
}



