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
#include <cstring>
#include "creflector.h"
#include "ctranscoder.h"

////////////////////////////////////////////////////////////////////////////////////////
// define

// status
#define STATUS_IDLE                 0
#define STATUS_LOGGED               1

// timeout — must accommodate remote AMBEd with 50-200ms network RTT
#define AMBED_OPENSTREAM_TIMEOUT    1000    // in ms

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
    m_bWaitingForStream = false;
    m_StreamidOpenStream = 0;
    m_PortOpenStream = 0;
}

////////////////////////////////////////////////////////////////////////////////////////
// destructor

CTranscoder::~CTranscoder()
{
    Close();
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
    // stop the transcoder task thread first so it doesn't access
    // streams or socket while we're tearing them down
    m_bStopThread = true;
    if ( m_pThread != NULL )
    {
        m_pThread->join();
        delete m_pThread;
        m_pThread = NULL;
    }

    // close socket (no more UDP traffic)
    m_Socket.Close();

    // close and delete all codec streams
    m_Mutex.lock();
    {
        for ( size_t i = 0; i < m_Streams.size(); i++ )
        {
            delete m_Streams[i];
        }
        m_Streams.clear();
    }
    m_Mutex.unlock();
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
            if ( m_bWaitingForStream )
            {
                // Normal path: GetStream is waiting for this response
                m_bStreamOpened = true;
                m_StreamidOpenStream = StreamId;
                m_PortOpenStream = Port;
                m_SemaphoreOpenStream.Notify();
            }
            else
            {
                // Late response — GetStream already timed out.
                // Close the orphaned AMBEd channel to prevent hardware exhaustion.
                CBuffer closeBuf;
                EncodeClosestreamPacket(&closeBuf, StreamId);
                m_Socket.Send(closeBuf, m_Ip, TRANSCODER_PORT);
                std::cout << "ambed late response for stream " << (int)StreamId
                          << " on port " << (int)Port
                          << " — sent close to free hardware channel" << std::endl;
            }
        }
        else if ( IsValidNoStreamAvailablePacket(Buffer) )
        {
            if ( m_bWaitingForStream )
            {
                m_bStreamOpened = false;
                m_SemaphoreOpenStream.Notify();
            }
            // else: late BUSY response after timeout — discard
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
            // determine output codecs based on input
            uint8 uiCodecOut1, uiCodecOut2, uiCodecOut3;
            switch ( uiCodecIn )
            {
                case CODEC_AMBEPLUS:    // D-Star -> DMR + M17 + P25
                    uiCodecOut1 = CODEC_AMBE2PLUS;
                    uiCodecOut2 = CODEC_CODEC2;
                    uiCodecOut3 = CODEC_IMBE;
                    break;
                case CODEC_AMBE2PLUS:   // DMR -> D-Star + M17 + P25
                    uiCodecOut1 = CODEC_AMBEPLUS;
                    uiCodecOut2 = CODEC_CODEC2;
                    uiCodecOut3 = CODEC_IMBE;
                    break;
                case CODEC_CODEC2:      // M17 -> D-Star + DMR + P25
                    uiCodecOut1 = CODEC_AMBEPLUS;
                    uiCodecOut2 = CODEC_AMBE2PLUS;
                    uiCodecOut3 = CODEC_IMBE;
                    break;
                case CODEC_IMBE:        // P25 -> D-Star + DMR + M17
                    uiCodecOut1 = CODEC_AMBEPLUS;
                    uiCodecOut2 = CODEC_AMBE2PLUS;
                    uiCodecOut3 = CODEC_CODEC2;
                    break;
                default:
                    return NULL;
            }

            // Serialize stream open requests — the shared m_bStreamOpened /
            // m_StreamidOpenStream / m_PortOpenStream fields are single-use,
            // so concurrent callers must wait their turn
            Lock();

            // send openstream request
            m_bStreamOpened = false;
            m_bWaitingForStream = true;
            EncodeOpenstreamPacket(&Buffer, uiCodecIn, uiCodecOut1, uiCodecOut2, uiCodecOut3);
            m_SemaphoreOpenStream.PreWaitFor();
            m_Socket.Send(Buffer, m_Ip, TRANSCODER_PORT);

            // wait for AMBEd response (holding mutex blocks other GetStream callers)
            bool gotResponse = m_SemaphoreOpenStream.WaitFor(AMBED_OPENSTREAM_TIMEOUT);

            // If primary wait timed out, give a brief second chance to catch
            // responses that were in-flight. This preserves the semaphore's
            // happens-before ordering for m_StreamidOpenStream/m_PortOpenStream
            // (reading those fields without the semaphore signal is a data race).
            if ( !gotResponse )
            {
                gotResponse = m_SemaphoreOpenStream.WaitFor(50);
            }

            // Clear waiting flag — any later response from Task() will be
            // detected as orphaned and closed via AMBEDCS
            m_bWaitingForStream = false;

            if ( gotResponse && m_bStreamOpened )
            {
                std::cout << "ambed stream open on port " << m_PortOpenStream << std::endl;

                stream = new CCodecStream(PacketStream, m_StreamidOpenStream, uiCodecIn, uiCodecOut1, uiCodecOut2, uiCodecOut3);

                if ( stream->Init(m_PortOpenStream) )
                {
                    m_Streams.push_back(stream);
                }
                else
                {
                    EncodeClosestreamPacket(&Buffer, stream->GetStreamId());
                    m_Socket.Send(Buffer, m_Ip, TRANSCODER_PORT);
                    delete stream;
                    stream = NULL;
                }
            }
            else if ( gotResponse && !m_bStreamOpened )
            {
                std::cout << "ambed openstream failed (no suitable channel available)" << std::endl;
            }
            else
            {
                std::cout << "ambed openstream timeout" << std::endl;
            }

            Unlock();
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
            for ( size_t i = 0; (i < m_Streams.size()) && !found; i++ )
            {
                // compare object pointers
                if ( (m_Streams[i]) ==  stream )
                {
                    // send close packet
                    EncodeClosestreamPacket(&Buffer, m_Streams[i]->GetStreamId());
                    m_Socket.Send(Buffer, m_Ip, TRANSCODER_PORT);
                    
                    // and close it. Close() sets m_bStopThread on the
                    // codec stream and joins its Task thread. After
                    // Close() returns, the codec thread is no longer
                    // touching any of the stream's stats fields, so
                    // they're safe to read without locking. (Reading
                    // them BEFORE Close() would race the still-running
                    // Task() thread which writes m_uiReturnedPackets,
                    // m_uiResponseLookupMisses, m_uiUnfilledReleases,
                    // m_uiOverrunDrops, and the m_TargetMin/Max/Sum/
                    // Samples set every recompute — none of those are
                    // atomic.)
                    m_Streams[i]->Close();

                    // display stats:
                    //   ping min/avg/max — round-trip time to ambed
                    //   sent / returned — packets sent to ambed / popped
                    //                     from jitter (should match)
                    //   late — ambed responses that arrived after the
                    //          jitter timer had already released the
                    //          packet (LAN return-path loss or ambed
                    //          RTT > current jitter target)
                    //   unfilled — jitter pops where ambed never
                    //              responded at all (LAN forward-path
                    //              loss or ambed offline)
                    //   jitter min/avg/max — adaptive jitter delay
                    //                        target the buffer self-tuned
                    //                        to during the stream (in ms)
                    //   drops — overrun protection drops (buffer
                    //           bloated > 1.5x target, oldest packets
                    //           dropped to recover cadence)
                    //
                    // late/unfilled/drops should all be zero in a
                    // healthy deployment. Non-zero late/unfilled values
                    // mean some other-mode listeners heard ~20 ms of
                    // silence per affected frame; D-Star pass-through
                    // audio is unaffected by either. Non-zero drops
                    // means a sustained input overrun (bursty source
                    // exceeding ambed throughput); listeners hear a
                    // brief cadence stutter per drop.
                    {
                        char sz[640];
                        uint32 sent = m_Streams[i]->GetTotalPackets();
                        uint32 returned = m_Streams[i]->GetReturnedPackets();
                        uint32 lookupMisses = m_Streams[i]->GetResponseLookupMisses();
                        uint32 unfilled = m_Streams[i]->GetUnfilledReleases();
                        unsigned int jitterMin = m_Streams[i]->GetJitterTargetMin();
                        unsigned int jitterAvg = m_Streams[i]->GetJitterTargetAvg();
                        unsigned int jitterMax = m_Streams[i]->GetJitterTargetMax();
                        uint32 drops = m_Streams[i]->GetOverrunDrops();
                        // Diagnostic instrumentation — see CCodecStream
                        // getter comments. peak = max jitter buffer
                        // occupancy this stream; arrival_max = longest
                        // admitted source inter-arrival; first/last =
                        // m_uiTotalPackets at first/last overrun drop.
                        uint32 peak = m_Streams[i]->GetPeakBufferOccupancy();
                        uint32 arrivalMax = m_Streams[i]->GetArrivalMaxMs();
                        uint32 firstDrop = m_Streams[i]->GetFirstDropFrame();
                        uint32 lastDrop = m_Streams[i]->GetLastDropFrame();
                        snprintf(sz, sizeof(sz),
                                "ambed stats (ms) : %.1f/%.1f/%.1f — %d sent, %d returned, "
                                "%d late, %d unfilled, jitter %u/%u/%u ms, %u drops "
                                "[peak=%u arrival_max=%ums drops@first=%u/last=%u]",
                                m_Streams[i]->GetPingMin() * 1000.0,
                                m_Streams[i]->GetPingAve() * 1000.0,
                                m_Streams[i]->GetPingMax() * 1000.0,
                                sent, returned, lookupMisses, unfilled,
                                jitterMin, jitterAvg, jitterMax, drops,
                                peak, arrivalMax, firstDrop, lastDrop);
                        std::cout << sz << std::endl;
                    }

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
        ::memcpy(Id, &Buffer.data()[8], sizeof(uint16));
        ::memcpy(Port, &Buffer.data()[10], sizeof(uint16));
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

void CTranscoder::EncodeOpenstreamPacket(CBuffer *Buffer, uint8 uiCodecIn, uint8 uiCodecOut1, uint8 uiCodecOut2, uint8 uiCodecOut3)
{
    uint8 tag[] = { 'A','M','B','E','D','O','S' };

    Buffer->Set(tag, sizeof(tag));
    Buffer->Append((uint8 *)(const char *)g_Reflector.GetCallsign(), CALLSIGN_LEN);
    Buffer->Append((uint8)uiCodecIn);
    Buffer->Append((uint8)uiCodecOut1);
    Buffer->Append((uint8)uiCodecOut2);
    Buffer->Append((uint8)uiCodecOut3);
}

void CTranscoder::EncodeClosestreamPacket(CBuffer *Buffer, uint16 uiStreamId)
{
    uint8 tag[] = { 'A','M','B','E','D','C','S' };

    Buffer->Set(tag, sizeof(tag));
    Buffer->Append((uint16)uiStreamId);
}

////////////////////////////////////////////////////////////////////////////////////////
