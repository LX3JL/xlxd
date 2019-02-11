//
//  cstream.cpp
//  ambed
//
//  Created by Jean-Luc Deltombe (LX3JL) on 15/04/2017.
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
#include "cambeserver.h"
#include "cvocodecs.h"
#include "cambepacket.h"
#include "cstream.h"

////////////////////////////////////////////////////////////////////////////////////////
// define

#define AMBE_FRAME_SIZE         9

////////////////////////////////////////////////////////////////////////////////////////
// constructor

CStream::CStream()
{
    m_uiId = 0;
    m_uiPort = 0;
    m_bStopThread = false;
    m_pThread = NULL;
    m_VocodecChannel = NULL;
    m_LastActivity.Now();
    m_iTotalPackets = 0;
    m_iLostPackets = 0;
}

CStream::CStream(uint16 uiId, const CCallsign &Callsign, const CIp &Ip, uint8 uiCodecIn, uint8 uiCodecsOut)
{
    m_uiId = uiId;
    m_Callsign = Callsign;
    m_Ip = Ip;
    m_uiPort = 0;
    m_uiCodecIn = uiCodecIn;
    m_uiCodecsOut = uiCodecsOut;
    m_bStopThread = false;
    m_pThread = NULL;
    m_VocodecChannel = NULL;
    m_LastActivity.Now();
    m_iTotalPackets = 0;
    m_iLostPackets = 0;
}

////////////////////////////////////////////////////////////////////////////////////////
// destructor

CStream::~CStream()
{
    m_Socket.Close();
    if ( m_VocodecChannel != NULL )
    {
        g_Vocodecs.CloseChannel(m_VocodecChannel);
        m_VocodecChannel = NULL;
    }
    
    m_bStopThread = true;
    if ( m_pThread != NULL )
    {
        m_pThread->join();
        delete m_pThread;
    }

    PurgeAllQueues();
}

////////////////////////////////////////////////////////////////////////////////////////
// initialization

bool CStream::Init(uint16 uiPort)
{
    bool ok;
    
    // reset stop flag
    m_bStopThread = false;
    
    // create our socket
    ok = m_Socket.Open(g_AmbeServer.GetListenIp(), uiPort);
    if ( ok )
    {
        // open the vocodecchannel
        ok &= ((m_VocodecChannel = g_Vocodecs.OpenChannel(m_uiCodecIn, m_uiCodecsOut)) != NULL);
        
        if ( ok )
        {
            PurgeAllQueues();

            // store port
            m_uiPort = uiPort;
            
            // start  thread;
            m_pThread = new std::thread(CStream::Thread, this);
            
            // init timeout system
            m_LastActivity.Now();
            m_iTotalPackets = 0;
            m_iLostPackets = 0;
            
        }
        else
        {
            std::cout << "Error opening stream : no suitable channel available" << std::endl;
        }
    }
    else
    {
        std::cout << "Error opening socket on port UDP" << uiPort << " on ip " << m_Ip << std::endl;
    }
    
    // done
    return ok;
    
}

void CStream::Close(void)
{
    // close everything
    m_Socket.Close();
    if ( m_VocodecChannel != NULL )
    {
        m_VocodecChannel->Close();
    }
    
    m_bStopThread = true;
    if ( m_pThread != NULL )
    {
        m_pThread->join();
        delete m_pThread;
        m_pThread = NULL;
    }
    
    PurgeAllQueues();

    // report
    std::cout << m_iLostPackets << " of " << m_iTotalPackets << " packets lost" << std::endl;
}

////////////////////////////////////////////////////////////////////////////////////////
// thread

void CStream::Thread(CStream *This)
{
    while ( !This->m_bStopThread )
    {
        This->Task();
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// task

void CStream::Task(void)
{
    CBuffer     Buffer;
    static CIp  Ip;
    uint8       uiPid;
    uint8       Ambe[AMBE_FRAME_SIZE];
    CAmbePacket *packet1;
    CAmbePacket *packet2;
    CPacketQueue *queue;
    
    // anything coming in from codec client ?
    if ( m_Socket.Receive(&Buffer, &Ip, 1) != -1 )
    {
        // crack packet
        if ( IsValidDvFramePacket(Buffer, &uiPid, Ambe) )
        {
            // transcode AMBE here
            m_LastActivity.Now();
            m_iTotalPackets++;
            
            // post packet to VocoderChannel
            packet1 = new CAmbePacket(uiPid, m_uiCodecIn, Ambe);
            queue = m_VocodecChannel->GetPacketQueueIn();
            queue->push(packet1);
            m_VocodecChannel->ReleasePacketQueueIn();
        }
    }
    
    // anything in our queue ?
    queue = m_VocodecChannel->GetPacketQueueOut1();
    while ( !queue->empty() )
    {
        // get the packet
        packet1 = (CAmbePacket *)queue->front();
        queue->pop();
        // add it to the outgoing queue
        m_QueuePacketOut1.push(packet1);
    }
    m_VocodecChannel->ReleasePacketQueueOut1();

    queue = m_VocodecChannel->GetPacketQueueOut2();
    while ( !queue->empty() )
    {
        // get the packet
        packet2 = (CAmbePacket *)queue->front();
        queue->pop();
        // add it to the outgoing queue
        m_QueuePacketOut2.push(packet2);
    }
    m_VocodecChannel->ReleasePacketQueueOut2();

    while ( !m_QueuePacketOut1.empty() && !m_QueuePacketOut2.empty() )
    {
        packet1 = (CAmbePacket *)m_QueuePacketOut1.front();
        m_QueuePacketOut1.pop();
        packet2 = (CAmbePacket *)m_QueuePacketOut2.front();
        m_QueuePacketOut2.pop();
        // send it to client
        // TODO :
        //      when packet PIDs are preserverd, make sure that they match
        EncodeDvFramePacket(&Buffer, packet1->GetPid(), packet1->GetCodec(), packet1->GetAmbe(), packet2->GetCodec(), packet2->GetAmbe());
        m_Socket.Send(Buffer, Ip);
        // and done
        delete packet1;
        delete packet2;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// packet decoding helpers

bool CStream::IsValidDvFramePacket(const CBuffer &Buffer, uint8 *pid, uint8 *ambe)
{
    bool valid = false;
    
    if ( Buffer.size() == 11 )
    {
        uint8 codec = Buffer.data()[0];
        *pid = Buffer.data()[1];
        ::memcpy(ambe, &(Buffer.data()[2]), 9);
        valid = (codec == GetCodecIn());
    }
    
    return valid;
}

////////////////////////////////////////////////////////////////////////////////////////
// packet encoding helpers

void CStream::EncodeDvFramePacket(CBuffer *Buffer, uint8 Pid, uint8 Codec1, uint8 *Ambe1, uint8 Codec2, uint8 *Ambe2)
{
    Buffer->clear();
    Buffer->Append((uint8)Codec1);
    Buffer->Append((uint8)Codec2);
    Buffer->Append((uint8)Pid);
    Buffer->Append(Ambe1, 9);
    Buffer->Append(Ambe2, 9);
}

////////////////////////////////////////////////////////////////////////////////////////
// queues helpers

void CStream::PurgeAllQueues(void)
{
    m_QueuePacketOut1.Purge();
    m_QueuePacketOut2.Purge();
}

