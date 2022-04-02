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

CStream::CStream(uint16 uiId, const CCallsign &Callsign, const CIp &Ip, uint8 uiCodecIn, uint8 uiCodecOut)
{
    m_uiId = uiId;
    m_Callsign = Callsign;
    m_Ip = Ip;
    m_uiPort = 0;
    m_uiCodecIn = uiCodecIn;
    m_uiCodecOut = uiCodecOut;
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
    // stop thread first
    m_bStopThread = true;
    if ( m_pThread != NULL )
    {
        m_pThread->join();
        delete m_pThread;
        m_pThread = NULL;
    }
    
    // then close everything
    m_Socket.Close();
    if ( m_VocodecChannel != NULL )
    {
        m_VocodecChannel->Close();
    }
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
        ok &= ((m_VocodecChannel = g_Vocodecs.OpenChannel(m_uiCodecIn, m_uiCodecOut)) != NULL);
        
        if ( ok )
        {
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
        std::cout << "Error opening socket on port UDP" << uiPort << " on ip " << g_AmbeServer.GetListenIp() << std::endl;
    }
    
    // done
    return ok;
    
}

void CStream::Close(void)
{
    // stop thread first
    m_bStopThread = true;
    if ( m_pThread != NULL )
    {
        m_pThread->join();
        delete m_pThread;
        m_pThread = NULL;
    }

    // then close everything
    m_Socket.Close();
    if ( m_VocodecChannel != NULL )
    {
        m_VocodecChannel->Close();
    }
    
    
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
    CIp         Ip;
    uint8       uiPid;
    uint8       Ambe[AMBE_FRAME_SIZE];
    CAmbePacket *packet;
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
            packet = new CAmbePacket(uiPid, m_uiCodecIn, Ambe);
            queue = m_VocodecChannel->GetPacketQueueIn();
            queue->push(packet);
            m_VocodecChannel->ReleasePacketQueueIn();
        }
    }
    
    // anything in our queue ?
    queue = m_VocodecChannel->GetPacketQueueOut();
    while ( !queue->empty() )
    {
        // get the packet
        packet = (CAmbePacket *)queue->front();
        queue->pop();
        // send it to client
        EncodeDvFramePacket(&Buffer, packet->GetPid(), packet->GetAmbe());
        m_Socket.Send(Buffer, m_Ip, m_uiPort);
        // and done
        delete packet;
    }
    m_VocodecChannel->ReleasePacketQueueOut();
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
// packet encodeing helpers

void CStream::EncodeDvFramePacket(CBuffer *Buffer, uint8 Pid, uint8 *Ambe)
{
    Buffer->clear();
    Buffer->Append((uint8)GetCodecOut());
    Buffer->Append((uint8)Pid);
    Buffer->Append(Ambe, 9);
}

