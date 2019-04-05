//
//  ctranscoder.cpp
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
#include "creflector.h"
#include "ctranscoder.h"

////////////////////////////////////////////////////////////////////////////////////////
// define

// status
#define STATUS_IDLE                 0
#define STATUS_LOGGED               1

// timeout
#define AMBED_OPENSTREAM_TIMEOUT    200     // in ms

////////////////////////////////////////////////////////////////////////////////////////

CTranscoder g_Transcoder;


////////////////////////////////////////////////////////////////////////////////////////
// constructor

CTranscoder::CTranscoder()
{
    m_bStopThread = false;
    m_pThread = NULL;
    m_Streams.reserve(12);
    m_bConnected = false;
    m_LastKeepaliveTime.Now();
    m_LastActivityTime.Now();
    m_bStreamOpened = false;
    m_StreamidOpenStream = 0;
    m_PortOpenStream = 0;
}

////////////////////////////////////////////////////////////////////////////////////////
// destructor

CTranscoder::~CTranscoder()
{
    // close all streams
    m_Mutex.lock();
    {
        for ( int i = 0; i < m_Streams.size(); i++ )
        {
            delete m_Streams[i];
        }
        m_Streams.clear();
        
    }
    m_Mutex.unlock();
    
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

bool CTranscoder::Init(void)
{
    bool ok;
    
    // reset stop flag
    m_bStopThread = false;

    // create server's IP
    m_Ip = g_Reflector.GetTranscoderIp();
    
    // create our socket
    ok = m_Socket.Open(TRANSCODER_PORT);
    if ( ok )
    {
        // start  thread;
        m_pThread = new std::thread(CTranscoder::Thread, this);
    }
    else
    {
        std::cout << "Error opening socket on port UDP" << TRANSCODER_PORT << " on ip " << g_Reflector.GetListenIp() << std::endl;
    }

    // done
    return ok;
}

void CTranscoder::Close(void)
{
    // close socket
    m_Socket.Close();
    
    // close all streams
    m_Mutex.lock();
    {
        for ( int i = 0; i < m_Streams.size(); i++ )
        {
            delete m_Streams[i];
        }
        m_Streams.clear();
        
    }
    m_Mutex.unlock();
    
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

void CTranscoder::Thread(CTranscoder *This)
{
    while ( !This->m_bStopThread )
    {
        This->Task();
    }
}

void CTranscoder::Task(void)
{
    CBuffer     Buffer;
    CIp         Ip;
    uint16      StreamId;
    uint16      Port;
    
    // anything coming in from codec server ?
    //if ( (m_Socket.Receive(&Buffer, &Ip, 20) != -1) && (Ip == m_Ip) )
    if ( m_Socket.Receive(&Buffer, &Ip, 20) != -1 )
    {
        m_LastActivityTime.Now();
        
        // crack packet
        if ( IsValidStreamDescrPacket(Buffer, &StreamId, &Port) )
        {
            //std::cout << "Transcoder stream " << (int) StreamId << " descr packet " << std::endl;
            m_bStreamOpened = true;
            m_StreamidOpenStream = StreamId;
            m_PortOpenStream = Port;
            m_SemaphoreOpenStream.Notify();
        }
        else if ( IsValidNoStreamAvailablePacket(Buffer) )
        {
            m_bStreamOpened = false;
            m_SemaphoreOpenStream.Notify();
        }
        else if ( IsValidKeepAlivePacket(Buffer) )
        {
            if ( !m_bConnected )
            {
                std::cout << "Transcoder connected at " << Ip << std::endl;
            }
            m_bConnected = true;
        }
        
    }
    
    // keep client alive
    if ( m_LastKeepaliveTime.DurationSinceNow() > TRANSCODER_KEEPALIVE_PERIOD )
    {
        //
        HandleKeepalives();
        
        // update time
        m_LastKeepaliveTime.Now();
    }
 }

////////////////////////////////////////////////////////////////////////////////////////
// manage streams

CCodecStream *CTranscoder::GetStream(CPacketStream *PacketStream, uint8 uiCodecIn)
{
    CBuffer     Buffer;
    
    CCodecStream *stream = NULL;
    
    // do we need transcoding
    if ( uiCodecIn != CODEC_NONE )
    {
        // are we connected to server
        if ( m_bConnected )
        {
            // yes, post openstream request
            EncodeOpenstreamPacket(&Buffer, uiCodecIn, GetCodecsOut(uiCodecIn));
            m_Socket.Send(Buffer, m_Ip, TRANSCODER_PORT);
            
            // wait relpy here
            if ( m_SemaphoreOpenStream.WaitFor(AMBED_OPENSTREAM_TIMEOUT) )
            {
                if ( m_bStreamOpened )
                {
                    std::cout << "ambed openstream ok" << std::endl;
                
                    // create stream object
                    stream = new CCodecStream(PacketStream, m_StreamidOpenStream, uiCodecIn, GetCodecsOut(uiCodecIn));
                    
                    // init it
                    if ( stream->Init(m_PortOpenStream) )
                    {
                        // and append to list
                        Lock();
                        m_Streams.push_back(stream);
                        Unlock();
                    }
                    else
                    {
                        // send close packet
                        EncodeClosestreamPacket(&Buffer, stream->GetStreamId());
                        m_Socket.Send(Buffer, m_Ip, TRANSCODER_PORT);
                        // and delete
                        delete stream;
                        stream = NULL;
                    }
                }
                else
                {
                    std::cout << "ambed openstream failed (no suitable channel available)" << std::endl;
                }
            }
            else
            {
                std::cout << "ambed openstream timeout" << std::endl;
            }
            
        }
    }
    return stream;
}

void CTranscoder::ReleaseStream(CCodecStream *stream)
{
    CBuffer Buffer;
    
    if ( stream != NULL )
    {
        // look for the stream
        bool found = false;
        Lock();
        {
            for ( int i = 0; (i < m_Streams.size()) && !found; i++ )
            {
                // compare object pointers
                if ( (m_Streams[i]) ==  stream )
                {
                    // send close packet
                    EncodeClosestreamPacket(&Buffer, m_Streams[i]->GetStreamId());
                    m_Socket.Send(Buffer, m_Ip, TRANSCODER_PORT);
                    
                    // display stats
                    if ( m_Streams[i]->GetPingMin() >= 0.0 )
                    {
                        char sz[256];
                        sprintf(sz, "ambed stats (ms) : %.1f/%.1f/%.1f",
                                m_Streams[i]->GetPingMin() * 1000.0,
                                m_Streams[i]->GetPingAve() * 1000.0,
                                m_Streams[i]->GetPingMax() * 1000.0);
                        std::cout << sz << std::endl;
                    }
                    if ( m_Streams[i]->GetTimeoutPackets() > 0 )
                    {
                        char sz[256];
                        sprintf(sz, "ambed %d of %d packets timed out",
                                m_Streams[i]->GetTimeoutPackets(),
                                m_Streams[i]->GetTotalPackets());
                        std::cout << sz << std::endl;
                    }

                    // and close it
                    m_Streams[i]->Close();
                    delete m_Streams[i];
                    m_Streams.erase(m_Streams.begin()+i);
                    found = true;
                }
            }
        }
        Unlock();
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// codec helpers

uint8 CTranscoder::GetCodecsOut(uint8 CodecIn)
{
    switch (CodecIn) {
        case CODEC_AMBEPLUS:    return CODEC_AMBE2PLUS | CODEC_CODEC2_3200;
        case CODEC_AMBE2PLUS:   return CODEC_AMBEPLUS | CODEC_CODEC2_3200;
        case CODEC_CODEC2_3200:
        case CODEC_CODEC2_2400: return CODEC_AMBEPLUS | CODEC_AMBE2PLUS;
        default:                return CODEC_NONE;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// keepalive helpers

void CTranscoder::HandleKeepalives(void)
{
    CBuffer keepalive;
    
    // send keepalive
    EncodeKeepAlivePacket(&keepalive);
    m_Socket.Send(keepalive, m_Ip, TRANSCODER_PORT);
    
    // check if still with us
    if ( m_bConnected && (m_LastActivityTime.DurationSinceNow() >= TRANSCODER_KEEPALIVE_TIMEOUT) )
    {
        // no, disconnect
        m_bConnected = false;
        std::cout << "Transcoder keepalive timeout" << std::endl;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// packet decoding helpers

bool CTranscoder::IsValidKeepAlivePacket(const CBuffer &Buffer)
{
    uint8 tag[] = { 'A','M','B','E','D','P','O','N','G' };
    
    bool valid = false;
    if ( (Buffer.size() == 9) && (Buffer.Compare(tag, sizeof(tag)) == 0) )
    {
        valid = true;
    }
    return valid;
}

bool CTranscoder::IsValidStreamDescrPacket(const CBuffer &Buffer, uint16 *Id, uint16 *Port)
{
    uint8 tag[] = { 'A','M','B','E','D','S','T','D' };
    
    bool valid = false;
    if ( (Buffer.size() == 14) && (Buffer.Compare(tag, sizeof(tag)) == 0) )
    {
        *Id = *(uint16 *)(&Buffer.data()[8]);
        *Port = *(uint16 *)(&Buffer.data()[10]);
        // uint8 CodecIn = Buffer.data()[12];
        // uint8 CodecOut = Buffer.data()[13];
        valid = true;
    }
    return valid;
}

bool CTranscoder::IsValidNoStreamAvailablePacket(const CBuffer&Buffer)
{
    uint8 tag[] = { 'A','M','B','E','D','B','U','S','Y' };
    
    return  ( (Buffer.size() == 9) && (Buffer.Compare(tag, sizeof(tag)) == 0) );
}


////////////////////////////////////////////////////////////////////////////////////////
// packet encoding helpers

void CTranscoder::EncodeKeepAlivePacket(CBuffer *Buffer)
{
    uint8 tag[] = { 'A','M','B','E','D','P','I','N','G' };
    
    Buffer->Set(tag, sizeof(tag));
    Buffer->Append((uint8 *)(const char *)g_Reflector.GetCallsign(), CALLSIGN_LEN);
}

void CTranscoder::EncodeOpenstreamPacket(CBuffer *Buffer, uint8 uiCodecIn, uint8 uiCodecsOut)
{
    uint8 tag[] = { 'A','M','B','E','D','O','S' };

    Buffer->Set(tag, sizeof(tag));
    Buffer->Append((uint8 *)(const char *)g_Reflector.GetCallsign(), CALLSIGN_LEN);
    Buffer->Append((uint8)uiCodecIn);
    Buffer->Append((uint8)uiCodecsOut);
}

void CTranscoder::EncodeClosestreamPacket(CBuffer *Buffer, uint16 uiStreamId)
{
    uint8 tag[] = { 'A','M','B','E','D','C','S' };
    
    Buffer->Set(tag, sizeof(tag));
    Buffer->Append((uint16)uiStreamId);
}

