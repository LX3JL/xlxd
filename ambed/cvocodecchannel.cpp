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

CVocodecChannel::CVocodecChannel(CVocodecInterface *InterfaceIn, int iChIn, CVocodecInterface *InterfaceOut1, int iChOut1, CVocodecInterface *InterfaceOut2, int iChOut2, int iSpeechGain)
{
    m_bOpen = false;
    m_GroupChannels.reserve(2);
    m_InterfaceIn = InterfaceIn;
    m_iChannelIn = iChIn;
    m_InterfaceOut1 = InterfaceOut1;
    m_iChannelOut1 = iChOut1;
    m_InterfaceOut2 = InterfaceOut2;
    m_iChannelOut2 = iChOut2;
    m_iSpeechGain = iSpeechGain;
}

////////////////////////////////////////////////////////////////////////////////////////
// destructor

CVocodecChannel::~CVocodecChannel()
{
    // empty array of grouped channels
    // channels are deleted by their owner (CVocodecs)
    m_GroupChannels.clear();

    PurgeAllQueues();
}

////////////////////////////////////////////////////////////////////////////////////////
// manage group

void CVocodecChannel::AddGroupChannel(CVocodecChannel *Channel)
{
    m_GroupChannels.push_back(Channel);
}

bool CVocodecChannel::IsAvailable(void) const
{
    if ( m_bOpen )
    {
        return false;
    }

    bool available = true;

    for ( int i = 0; (i < m_GroupChannels.size()) && available; i++ )
    {
        if ( m_GroupChannels[i]->m_bOpen )
        {
            available = false;
        }
    }

    return available;
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
        m_InterfaceIn->SetChannelWithChannelIn(this, m_iChannelIn);
        m_InterfaceOut1->SetChannelWithChannelOut(this, m_iChannelOut1);
        m_InterfaceOut2->SetChannelWithChannelOut(this, m_iChannelOut2);
        std::cout << "Vocodec channel " <<
            m_InterfaceIn->GetName() << ":" << (int)m_iChannelIn << " -> " <<
            m_InterfaceOut1->GetName() << ":" << (int)m_iChannelOut1 << ", " <<
            m_InterfaceOut2->GetName() << ":" << (int)m_iChannelOut2 << " open" << std::endl;
    }
    return ok;
}

void CVocodecChannel::Close(void)
{
    if ( m_bOpen )
    {
        m_bOpen = false;
        PurgeAllQueues();
        m_InterfaceIn->SetChannelWithChannelIn(NULL, m_iChannelIn);
        m_InterfaceOut1->SetChannelWithChannelOut(NULL, m_iChannelOut1);
        m_InterfaceOut2->SetChannelWithChannelOut(NULL, m_iChannelOut2);
        std::cout << "Vocodec channel " <<
            m_InterfaceIn->GetName() << ":" << (int)m_iChannelIn << " -> " <<
            m_InterfaceOut1->GetName() << ":" << (int)m_iChannelOut1 << ", " <<
            m_InterfaceOut2->GetName() << ":" << (int)m_iChannelOut2 << " closed" << std::endl;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// get

uint8 CVocodecChannel::GetCodecIn(void) const
{
    return m_InterfaceIn->GetChannelCodec(m_iChannelIn);
}

uint8 CVocodecChannel::GetCodecOut1(void) const
{
    return m_InterfaceOut1->GetChannelCodec(m_iChannelOut1);
}

uint8 CVocodecChannel::GetCodecOut2(void) const
{
    return m_InterfaceOut2->GetChannelCodec(m_iChannelOut2);
}

uint8 CVocodecChannel::GetCodecsOut(void) const
{
    return GetCodecOut1() | GetCodecOut2();
}

////////////////////////////////////////////////////////////////////////////////////////
// queues helpers

void CVocodecChannel::PurgeAllQueues(void)
{
    GetPacketQueueIn()->Purge();
    ReleasePacketQueueIn();
    GetPacketQueueOut1()->Purge();
    ReleasePacketQueueOut1();
    GetPacketQueueOut2()->Purge();
    ReleasePacketQueueOut2();
    GetVoiceQueue1()->Purge();
    ReleaseVoiceQueue1();
    GetVoiceQueue2()->Purge();
    ReleaseVoiceQueue2();
}



