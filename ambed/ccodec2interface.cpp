//
//  ccodec2interface.cpp
//  ambed
//
//  Created by Antony Chazapis (SV9OAN) on 26/12/2018.
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
#include "ctimepoint.h"
#include "cambepacket.h"
#include "cvoicepacket.h"
#include "ccodec2interface.h"
#include "cvocodecchannel.h"


////////////////////////////////////////////////////////////////////////////////////////
// constructor

CCodec2Interface::CCodec2Interface()
: CVocodecInterface()
{}

////////////////////////////////////////////////////////////////////////////////////////
// destructor

CCodec2Interface::~CCodec2Interface()
{
    codec2_destroy(codec2_state);
}

////////////////////////////////////////////////////////////////////////////////////////
// initialization

bool CCodec2Interface::Init(void)
{
    bool ok = true;

    // create codec state    
    codec2_state = codec2_create(CODEC2_MODE_3200);
    if (codec2_state == NULL)
    {
        ok = false;
    }
    
    // base class
    if ( ok )
    {
        ok &= CVocodecInterface::Init();
    }
    
    // done
    return ok;
}

////////////////////////////////////////////////////////////////////////////////////////
// task

void CCodec2Interface::Task(void)
{
    CPacketQueue    *Queue;
    CVocodecChannel *Channel;
    CAmbePacket     AmbePacket;
    CVoicePacket    VoicePacket;
    bool            done;
    
    // process the streams (channels) incoming queue
    do
    {
        done = true;
        for ( int i = 0; i < GetNbChannels(); i++ )
        {
            // get active outgoing channel for interface channel
            Channel = GetChannelWithChannelOut(i);
            
            // any packet in voice queue 1 ?
            if ( Channel != NULL && Channel->IsInterfaceOut1(this) )
            {
                CVoicePacket *Packet = NULL;

                Queue = Channel->GetVoiceQueue1();
                if ( !Queue->empty() )
                {
                    // get packet
                    Packet = (CVoicePacket *)Queue->front();
                    Queue->pop();                    
                }
                Channel->ReleaseVoiceQueue1();

                if ( Packet != NULL )
                {
                    // this is second step of transcoding
                    // we just received from a decoded speech packet
                    // encode and cpush back to relevant channel outcoming queue
                    EncodeVoicePacket(Packet, &AmbePacket);
                    AmbePacket.SetPid(Packet->GetPid());
                    delete Packet;

                    CAmbePacket *clone = new CAmbePacket(AmbePacket);

                    Queue = Channel->GetPacketQueueOut1();
                    Queue->push(clone);
                    Channel->ReleasePacketQueueOut1();

                    // done
                    done = false;                    
                }
            }

            // any packet in voice queue 1 ?
            if ( Channel != NULL && Channel->IsInterfaceOut2(this) )
            {
                CVoicePacket *Packet = NULL;

                Queue = Channel->GetVoiceQueue2();
                if ( !Queue->empty() )
                {
                    // get packet
                    Packet = (CVoicePacket *)Queue->front();
                    Queue->pop();                    
                }
                Channel->ReleaseVoiceQueue2();

                if ( Packet != NULL )
                {
                    // this is second step of transcoding
                    // we just received from a decoded speech packet
                    // encode and cpush back to relevant channel outcoming queue
                    EncodeVoicePacket(Packet, &AmbePacket);
                    AmbePacket.SetPid(Packet->GetPid());
                    delete Packet;

                    CAmbePacket *clone = new CAmbePacket(AmbePacket);

                    Queue = Channel->GetPacketQueueOut2();
                    Queue->push(clone);
                    Channel->ReleasePacketQueueOut2();

                    // done
                    done = false;                    
                }
            }

            // get active incoming channel for interface channel
            Channel = GetChannelWithChannelIn(i);

            // any packet in ambe queue for us ?
            if ( Channel != NULL && Channel->IsInterfaceIn(this) )
            {
                CAmbePacket *Packet = NULL;

                Queue = Channel->GetPacketQueueIn();
                if ( !Queue->empty() )
                {
                    // get packet
                    Packet = (CAmbePacket *)Queue->front();
                    Queue->pop();
                }
                Channel->ReleasePacketQueueIn();

                if ( Packet != NULL )
                {
                    // this is first step of transcoding
                    // a fresh new packet to be transcoded is showing up
                    // decode and copy the result into both voice queues
                    DecodeAmbePacket(Packet, &VoicePacket);
                    VoicePacket.SetPid(Packet->GetPid());
                    delete Packet;

                    CVoicePacket *clone1 = new CVoicePacket(VoicePacket);
                    clone1->ApplyGain(Channel->GetSpeechGain());
                    CVoicePacket *clone2 = new CVoicePacket(*clone1);

                    Queue = Channel->GetVoiceQueue1();
                    Queue->push(clone1);
                    Channel->ReleaseVoiceQueue1();

                    Queue = Channel->GetVoiceQueue2();
                    Queue->push(clone2);
                    Channel->ReleaseVoiceQueue2();

                    // done
                    done = false;
                }
            }
        }
    } while (!done);
    
    // and wait a bit
    CTimePoint::TaskSleepFor(2);
}

////////////////////////////////////////////////////////////////////////////////////////
// decoder helper

void CCodec2Interface::DecodeAmbePacket(CAmbePacket *PacketIn, CVoicePacket *PacketOut)
{
    short voice[160];

    codec2_decode(codec2_state, voice, (unsigned char *)PacketIn->GetAmbe());
    for ( int i = 0; i < 160; i++ )
    {
        voice[i] = MAKEWORD(HIBYTE(voice[i]), LOBYTE(voice[i]));
    }
    PacketOut->SetVoice((uint8 *)voice, 160 * 2);
}

////////////////////////////////////////////////////////////////////////////////////////
// encoder helpers

void CCodec2Interface::EncodeVoicePacket(CVoicePacket *PacketIn, CAmbePacket *PacketOut)
{
    unsigned char ambe[AMBE_SIZE];
    short voice[160];

    ::memcpy(voice, (short *)PacketIn->GetVoice(), 160 * 2);    
    for ( int i = 0; i < 160; i++ )
    {
        voice[i] = MAKEWORD(HIBYTE(voice[i]), LOBYTE(voice[i]));
    }
    codec2_encode(codec2_state, ambe, voice);
    ambe[8] = 0x00;
    PacketOut->SetCodec(CODEC_CODEC2);
    PacketOut->SetAmbe((uint8 *)ambe);
}
