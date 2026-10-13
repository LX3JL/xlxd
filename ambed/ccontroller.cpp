//
//  ccontroller.cpp
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
#include <cstring>
#include "ctimepoint.h"
#include "cvocodecs.h"
#include "ccontroller.h"


////////////////////////////////////////////////////////////////////////////////////////
// constructor

CController::CController()
{
    m_bStopThread = false;
    m_pThread = NULL;
    m_Ip = CIp("127.0.0.1");
    m_uiLastStreamId = 0;
    m_uiMaxStreams = 0;
}

////////////////////////////////////////////////////////////////////////////////////////
// destructor

CController::~CController()
{
    Close();
}

////////////////////////////////////////////////////////////////////////////////////////
// initialization

bool CController::Init(void)
{
    bool ok;

    // reset stop flag
    m_bStopThread = false;

    // calculate max streams from available vocoder channels
    // Codec2/IMBE input modes use 2 channels per stream, so divide by 2
    int nbChannels = g_Vocodecs.GetNbChannels();
    m_uiMaxStreams = (nbChannels > 0) ? (nbChannels / 2) : 0;
    if ( m_uiMaxStreams == 0 && nbChannels > 0 )
    {
        m_uiMaxStreams = 1;  // at least 1 stream if we have any channels
    }
    std::cout << "Max concurrent streams: " << m_uiMaxStreams << " (from " << nbChannels << " vocoder channels)" << std::endl;

    // create our socket
    ok = m_Socket.Open(m_Ip, TRANSCODER_PORT);
    if ( ok )
    {
        // start  thread;
        m_pThread = new std::thread(CController::Thread, this);
    }
    else
    {
        std::cout << "Error opening socket on port UDP" << TRANSCODER_PORT << " on ip " << m_Ip << std::endl;
    }

    // done
    return ok;
}

void CController::Close(void)
{
    // Idempotent: if thread already stopped, Close() was already called
    if ( m_pThread == NULL )
        return;

    m_bStopThread = true;
    m_Socket.Close();
    m_pThread->join();
    delete m_pThread;
    m_pThread = NULL;

    // Close all streams now that the controller thread is stopped
    for ( int i = 0; i < m_Streams.size(); i++ )
    {
        delete m_Streams[i];
    }
    m_Streams.clear();
}

////////////////////////////////////////////////////////////////////////////////////////
// thread

void CController::Thread(CController *This)
{
    while ( !This->m_bStopThread )
    {
        This->Task();
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// task

void CController::Task(void)
{
    CBuffer     Buffer;
    CIp         Ip;
    CCallsign   Callsign;
    uint8       CodecIn;
    uint8       CodecOut1;
    uint8       CodecOut2;
    uint8       CodecOut3;
    uint16      StreamId;
    CStream     *Stream;

    // anything coming in from codec client ?
    if ( m_Socket.Receive(&Buffer, &Ip, 20) != -1 )
    {
        // crack packet
        if ( IsValidOpenstreamPacket(Buffer, &Callsign, &CodecIn, &CodecOut1, &CodecOut2, &CodecOut3) )
        {
            std::cout << "Stream Open from " << Callsign << " at " << Ip
                      << " CodecIn=" << (int)CodecIn
                      << " Out1=" << (int)CodecOut1
                      << " Out2=" << (int)CodecOut2
                      << " Out3=" << (int)CodecOut3 << std::endl;

            // try create the stream
            Stream = OpenStream(Callsign, Ip, CodecIn, CodecOut1, CodecOut2, CodecOut3);
            
            // send back details
            if ( Stream != NULL )
            {
                EncodeStreamDescrPacket(&Buffer, *Stream);
            }
            else
            {
                EncodeNoStreamAvailablePacket(&Buffer);
            }
            m_Socket.Send(Buffer, Ip);
        }
        else if ( IsValidClosestreamPacket(Buffer, &StreamId) )
        {
            // close the stream
            CloseStream(StreamId);
            
            std::cout << "Stream " << (int)StreamId << " closed" << std::endl;            
        }
        else if ( IsValidKeepAlivePacket(Buffer, &Callsign) )
        {
            //std::cout << "ping - " << Callsign << std::endl;
            // pong back
            EncodeKeepAlivePacket(&Buffer);
            m_Socket.Send(Buffer, Ip);
        }
    }
    
    
    // HandleTimout/keepalive
    bool timeout;
    do
    {
        // init loop stuffs
        timeout = false;
        CStream *stream = NULL;
        
        // any inactive streams?
        Lock();
        {
            for ( int i = 0; (i < m_Streams.size()) && !timeout; i++ )
            {
                if ( !(m_Streams[i]->IsActive()) )
                {
                    timeout = true;
                    stream = m_Streams[i];
                    std::cout << "Stream " << (int)m_Streams[i]->GetId() << " activity timeout " << std::endl;
                }
            }
        }
        Unlock();
        
        // if any streams timeout, close it
        // this cannot be done in above loop as it suppress it from array
        if ( timeout )
        {
            CloseStream(stream);
        }
    } while (timeout);
}

////////////////////////////////////////////////////////////////////////////////////////
// streams management

CStream *CController::OpenStream(const CCallsign &Callsign, const CIp &Ip, uint8 CodecIn, uint8 CodecOut1, uint8 CodecOut2, uint8 CodecOut3)
{
    CStream *stream = NULL;

    // check capacity
    if ( m_Streams.size() >= (size_t)m_uiMaxStreams )
    {
        std::cout << "Max streams reached (" << m_uiMaxStreams << "), rejecting" << std::endl;
        return NULL;
    }

    // create a new stream
    m_uiLastStreamId = (m_uiLastStreamId + 1);
    m_uiLastStreamId = (m_uiLastStreamId > m_uiMaxStreams) ? 1 : m_uiLastStreamId;
    stream = new CStream(m_uiLastStreamId, Callsign, Ip, CodecIn, CodecOut1, CodecOut2, CodecOut3);
    if ( stream->Init(TRANSCODER_PORT+m_uiLastStreamId) )
    {
        std::cout << "Opened stream " << m_uiLastStreamId << std::endl;
        // and store it
        Lock();
        m_Streams.push_back(stream);
        Unlock();
    }
    else
    {
        delete stream;
        stream = NULL;
    }

    // done
    return stream;
}

void CController::CloseStream(CStream *stream)
{
    CStream *found = NULL;
    Lock();
    {
        for ( int i = 0; i < m_Streams.size(); i++ )
        {
            if ( m_Streams[i] == stream )
            {
                found = m_Streams[i];
                m_Streams.erase(m_Streams.begin()+i);
                break;
            }
        }
    }
    Unlock();

    // Close and delete outside the lock to avoid blocking other stream operations
    if ( found != NULL )
    {
        found->Close();
        delete found;
    }
}

void CController::CloseStream(uint16 StreamId)
{
    CStream *found = NULL;
    Lock();
    {
        for ( int i = 0; i < m_Streams.size(); i++ )
        {
            if ( m_Streams[i]->GetId() == StreamId )
            {
                found = m_Streams[i];
                m_Streams.erase(m_Streams.begin()+i);
                break;
            }
        }
    }
    Unlock();

    // Close and delete outside the lock to avoid blocking other stream operations
    if ( found != NULL )
    {
        found->Close();
        delete found;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// packet decoding helpers

bool CController::IsValidKeepAlivePacket(const CBuffer &Buffer, CCallsign *Callsign)
{
    uint8 tag[] = { 'A','M','B','E','D','P','I','N','G' };
    
    bool valid = false;
    if ( (Buffer.size() == 17) && (Buffer.Compare(tag, sizeof(tag)) == 0) )
    {
        // get callsign here
        Callsign->SetCallsign(&(Buffer.data()[9]), 8);
        valid = Callsign->IsValid();
    }
    return valid;
}

bool CController::IsValidOpenstreamPacket(const CBuffer &Buffer, CCallsign *Callsign, uint8 *CodecIn, uint8 *CodecOut1, uint8 *CodecOut2, uint8 *CodecOut3)
{
    uint8 tag[] = { 'A','M','B','E','D','O','S' };

    bool valid = false;
    // New format: 19 bytes with 3 output codecs
    if ( (Buffer.size() == 19) && (Buffer.Compare(tag, sizeof(tag)) == 0) )
    {
        // get callsign here
        Callsign->SetCallsign(&(Buffer.data()[7]), 8);
        *CodecIn = Buffer.data()[15];
        *CodecOut1 = Buffer.data()[16];
        *CodecOut2 = Buffer.data()[17];
        *CodecOut3 = Buffer.data()[18];

        // valid ?
        valid = Callsign->IsValid() && IsValidCodecIn(*CodecIn) && IsValidCodecOut(*CodecOut1) && IsValidCodecOut(*CodecOut2);
        // CodecOut3 is optional (can be CODEC_NONE)
        if (*CodecOut3 != CODEC_NONE)
        {
            valid = valid && IsValidCodecOut(*CodecOut3);
        }
    }
    // 2 output codecs format: 18 bytes
    else if ( (Buffer.size() == 18) && (Buffer.Compare(tag, sizeof(tag)) == 0) )
    {
        // get callsign here
        Callsign->SetCallsign(&(Buffer.data()[7]), 8);
        *CodecIn = Buffer.data()[15];
        *CodecOut1 = Buffer.data()[16];
        *CodecOut2 = Buffer.data()[17];
        *CodecOut3 = CODEC_NONE;  // no third output codec

        // valid ?
        valid = Callsign->IsValid() && IsValidCodecIn(*CodecIn) && IsValidCodecOut(*CodecOut1) && IsValidCodecOut(*CodecOut2);
    }
    // Legacy format: 17 bytes with 1 output codec (backward compatibility)
    else if ( (Buffer.size() == 17) && (Buffer.Compare(tag, sizeof(tag)) == 0) )
    {
        // get callsign here
        Callsign->SetCallsign(&(Buffer.data()[7]), 8);
        *CodecIn = Buffer.data()[15];
        *CodecOut1 = Buffer.data()[16];
        *CodecOut2 = CODEC_NONE;  // no second output codec
        *CodecOut3 = CODEC_NONE;  // no third output codec

        // valid ?
        valid = Callsign->IsValid() && IsValidCodecIn(*CodecIn) && IsValidCodecOut(*CodecOut1);
    }
    return valid;
}

bool CController::IsValidClosestreamPacket(const CBuffer &Buffer, uint16 *StreamId)
{
    uint8 tag[] = { 'A','M','B','E','D','C','S' };
    
    bool valid = false;
    if ( (Buffer.size() >= 9) && (Buffer.Compare(tag, sizeof(tag)) == 0) )
    {
        // get stream id
        ::memcpy(StreamId, &Buffer.data()[7], sizeof(uint16));
        valid = true;
    }
    return valid;
}

////////////////////////////////////////////////////////////////////////////////////////
// packet encoding helpers

void CController::EncodeKeepAlivePacket(CBuffer *Buffer)
{
    uint8 tag[] = { 'A','M','B','E','D','P','O','N','G' };
    
    Buffer->Set(tag, sizeof(tag));
}

void CController::EncodeStreamDescrPacket(CBuffer *Buffer, const CStream &Stream)
{
    uint8 tag[] = { 'A','M','B','E','D','S','T','D' };
    
    Buffer->Set(tag, sizeof(tag));
    // id
    Buffer->Append((uint16)Stream.GetId());
    // port
    Buffer->Append((uint16)Stream.GetPort());
    // codecin
    Buffer->Append((uint8)Stream.GetCodecIn());
    // codecout
    Buffer->Append((uint8)Stream.GetCodecOut());
}

void CController::EncodeNoStreamAvailablePacket(CBuffer *Buffer)
{
    uint8 tag[] = { 'A','M','B','E','D','B','U','S','Y' };
    
    Buffer->Set(tag, sizeof(tag));
}


////////////////////////////////////////////////////////////////////////////////////////
// codec helpers

bool CController::IsValidCodecIn(uint8 codec)
{
    return ((codec == CODEC_AMBEPLUS) || (codec == CODEC_AMBE2PLUS) || (codec == CODEC_CODEC2) || (codec == CODEC_IMBE));
}

bool CController::IsValidCodecOut(uint8 codec)
{
    return ((codec == CODEC_AMBEPLUS) || (codec == CODEC_AMBE2PLUS) || (codec == CODEC_CODEC2) || (codec == CODEC_IMBE));
}

