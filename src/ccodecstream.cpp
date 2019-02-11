//
//  ccodecstream.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 13/04/2017.
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
#include "ccodecstream.h"
#include "cdvframepacket.h"
#include "creflector.h"

////////////////////////////////////////////////////////////////////////////////////////
// define



////////////////////////////////////////////////////////////////////////////////////////
// constructor

CCodecStream::CCodecStream(CPacketStream *PacketStream, uint16 uiId, uint8 uiCodecIn, uint8 uiCodecsOut)
{
    m_bStopThread = false;
    m_pThread = NULL;
    m_uiStreamId = uiId;
    m_uiPid = 0;
    m_uiCodecIn = uiCodecIn;
    m_uiCodecsOut = uiCodecsOut;
    m_bConnected = false;
    m_fPingMin = -1;
    m_fPingMax = -1;
    m_fPingSum = 0;
    m_fPingCount = 0;
    m_uiTotalPackets = 0;
    m_uiTimeoutPackets = 0;
    m_PacketStream = PacketStream;
}

////////////////////////////////////////////////////////////////////////////////////////
// destructor

CCodecStream::~CCodecStream()
{
    // close socket
    m_Socket.Close();
    
    // kill threads
    m_bStopThread = true;
    if ( m_pThread != NULL )
    {
        m_pThread->join();
        delete m_pThread;
    }
    
    // empty local queue
    while ( !m_LocalQueue.empty() )
    {
        delete m_LocalQueue.front();
        m_LocalQueue.pop();
    }
    // empty ourselves
    while ( !empty() )
    {
        delete front();
        pop();
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// initialization

bool CCodecStream::Init(uint16 uiPort)
{
    bool ok;
    
    // reset stop flag
    m_bStopThread = false;
    
    // create server's IP
    m_Ip = g_Reflector.GetTranscoderIp();
    m_uiPort = uiPort;
    
    // create our socket
    ok = m_Socket.Open(uiPort);
    if ( ok )
    {
        // start  thread;
        m_pThread = new std::thread(CCodecStream::Thread, this);
        m_bConnected = true;
    }
    else
    {
        std::cout << "Error opening socket on port UDP" << uiPort << " on ip " << g_Reflector.GetListenIp() << std::endl;
        m_bConnected = false;
    }
    
    // done
    return ok;
}

void CCodecStream::Close(void)
{
    // close socket
    m_bConnected = false;
    m_Socket.Close();
    
    // kill threads
    m_bStopThread = true;
    if ( m_pThread != NULL )
    {
        m_pThread->join();
        delete m_pThread;
        m_pThread = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// get

bool CCodecStream::IsEmpty(void) const
{
    return (m_LocalQueue.empty() && m_PacketStream->empty());
}

////////////////////////////////////////////////////////////////////////////////////////
// thread

void CCodecStream::Thread(CCodecStream *This)
{
    while ( !This->m_bStopThread )
    {
        This->Task();
    }
}

void CCodecStream::Task(void)
{
    CBuffer Buffer;
    CIp     Ip;
    uint8   Codec1;
    uint8   Codec2;
    uint8   Ambe1[AMBE_SIZE];
    uint8   Ambe2[AMBE_SIZE];
    uint8   DStarSync[] = { 0x55,0x2D,0x16 };
    
    // any packet from transcoder
    if ( m_Socket.Receive(&Buffer, &Ip, 5) != -1 )
    {
        // crack
        if ( IsValidAmbePacket(Buffer, &Codec1, Ambe1, &Codec2, Ambe2) )
        {
            // tickle
            m_TimeoutTimer.Now();
            
            // update statistics
            double ping = m_StatsTimer.DurationSinceNow();
            if ( m_fPingMin == -1 )
            {
                m_fPingMin = ping;
                m_fPingMax = ping;
                
            }
            else
            {
                m_fPingMin = MIN(m_fPingMin, ping);
                m_fPingMax = MAX(m_fPingMax, ping);
                
            }
            m_fPingSum += ping;
            m_fPingCount += 1;
            
            // pop the original packet
            if ( !m_LocalQueue.empty() )
            {
                CDvFramePacket *Packet = (CDvFramePacket *)m_LocalQueue.front();
                m_LocalQueue.pop();
                // todo: check the PID
                // update content with transcoded ambe
                Packet->SetAmbe(Codec1, Ambe1);
                Packet->SetAmbe(Codec2, Ambe2);
                // tag syncs in DvData
                if ( ((Codec1 == CODEC_AMBEPLUS) || (Codec2 == CODEC_AMBEPLUS)) && (Packet->GetPacketId() % 21) == 0 )
                {
                    Packet->SetDvData(DStarSync);
                }
                // and push it back to client
                m_PacketStream->Lock();
                m_PacketStream->push(Packet);
                m_PacketStream->Unlock();
            }
            else
            {
                std::cout << "Unexpected transcoded packet received from ambed" << std::endl;
            }
         }
    }
    
    // anything in our queue
    while ( !empty() )
    {
        // yes, pop it from queue
        CPacket *Packet = front();
        pop();
        
        // yes, send to ambed
        // this assume that thread pushing the Packet
        // have verified that the CodecStream is connected
        // and that the packet needs transcoding
        m_StatsTimer.Now();
        m_uiTotalPackets++;
        EncodeAmbePacket(&Buffer, ((CDvFramePacket *)Packet)->GetAmbe(m_uiCodecIn));
        m_Socket.Send(Buffer, m_Ip, m_uiPort);
       
        // and push to our local queue
        m_LocalQueue.push(Packet);
    }
    
    // handle timeout
    if ( !m_LocalQueue.empty() && (m_TimeoutTimer.DurationSinceNow() >= (TRANSCODER_AMBEPACKET_TIMEOUT/1000.0f)) )
    {
        //std::cout << "ambed packet timeout" << std::endl;
        m_uiTimeoutPackets++;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
/// packet decoding helpers

bool CCodecStream::IsValidAmbePacket(const CBuffer &Buffer, uint8 *Codec1, uint8 *Ambe1, uint8 *Codec2, uint8 *Ambe2)
{
    bool valid = false;
    
    if ( (Buffer.size() == 21) && ((Buffer.data()[0] | Buffer.data()[1]) == m_uiCodecsOut) )
    {
        *Codec1 = Buffer.data()[0];
        ::memcpy(Ambe1, &(Buffer.data()[3]), 9);
        *Codec2 = Buffer.data()[1];
        ::memcpy(Ambe2, &(Buffer.data()[12]), 9);
        valid = true;
    }
    return valid;
}

////////////////////////////////////////////////////////////////////////////////////////
/// packet encoding helpers

void CCodecStream::EncodeAmbePacket(CBuffer *Buffer, const uint8 *Ambe)
{
    Buffer->clear();
    Buffer->Append(m_uiCodecIn);
    Buffer->Append(m_uiPid);
    Buffer->Append((uint8 *)Ambe, 9);
}
