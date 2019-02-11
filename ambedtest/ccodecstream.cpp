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
#include "samples.h"
#include "ctranscoder.h"

////////////////////////////////////////////////////////////////////////////////////////
// define

////////////////////////////////////////////////////////////////////////////////////////
// constructor

CCodecStream::CCodecStream(uint16 uiId, uint8 uiCodecIn, uint8 uiCodecsOut)
{
    m_bStopThread = false;
    m_pThread = NULL;
    m_uiStreamId = uiId;
    m_uiPid = 0;
    m_uiCodecIn = uiCodecIn;
    m_uiCodecsOut = uiCodecsOut;
    m_bConnected = false;
    m_iAmbeSrcPtr = 0;
    m_iAmbeDestPtr = 0;
    m_uiNbTotalPacketSent = 0;
    m_uiNbPacketSent = 0;
    m_uiNbPacketReceived = 0;
    m_uiNbPacketBad = 0;
    m_uiNbPacketTimeout = 0;
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
}

////////////////////////////////////////////////////////////////////////////////////////
// initialization

bool CCodecStream::Init(uint16 uiPort)
{
    bool ok;
    
    // reset stop flag
    m_bStopThread = false;
    
    // copy our test data
    if ( m_uiCodecIn == CODEC_AMBE2PLUS )
    {
        // DMR -> DSTAR
        for ( int i = 0; i < sizeof(g_uiDmrSample); i += AMBE_SIZE )
        {
            CAmbe *ambe = new CAmbe(&(g_uiDmrSample[i]));
            m_AmbeSrc.push_back(ambe);
        }
        for ( int i = 0; i < sizeof(g_uiDstarSample); i += AMBE_SIZE )
        {
            CAmbe *ambe = new CAmbe(&(g_uiDstarSample[i]));
            m_AmbeDest.push_back(ambe);
        }
    }
    else
    {
        // DSTAR -> DMR
        for ( int i = 0; i < sizeof(g_uiDstarSample); i += AMBE_SIZE )
        {
            CAmbe *ambe = new CAmbe(&(g_uiDstarSample[i]));
            m_AmbeSrc.push_back(ambe);
        }
        for ( int i = 0; i < sizeof(g_uiDmrSample); i += AMBE_SIZE )
        {
            CAmbe *ambe = new CAmbe(&(g_uiDmrSample[i]));
            m_AmbeDest.push_back(ambe);
        }
    }
    
    // create server's IP
    m_Ip = g_Transcoder.GetAmbedIp();
    m_uiPort = uiPort;
    
    // create our socket
    ok = m_Socket.Open(uiPort);
    if ( ok )
    {
        // start  thread;
        m_pThread = new std::thread(CCodecStream::Thread, this);
        m_bConnected = true;
        m_FrameTimer.Now();
        m_uiNbTotalPacketSent = 0;
        ResetStats();
    }
    else
    {
        std::cout << "Error opening socket on port UDP" << uiPort << " on ip " << m_Ip << std::endl;
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
    
    // connected ?
    if ( m_bConnected )
    {
        // time to send next packet to be transcoded ?
        /*if ( m_FrameTimer.DurationSinceNow() >= 0.020 )
        {
            // yes
            m_FrameTimer.Now();
            
            // encode packet @ send it
            EncodeAmbePacket(&Buffer, m_AmbeSrc[m_iAmbeSrcPtr]->GetData());
            m_Socket.Send(Buffer, m_Ip, m_uiPort);
            
            // and increment pointer
            m_iAmbeSrcPtr = (m_iAmbeSrcPtr + 1) % m_AmbeSrc.size();
         m_uiNbTotalPacketSent++;
            m_uiNbPacketSent++;
        }*/
        
        // any packt to send to trancoder ?
        uint32 uiNbPacketToSend = (uint32)(m_FrameTimer.DurationSinceNow() * 50.0) - m_uiNbTotalPacketSent;
        if ( uiNbPacketToSend > 0 )
        {
            for ( int i = 0; i < uiNbPacketToSend; i++ )
            {
                // encode packet @ send it
                EncodeAmbePacket(&Buffer, m_AmbeSrc[m_iAmbeSrcPtr]->GetData());
                m_Socket.Send(Buffer, m_Ip, m_uiPort);
                
                // and increment pointer
                m_iAmbeSrcPtr = (m_iAmbeSrcPtr + 1) % m_AmbeSrc.size();
                m_uiNbTotalPacketSent++;
                m_uiNbPacketSent++;
            }
        }
        
        // any packet from transcoder
        if ( m_Socket.Receive(&Buffer, &Ip, 1) != -1 )
        {
            // crack
            if ( IsValidAmbePacket(Buffer, &Codec1, Ambe1, &Codec2, Ambe2) )
            {
                m_TimeoutTimer.Now();
                
                // check the PID
                // check the transcoded packet
                /*if ( ::memcmp(Ambe, m_AmbeDest[m_iAmbeDestPtr]->GetData(), AMBE_SIZE) != 0 )
                {
                    m_uiNbPacketBad++;
                    ::memcpy((void *)m_AmbeDest[m_iAmbeDestPtr]->GetData(), Ambe, AMBE_SIZE);
                }*/
                
                // and increment pointer
                m_iAmbeDestPtr = (m_iAmbeDestPtr + 1) % m_AmbeDest.size();
                m_uiNbPacketReceived++;
                
            }
        }
    }
    
    // display stats
    if ( m_DisplayStatsTimer.DurationSinceNow() >= 2.0 )
    {
        m_DisplayStatsTimer.Now();
        DisplayStats();
    }

    // handle timeout
    if ( m_TimeoutTimer.DurationSinceNow() >= (TRANSCODER_AMBEPACKET_TIMEOUT/1000.0f) )
    {
        //std::cout << "ambed packet timeout" << std::endl;
        m_uiNbPacketTimeout++;
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


////////////////////////////////////////////////////////////////////////////////////////
// stats helpers

void CCodecStream::ResetStats(void)
{
    m_StatsTimer.Now();
    m_DisplayStatsTimer.Now();
    m_TimeoutTimer.Now();
    m_uiNbPacketSent = 0;
    m_uiNbPacketReceived = 0;
    m_uiNbPacketBad = 0;
    m_uiNbPacketTimeout = 0;
    
}

void CCodecStream::DisplayStats(void)
{
    // get stats
    uint32 uiSent = m_uiNbPacketSent;
    uint32 uiReceived = m_uiNbPacketReceived;
    uint32 uiBad = m_uiNbPacketBad;
    double fps = (double)uiReceived / m_StatsTimer.DurationSinceNow();
    
    // resets
    ResetStats();
    
    // displays
    char sz[256];
    sprintf(sz, "Stream %d (%d->%d) : %u / %u / %u : %.1f fps",
            m_uiStreamId, m_uiCodecIn, m_uiCodecsOut,
            uiSent, uiReceived, uiBad, fps);
    std::cout << sz << std::endl;
}
