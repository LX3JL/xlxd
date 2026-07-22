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
#include <math.h>
#include "ctimepoint.h"
#include "cambeserver.h"
#include "cvocodecs.h"
#include "cambepacket.h"
#include "cvoicepacket.h"
#include "cstream.h"

////////////////////////////////////////////////////////////////////////////////////////
// define

////////////////////////////////////////////////////////////////////////////////////////
// constructor

CStream::CStream()
{
    m_uiId = 0;
    m_uiPort = 0;
    m_uiCodecIn = CODEC_NONE;
    m_uiCodecOut1 = CODEC_NONE;
    m_uiCodecOut2 = CODEC_NONE;
    m_uiCodecOut3 = CODEC_NONE;
    m_bStopThread = false;
    m_pThread = NULL;
    m_VocodecChannel = NULL;
    m_EncoderChannel1 = NULL;
    m_EncoderChannel2 = NULL;
    m_pCodec2Decoder = NULL;
    m_pSignalProcessor1 = NULL;
    m_pSignalProcessor2 = NULL;
    m_pImbeEncoder = NULL;
    m_pImbeDecoder = NULL;
    m_pCodec2Encoder = NULL;
    m_fCodec2DecodeGain = 1.0f;
    m_fImbeGain = 1.0f;
    m_fCodec2Gain = 1.0f;
    m_pStagedPacket = NULL;
    m_LastActivity.Now();
    m_iTotalPackets = 0;
    m_iLostPackets = 0;
    m_nImbeQueueSize = 0;
    m_nCodec2QueueSize = 0;
}

CStream::CStream(uint16 uiId, const CCallsign &Callsign, const CIp &Ip, uint8 uiCodecIn, uint8 uiCodecOut1, uint8 uiCodecOut2, uint8 uiCodecOut3)
{
    m_uiId = uiId;
    m_Callsign = Callsign;
    m_Ip = Ip;
    m_uiPort = 0;
    m_uiCodecIn = uiCodecIn;
    m_uiCodecOut1 = uiCodecOut1;
    m_uiCodecOut2 = uiCodecOut2;
    m_uiCodecOut3 = uiCodecOut3;
    m_bStopThread = false;
    m_pThread = NULL;
    m_VocodecChannel = NULL;
    m_EncoderChannel1 = NULL;
    m_EncoderChannel2 = NULL;
    m_pCodec2Decoder = NULL;
    m_pSignalProcessor1 = NULL;
    m_pSignalProcessor2 = NULL;
    m_pImbeEncoder = NULL;
    m_pImbeDecoder = NULL;
    m_pCodec2Encoder = NULL;
    m_fCodec2DecodeGain = 1.0f;
    m_fImbeGain = 1.0f;
    m_fCodec2Gain = 1.0f;
    m_LastActivity.Now();
    m_iTotalPackets = 0;
    m_iLostPackets = 0;
    m_pStagedPacket = NULL;
    m_nImbeQueueSize = 0;
    m_nCodec2QueueSize = 0;
}

////////////////////////////////////////////////////////////////////////////////////////
// destructor

CStream::~CStream()
{
    Close();
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
        // Codec2 input mode: need two encoder channels + software decoder + software IMBE encoder
        if (m_uiCodecIn == CODEC_CODEC2)
        {
            // Create Codec2 software decoder
            m_pCodec2Decoder = new CCodec2(true);  // true = 3200bps mode

            // Create IMBE software encoder for P25 output
            m_pImbeEncoder = new imbe_vocoder();

            // Open two encoder channels for AMBE+ and AMBE2+ output
            m_EncoderChannel1 = g_Vocodecs.OpenEncoderChannel(m_uiCodecOut1);
            m_EncoderChannel2 = g_Vocodecs.OpenEncoderChannel(m_uiCodecOut2);

            ok = (m_EncoderChannel1 != NULL) && (m_EncoderChannel2 != NULL);

            if ( ok )
            {
                // store port
                m_uiPort = uiPort;

                // Create signal processors for each output encoder
                // Apply gain based on output codec - adjusted for Codec2 input levels
                // Codec2 encoder gains from main.h
                int gain1 = (m_uiCodecOut1 == CODEC_AMBEPLUS) ? CODEC2_TO_AMBEPLUS_GAIN : CODEC2_TO_AMBE2PLUS_GAIN;
                int gain2 = (m_uiCodecOut2 == CODEC_AMBEPLUS) ? CODEC2_TO_AMBEPLUS_GAIN : CODEC2_TO_AMBE2PLUS_GAIN;
                m_pSignalProcessor1 = new CSignalProcessor((float)gain1);
                m_pSignalProcessor2 = new CSignalProcessor((float)gain2);

                // Precompute gain multipliers (avoid pow() per packet)
                m_fCodec2DecodeGain = pow(10.0f, CODEC2_DECODE_GAIN / 20.0f);
                m_fImbeGain = pow(10.0f, CODEC2_TO_IMBE_GAIN / 20.0f);

                std::cout << "Codec2 input mode: software decode -> hardware encode (AMBE+ & AMBE2+) + software encode (IMBE)" << std::endl;
                std::cout << "  Signal processing: gain1=" << gain1 << "dB, gain2=" << gain2 << "dB" << std::endl;

                // start thread
                m_pThread = new std::thread(CStream::Thread, this);

                // init timeout system
                m_LastActivity.Now();
                m_iTotalPackets = 0;
                m_iLostPackets = 0;
            }
            else
            {
                std::cout << "Error opening stream : no suitable encoder channels available for Codec2 input" << std::endl;
                // Clean up partial allocations
                if (m_EncoderChannel1 != NULL) { m_EncoderChannel1->Close(); m_EncoderChannel1 = NULL; }
                if (m_EncoderChannel2 != NULL) { m_EncoderChannel2->Close(); m_EncoderChannel2 = NULL; }
                delete m_pCodec2Decoder;
                m_pCodec2Decoder = NULL;
                delete m_pImbeEncoder;
                m_pImbeEncoder = NULL;
                if (m_pSignalProcessor1 != NULL) { delete m_pSignalProcessor1; m_pSignalProcessor1 = NULL; }
                if (m_pSignalProcessor2 != NULL) { delete m_pSignalProcessor2; m_pSignalProcessor2 = NULL; }
            }
        }
        else if (m_uiCodecIn == CODEC_IMBE)
        {
            // IMBE input mode: need two encoder channels + software IMBE decoder + software Codec2 encoder
            // Create IMBE software decoder
            m_pImbeDecoder = new imbe_vocoder();

            // Create Codec2 software encoder for 3rd output
            m_pCodec2Encoder = new CCodec2(true);  // true = 3200bps mode

            // Open two encoder channels for AMBE+ and AMBE2+ output
            m_EncoderChannel1 = g_Vocodecs.OpenEncoderChannel(m_uiCodecOut1);
            m_EncoderChannel2 = g_Vocodecs.OpenEncoderChannel(m_uiCodecOut2);

            ok = (m_EncoderChannel1 != NULL) && (m_EncoderChannel2 != NULL);

            if ( ok )
            {
                // store port
                m_uiPort = uiPort;

                // Create signal processors for each output encoder
                // IMBE decode gain is applied separately, these are additional encoder gains
                int gain1 = IMBE_TO_AMBEPLUS_GAIN;   // D-Star encoder gain
                int gain2 = IMBE_TO_AMBE2PLUS_GAIN;  // DMR encoder gain
                m_pSignalProcessor1 = new CSignalProcessor((float)gain1);
                m_pSignalProcessor2 = new CSignalProcessor((float)gain2);

                // Precompute gain multipliers (avoid pow() per packet)
                m_fImbeGain = pow(10.0f, IMBE_DECODE_GAIN / 20.0f);
                m_fCodec2Gain = pow(10.0f, IMBE_TO_CODEC2_GAIN / 20.0f);

                std::cout << "IMBE input mode: software decode -> hardware encode (AMBE+ & AMBE2+) + software encode (Codec2)" << std::endl;
                std::cout << "  Signal processing: gain1=" << gain1 << "dB (D-Star), gain2=" << gain2 << "dB (DMR)" << std::endl;
                std::cout << "  Encoder ch1 (D-Star): in=" << m_EncoderChannel1->GetChannelIn()
                          << " out=" << m_EncoderChannel1->GetChannelOut()
                          << " codecOut=" << (int)m_EncoderChannel1->GetCodecOut() << std::endl;
                std::cout << "  Encoder ch2 (DMR):    in=" << m_EncoderChannel2->GetChannelIn()
                          << " out=" << m_EncoderChannel2->GetChannelOut()
                          << " codecOut=" << (int)m_EncoderChannel2->GetCodecOut() << std::endl;

                // start thread
                m_pThread = new std::thread(CStream::Thread, this);

                // init timeout system
                m_LastActivity.Now();
                m_iTotalPackets = 0;
                m_iLostPackets = 0;
            }
            else
            {
                std::cout << "Error opening stream : no suitable encoder channels available for IMBE input" << std::endl;
                // Clean up partial allocations
                if (m_EncoderChannel1 != NULL) { m_EncoderChannel1->Close(); m_EncoderChannel1 = NULL; }
                if (m_EncoderChannel2 != NULL) { m_EncoderChannel2->Close(); m_EncoderChannel2 = NULL; }
                delete m_pImbeDecoder;
                m_pImbeDecoder = NULL;
                delete m_pCodec2Encoder;
                m_pCodec2Encoder = NULL;
                if (m_pSignalProcessor1 != NULL) { delete m_pSignalProcessor1; m_pSignalProcessor1 = NULL; }
                if (m_pSignalProcessor2 != NULL) { delete m_pSignalProcessor2; m_pSignalProcessor2 = NULL; }
            }
        }
        else
        {
            // Normal AMBE input mode: open a VocodecChannel for hardware transcoding
            ok &= ((m_VocodecChannel = g_Vocodecs.OpenChannel(m_uiCodecIn, m_uiCodecOut1)) != NULL);

            if ( ok )
            {
                // store port
                m_uiPort = uiPort;

                // Enable Codec2 encoding if second output is Codec2
                if (m_uiCodecOut2 == CODEC_CODEC2)
                {
                    m_VocodecChannel->EnableCodec2(true);
                    std::cout << "Codec2 software encoding enabled for stream" << std::endl;
                }

                // Enable IMBE encoding if third output is IMBE
                if (m_uiCodecOut3 == CODEC_IMBE)
                {
                    m_VocodecChannel->EnableImbe(true);
                    std::cout << "IMBE software encoding enabled for stream" << std::endl;
                }

                // start thread
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

        // if init failed after socket was opened, close the socket
        if ( !ok )
        {
            m_Socket.Close();
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
    // Idempotent: if thread already stopped, Close() was already called
    if ( m_pThread == NULL )
        return;

    // For IMBE/Codec2 input modes, wait for encoder queues to drain
    // This ensures packets still in DVSI hardware FIFOs get processed and sent back
    // The thread must keep running during drain to collect hardware responses
    if ( (m_uiCodecIn == CODEC_IMBE || m_uiCodecIn == CODEC_CODEC2) &&
         m_EncoderChannel1 != NULL && m_EncoderChannel2 != NULL )
    {
        const int MAX_DRAIN_MS = 500;
        const int DRAIN_POLL_MS = 10;
        const int HW_LATENCY_MS = 100;
        int waitedMs = 0;

        // Wait for voice queues to drain (PCM waiting to be sent to hardware)
        while ( waitedMs < MAX_DRAIN_MS )
        {
            CPacketQueue *v1 = m_EncoderChannel1->GetVoiceQueue();
            bool v1e = v1->empty();
            m_EncoderChannel1->ReleaseVoiceQueue();

            CPacketQueue *v2 = m_EncoderChannel2->GetVoiceQueue();
            bool v2e = v2->empty();
            m_EncoderChannel2->ReleaseVoiceQueue();

            if ( v1e && v2e )
                break;

            std::this_thread::sleep_for(std::chrono::milliseconds(DRAIN_POLL_MS));
            waitedMs += DRAIN_POLL_MS;
        }

        // Wait for hardware latency - frames already in DVSI hardware need time to return
        std::this_thread::sleep_for(std::chrono::milliseconds(HW_LATENCY_MS));

        // Now wait for output queues to drain (encoded AMBE coming back from hardware)
        // Use atomic counters for software queue checks (thread-safe, relaxed ordering
        // is sufficient — stale reads only cause slightly over-waiting)
        waitedMs = 0;
        while ( waitedMs < MAX_DRAIN_MS )
        {
            CPacketQueue *q1 = m_EncoderChannel1->GetPacketQueueOut();
            bool q1e = q1->empty();
            m_EncoderChannel1->ReleasePacketQueueOut();

            CPacketQueue *q2 = m_EncoderChannel2->GetPacketQueueOut();
            bool q2e = q2->empty();
            m_EncoderChannel2->ReleasePacketQueueOut();

            bool swQueueEmpty;
            if (m_uiCodecIn == CODEC_CODEC2)
                swQueueEmpty = (m_nImbeQueueSize.load() == 0);
            else
                swQueueEmpty = (m_nCodec2QueueSize.load() == 0);

            if ( q1e && q2e && swQueueEmpty )
                break;

            std::this_thread::sleep_for(std::chrono::milliseconds(DRAIN_POLL_MS));
            waitedMs += DRAIN_POLL_MS;
        }
    }
    // Plain AMBE input mode (hardware-native transcoding): drain the DVSI
    // pipeline before stopping the thread and closing the channel. Without
    // this, frames still inside the hardware at close time were lost when
    // CVocodecChannel::Close → PurgeAllQueues() fired below — observable as
    // "N packets lost at stream close" / "drain stalled" warnings on the
    // xlxd side, cutting 20–40ms off the tail of the audio.
    //
    // Symmetric with the IMBE/Codec2 drain above but sized for the AMBE
    // hardware path: no intermediate PCM voice queue (the VocodecChannel's
    // PacketQueueIn feeds the USB pump thread directly), HW latency
    // observed in the 15–65ms range so the 100ms cap has comfortable
    // headroom. Phase 3 also waits on m_pStagedPacket because AMBE streams
    // with secondary Codec2/IMBE outputs (M17/P25 cross-mode listeners)
    // stage the hardware packet in Task() while the software encoder
    // catches up — we must not stop the thread until that last packet
    // has been sent.
    else if ( m_VocodecChannel != NULL )
    {
        const int MAX_DRAIN_MS    = 500;
        const int DRAIN_POLL_MS   = 10;
        const int HW_LATENCY_MS   = 100;
        int waitedMs = 0;

        // Phase 1: wait for the packet-in queue (packets xlxd sent that
        // the USB pump hasn't yet handed to the DVSI hardware) to drain.
        // The pump thread is an independent consumer of this queue; the
        // Task thread only writes to it.
        while ( waitedMs < MAX_DRAIN_MS )
        {
            CPacketQueue *qIn = m_VocodecChannel->GetPacketQueueIn();
            bool empty = qIn->empty();
            m_VocodecChannel->ReleasePacketQueueIn();
            if ( empty ) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(DRAIN_POLL_MS));
            waitedMs += DRAIN_POLL_MS;
        }

        // Phase 2: hard sleep for DVSI DSP latency. Frames already inside
        // the hardware pipeline need wall-clock time to emerge — we cannot
        // poll our way out of this, the hardware is asynchronous.
        std::this_thread::sleep_for(std::chrono::milliseconds(HW_LATENCY_MS));

        // Phase 3: wait for the packet-out queue (transcoded AMBE from HW,
        // awaiting forwarding to xlxd) to drain AND for any staged packet
        // to be sent. The Task thread is still running during this wait
        // and is what pops from the queue and sends via m_Socket.SendVoice().
        waitedMs = 0;
        while ( waitedMs < MAX_DRAIN_MS )
        {
            CPacketQueue *qOut = m_VocodecChannel->GetPacketQueueOut();
            bool empty = qOut->empty();
            m_VocodecChannel->ReleasePacketQueueOut();
            if ( empty && (m_pStagedPacket == NULL) ) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(DRAIN_POLL_MS));
            waitedMs += DRAIN_POLL_MS;
        }
    }

    // Stop thread — must complete before any pointer nulling below
    m_bStopThread = true;
    m_pThread->join();
    delete m_pThread;
    m_pThread = NULL;

    // Close socket
    m_Socket.Close();

    // Capture PID-FIFO desync drop counts BEFORE closing the channels —
    // CVocodecChannel::Close calls PurgeAllQueues which resets the
    // counters. We want to report the values that accumulated during
    // this stream's lifetime, not 0.
    uint32 totalOrphanDrops = 0;
    uint32 totalOverflowDrops = 0;
    if ( m_VocodecChannel != NULL )
    {
        totalOrphanDrops += m_VocodecChannel->GetPidFifoOrphanDrops();
        totalOverflowDrops += m_VocodecChannel->GetPidFifoOverflowDrops();
    }
    if ( m_EncoderChannel1 != NULL )
    {
        totalOrphanDrops += m_EncoderChannel1->GetPidFifoOrphanDrops();
        totalOverflowDrops += m_EncoderChannel1->GetPidFifoOverflowDrops();
    }
    if ( m_EncoderChannel2 != NULL )
    {
        totalOrphanDrops += m_EncoderChannel2->GetPidFifoOrphanDrops();
        totalOverflowDrops += m_EncoderChannel2->GetPidFifoOverflowDrops();
    }

    // Close and null vocodec channels
    if ( m_VocodecChannel != NULL )
    {
        m_VocodecChannel->EnableCodec2(false);
        m_VocodecChannel->EnableImbe(false);
        m_VocodecChannel->Close();
        m_VocodecChannel = NULL;
    }
    if ( m_EncoderChannel1 != NULL ) { m_EncoderChannel1->Close(); m_EncoderChannel1 = NULL; }
    if ( m_EncoderChannel2 != NULL ) { m_EncoderChannel2->Close(); m_EncoderChannel2 = NULL; }

    // Delete and null software objects
    if ( m_pCodec2Decoder != NULL ) { delete m_pCodec2Decoder; m_pCodec2Decoder = NULL; }
    if ( m_pSignalProcessor1 != NULL ) { delete m_pSignalProcessor1; m_pSignalProcessor1 = NULL; }
    if ( m_pSignalProcessor2 != NULL ) { delete m_pSignalProcessor2; m_pSignalProcessor2 = NULL; }
    if ( m_pImbeEncoder != NULL ) { delete m_pImbeEncoder; m_pImbeEncoder = NULL; }
    if ( m_pImbeDecoder != NULL ) { delete m_pImbeDecoder; m_pImbeDecoder = NULL; }
    if ( m_pCodec2Encoder != NULL ) { delete m_pCodec2Encoder; m_pCodec2Encoder = NULL; }

    // Delete staged packet if still pending
    if ( m_pStagedPacket != NULL )
    {
        delete m_pStagedPacket;
        m_pStagedPacket = NULL;
    }

    // Clear software queues and reset counters
    while ( !m_ImbeQueue.empty() ) m_ImbeQueue.pop();
    while ( !m_Codec2Queue.empty() ) m_Codec2Queue.pop();
    m_nImbeQueueSize = 0;
    m_nCodec2QueueSize = 0;

    // Report
    std::cout << m_iLostPackets << " of " << m_iTotalPackets << " packets lost" << std::endl;

    // PID-FIFO desync summary. Only logged when non-zero — healthy
    // deployments produce nothing (avoids log spam). Real-time
    // first-occurrence warnings are emitted from PushPid/PopPid in
    // CVocodecChannel; this is the per-stream cumulative summary.
    if ( totalOrphanDrops > 0 || totalOverflowDrops > 0 )
    {
        std::cout << "PID FIFO desync summary: "
                  << totalOrphanDrops << " orphan drops, "
                  << totalOverflowDrops << " overflow drops"
                  << std::endl;
    }
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
    // Route to appropriate task handler based on input codec
    if (m_uiCodecIn == CODEC_CODEC2)
    {
        TaskCodec2Input();
    }
    else if (m_uiCodecIn == CODEC_IMBE)
    {
        TaskImbeInput();
    }
    else
    {
        // Normal AMBE input mode
        CBuffer     Buffer;
        CIp         Ip;
        uint8       uiPid;
        uint8       Ambe[AMBE_FRAME_SIZE];
        uint8       Codec2[CODEC2_FRAME_SIZE];
        uint8       Imbe[IMBE_FRAME_SIZE];
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

        // Check if we have a staged packet waiting for software codecs
        if ( m_pStagedPacket != NULL )
        {
            bool codec2Ready = (m_uiCodecOut2 != CODEC_CODEC2) || m_VocodecChannel->HasCodec2Data();
            bool imbeReady   = (m_uiCodecOut3 != CODEC_IMBE)   || m_VocodecChannel->HasImbeData();

            if ( codec2Ready && imbeReady )
            {
                // software codecs caught up — get the data and send
                ::memset(Codec2, 0, CODEC2_FRAME_SIZE);
                ::memset(Imbe, 0, IMBE_FRAME_SIZE);
                if (m_uiCodecOut2 == CODEC_CODEC2)
                    m_VocodecChannel->GetCodec2Data(Codec2);
                if (m_uiCodecOut3 == CODEC_IMBE)
                    m_VocodecChannel->GetImbeData(Imbe);

                EncodeDvFramePacket(&Buffer, m_pStagedPacket->GetPid(), m_pStagedPacket->GetAmbe(), Codec2, Imbe);
                m_Socket.SendVoice(Buffer, m_Ip, m_uiPort);
                delete m_pStagedPacket;
                m_pStagedPacket = NULL;
            }
            else if ( m_StagedPacketTime.DurationSinceNow() > 1.0 )
            {
                // software encoder stalled — drop after 1s to avoid blocking
                m_iLostPackets++;
                delete m_pStagedPacket;
                m_pStagedPacket = NULL;
            }
        }

        // Pop from hardware output queue only if no packet is staged
        if ( m_pStagedPacket == NULL )
        {
            queue = m_VocodecChannel->GetPacketQueueOut();
            while ( !queue->empty() )
            {
                packet = (CAmbePacket *)queue->front();
                queue->pop();

                // check if software codecs are ready (peek, don't consume)
                bool codec2Ready = (m_uiCodecOut2 != CODEC_CODEC2) || m_VocodecChannel->HasCodec2Data();
                bool imbeReady   = (m_uiCodecOut3 != CODEC_IMBE)   || m_VocodecChannel->HasImbeData();

                if ( codec2Ready && imbeReady )
                {
                    // all codecs ready — get the data and send immediately
                    ::memset(Codec2, 0, CODEC2_FRAME_SIZE);
                    ::memset(Imbe, 0, IMBE_FRAME_SIZE);
                    if (m_uiCodecOut2 == CODEC_CODEC2)
                        m_VocodecChannel->GetCodec2Data(Codec2);
                    if (m_uiCodecOut3 == CODEC_IMBE)
                        m_VocodecChannel->GetImbeData(Imbe);

                    EncodeDvFramePacket(&Buffer, packet->GetPid(), packet->GetAmbe(), Codec2, Imbe);
                    m_Socket.SendVoice(Buffer, m_Ip, m_uiPort);
                    delete packet;
                }
                else
                {
                    // software codecs not ready — stage this packet and stop popping
                    m_pStagedPacket = packet;
                    m_StagedPacketTime.Now();
                    break;
                }
            }
            m_VocodecChannel->ReleasePacketQueueOut();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// Codec2 input handling

void CStream::TaskCodec2Input(void)
{
    CBuffer     Buffer;
    CIp         Ip;
    uint8       uiPid;
    uint8       Codec2Data[CODEC2_FRAME_SIZE];
    int16_t     pcmAudio[160];  // 160 samples = 20ms at 8kHz
    uint8       ImbeData[IMBE_FRAME_SIZE];
    CPacketQueue *queue1, *queue2;

    // anything coming in from codec client ?
    if ( m_Socket.Receive(&Buffer, &Ip, 1) != -1 )
    {
        // crack Codec2 packet
        if ( IsValidCodec2FramePacket(Buffer, &uiPid, Codec2Data) )
        {
            m_LastActivity.Now();
            m_iTotalPackets++;

            // Software decode Codec2 to PCM
            m_pCodec2Decoder->codec2_decode(pcmAudio, Codec2Data);

            // Apply decode gain to Codec2 output (matches IMBE path which applies IMBE_DECODE_GAIN)
            for (int i = 0; i < 160; i++)
            {
                int32_t sample = (int32_t)(pcmAudio[i] * m_fCodec2DecodeGain);
                if (sample > 32767) sample = 32767;
                else if (sample < -32768) sample = -32768;
                pcmAudio[i] = (int16_t)sample;
            }

            // Software encode PCM to IMBE for P25 output (3rd codec)
            // Apply precomputed gain adjustment for IMBE encoding
            int16_t pcmForImbe[160];
            for (int i = 0; i < 160; i++)
            {
                int32_t sample = (int32_t)(pcmAudio[i] * m_fImbeGain);
                if (sample > 32767) sample = 32767;
                else if (sample < -32768) sample = -32768;
                pcmForImbe[i] = (int16_t)sample;
            }
            m_pImbeEncoder->encode_4400(pcmForImbe, ImbeData);

            // Queue the IMBE data for later retrieval when hardware encoders return
            // Drop oldest if queue exceeds max depth (hardware stall protection)
            if ( m_ImbeQueue.size() >= SOFTWARE_QUEUE_MAX_DEPTH )
            {
                m_ImbeQueue.pop();
                m_nImbeQueueSize--;
            }
            std::vector<uint8> imbeVec(ImbeData, ImbeData + IMBE_FRAME_SIZE);
            m_ImbeQueue.push(imbeVec);
            m_nImbeQueueSize++;

            // Convert PCM to byte arrays for CVoicePacket (one for each encoder)
            // DVSI hardware expects big-endian PCM — use portable byte extraction
            uint8 pcmBytes1[320];
            uint8 pcmBytes2[320];
            for (int i = 0; i < 160; i++)
            {
                pcmBytes1[i*2] = (uint8)(((uint16_t)pcmAudio[i] >> 8) & 0xFF);     // high byte first
                pcmBytes1[i*2 + 1] = (uint8)((uint16_t)pcmAudio[i] & 0xFF);        // low byte second
                pcmBytes2[i*2] = pcmBytes1[i*2];
                pcmBytes2[i*2 + 1] = pcmBytes1[i*2 + 1];
            }

            // Apply signal processing (gain adjustment) to each PCM buffer
            // This matches the gain normalization used in the AMBE->AMBE path
            m_pSignalProcessor1->Process(pcmBytes1, 320);
            m_pSignalProcessor2->Process(pcmBytes2, 320);

            // Create voice packets and push to both encoder channels
            CVoicePacket *voicePacket1 = new CVoicePacket(pcmBytes1, 320);
            voicePacket1->SetChannel(m_EncoderChannel1->GetChannelOut());

            CVoicePacket *voicePacket2 = new CVoicePacket(pcmBytes2, 320);
            voicePacket2->SetChannel(m_EncoderChannel2->GetChannelOut());

            // PID-preservation: push the input packet's PID into both
            // encoder channels' FIFOs so the matching pops in the USB
            // interface (when the AMBE responses arrive from hardware)
            // can re-attach the correct PID to the output packets. The
            // response builder below uses packet1->GetPid() for the
            // outbound response, so channel 1's pid is the one that
            // matters; channel 2's is pushed for symmetry / future-
            // proofing in case any future code path reads packet2's pid.
            m_EncoderChannel1->PushPid(uiPid);
            m_EncoderChannel2->PushPid(uiPid);

            // Push to encoder channel 1 (AMBE+)
            queue1 = m_EncoderChannel1->GetVoiceQueue();
            queue1->push(voicePacket1);
            m_EncoderChannel1->ReleaseVoiceQueue();

            // Push to encoder channel 2 (AMBE2+)
            queue2 = m_EncoderChannel2->GetVoiceQueue();
            queue2->push(voicePacket2);
            m_EncoderChannel2->ReleaseVoiceQueue();
        }
    }

    // Check for encoded output from both hardware channels and IMBE queue
    // We need output from all three before we can send a response
    // Each queue lock is acquired and released independently to prevent ABBA deadlock
    // TOCTOU safety: only this thread pops from these queues; USB threads only push
    bool ready;
    do {
        ready = false;

        bool q1has, q2has;
        {
            CPacketQueue *q = m_EncoderChannel1->GetPacketQueueOut();
            q1has = !q->empty();
            m_EncoderChannel1->ReleasePacketQueueOut();
        }
        {
            CPacketQueue *q = m_EncoderChannel2->GetPacketQueueOut();
            q2has = !q->empty();
            m_EncoderChannel2->ReleasePacketQueueOut();
        }

        if ( q1has && q2has && !m_ImbeQueue.empty() )
        {
            CAmbePacket *packet1;
            {
                CPacketQueue *q = m_EncoderChannel1->GetPacketQueueOut();
                packet1 = (CAmbePacket *)q->front();
                q->pop();
                m_EncoderChannel1->ReleasePacketQueueOut();
            }
            CAmbePacket *packet2;
            {
                CPacketQueue *q = m_EncoderChannel2->GetPacketQueueOut();
                packet2 = (CAmbePacket *)q->front();
                q->pop();
                m_EncoderChannel2->ReleasePacketQueueOut();
            }

            std::vector<uint8> imbeData = m_ImbeQueue.front();
            m_ImbeQueue.pop();
            m_nImbeQueueSize--;

            Buffer.clear();
            Buffer.Append((uint8)m_uiCodecOut1);
            Buffer.Append((uint8)packet1->GetPid());
            Buffer.Append(packet1->GetAmbe(), AMBE_FRAME_SIZE);
            Buffer.Append((uint8)m_uiCodecOut2);
            Buffer.Append(packet2->GetAmbe(), AMBE_FRAME_SIZE);
            Buffer.Append((uint8)m_uiCodecOut3);
            Buffer.Append(imbeData.data(), IMBE_FRAME_SIZE);

            m_Socket.SendVoice(Buffer, m_Ip, m_uiPort);

            delete packet1;
            delete packet2;
            ready = true;
        }
    } while (ready);
}

////////////////////////////////////////////////////////////////////////////////////////
// IMBE input handling

void CStream::TaskImbeInput(void)
{
    CBuffer     Buffer;
    CIp         Ip;
    uint8       uiPid;
    uint8       ImbeData[IMBE_FRAME_SIZE];
    int16_t     pcmAudio[160];  // 160 samples = 20ms at 8kHz
    uint8       Codec2Data[CODEC2_FRAME_SIZE];
    CPacketQueue *queue1, *queue2;

    // anything coming in from codec client ?
    if ( m_Socket.Receive(&Buffer, &Ip, 1) != -1 )
    {
        // crack IMBE packet
        if ( IsValidImbeFramePacket(Buffer, &uiPid, ImbeData) )
        {
            m_LastActivity.Now();
            m_iTotalPackets++;

            // Software decode IMBE to PCM
            // imbe_vocoder::decode_4400(int16_t *snd, uint8_t *imbe)
            m_pImbeDecoder->decode_4400(pcmAudio, ImbeData);

            // Apply precomputed gain to IMBE decoder output
            for (int i = 0; i < 160; i++)
            {
                int32_t sample = (int32_t)(pcmAudio[i] * m_fImbeGain);
                // Clip to int16 range
                if (sample > 32767) sample = 32767;
                else if (sample < -32768) sample = -32768;
                pcmAudio[i] = (int16_t)sample;
            }

            // Software encode PCM to Codec2 for M17 output (3rd codec)
            // Apply precomputed IMBE->Codec2 gain adjustment
            int16_t pcmForCodec2[160];
            for (int i = 0; i < 160; i++)
            {
                int32_t sample = (int32_t)(pcmAudio[i] * m_fCodec2Gain);
                if (sample > 32767) sample = 32767;
                else if (sample < -32768) sample = -32768;
                pcmForCodec2[i] = (int16_t)sample;
            }
            m_pCodec2Encoder->codec2_encode(Codec2Data, pcmForCodec2);

            // Queue the Codec2 data for later retrieval when hardware encoders return
            // Drop oldest if queue exceeds max depth (hardware stall protection)
            if ( m_Codec2Queue.size() >= SOFTWARE_QUEUE_MAX_DEPTH )
            {
                m_Codec2Queue.pop();
                m_nCodec2QueueSize--;
            }
            std::vector<uint8> c2vec(Codec2Data, Codec2Data + CODEC2_FRAME_SIZE);
            m_Codec2Queue.push(c2vec);
            m_nCodec2QueueSize++;

            // Convert PCM to byte arrays for CVoicePacket (one for each encoder)
            // DVSI hardware expects big-endian PCM — use portable byte extraction
            uint8 pcmBytes1[320];
            uint8 pcmBytes2[320];
            for (int i = 0; i < 160; i++)
            {
                pcmBytes1[i*2] = (uint8)(((uint16_t)pcmAudio[i] >> 8) & 0xFF);     // high byte first
                pcmBytes1[i*2 + 1] = (uint8)((uint16_t)pcmAudio[i] & 0xFF);        // low byte second
                pcmBytes2[i*2] = pcmBytes1[i*2];
                pcmBytes2[i*2 + 1] = pcmBytes1[i*2 + 1];
            }

            // Apply signal processing (gain adjustment) to each PCM buffer
            m_pSignalProcessor1->Process(pcmBytes1, 320);
            m_pSignalProcessor2->Process(pcmBytes2, 320);

            // Create voice packets and push to both encoder channels
            CVoicePacket *voicePacket1 = new CVoicePacket(pcmBytes1, 320);
            voicePacket1->SetChannel(m_EncoderChannel1->GetChannelOut());

            CVoicePacket *voicePacket2 = new CVoicePacket(pcmBytes2, 320);
            voicePacket2->SetChannel(m_EncoderChannel2->GetChannelOut());

            // PID-preservation: push the input packet's PID into both
            // encoder channels' FIFOs so the matching pops in the USB
            // interface (when the AMBE responses arrive from hardware)
            // can re-attach the correct PID. Same rationale as
            // TaskCodec2Input above.
            m_EncoderChannel1->PushPid(uiPid);
            m_EncoderChannel2->PushPid(uiPid);

            // Push to encoder channel 1 (AMBE+)
            queue1 = m_EncoderChannel1->GetVoiceQueue();
            queue1->push(voicePacket1);
            m_EncoderChannel1->ReleaseVoiceQueue();

            // Push to encoder channel 2 (AMBE2+)
            queue2 = m_EncoderChannel2->GetVoiceQueue();
            queue2->push(voicePacket2);
            m_EncoderChannel2->ReleaseVoiceQueue();
        }
    }

    // Check for encoded output from both hardware channels and Codec2 queue
    // Each queue lock is acquired and released independently to prevent ABBA deadlock
    // TOCTOU safety: only this thread pops from these queues; USB threads only push
    bool ready;
    do {
        ready = false;

        bool q1has, q2has;
        {
            CPacketQueue *q = m_EncoderChannel1->GetPacketQueueOut();
            q1has = !q->empty();
            m_EncoderChannel1->ReleasePacketQueueOut();
        }
        {
            CPacketQueue *q = m_EncoderChannel2->GetPacketQueueOut();
            q2has = !q->empty();
            m_EncoderChannel2->ReleasePacketQueueOut();
        }

        if ( q1has && q2has && !m_Codec2Queue.empty() )
        {
            CAmbePacket *packet1;
            {
                CPacketQueue *q = m_EncoderChannel1->GetPacketQueueOut();
                packet1 = (CAmbePacket *)q->front();
                q->pop();
                m_EncoderChannel1->ReleasePacketQueueOut();
            }
            CAmbePacket *packet2;
            {
                CPacketQueue *q = m_EncoderChannel2->GetPacketQueueOut();
                packet2 = (CAmbePacket *)q->front();
                q->pop();
                m_EncoderChannel2->ReleasePacketQueueOut();
            }

            std::vector<uint8> c2data = m_Codec2Queue.front();
            m_Codec2Queue.pop();
            m_nCodec2QueueSize--;

            Buffer.clear();
            Buffer.Append((uint8)m_uiCodecOut1);
            Buffer.Append((uint8)packet1->GetPid());
            Buffer.Append(packet1->GetAmbe(), AMBE_FRAME_SIZE);
            Buffer.Append((uint8)m_uiCodecOut2);
            Buffer.Append(packet2->GetAmbe(), AMBE_FRAME_SIZE);
            Buffer.Append((uint8)m_uiCodecOut3);
            Buffer.Append(c2data.data(), CODEC2_FRAME_SIZE);

            m_Socket.SendVoice(Buffer, m_Ip, m_uiPort);

            delete packet1;
            delete packet2;
            ready = true;
        }
    } while (ready);
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

bool CStream::IsValidCodec2FramePacket(const CBuffer &Buffer, uint8 *pid, uint8 *codec2)
{
    bool valid = false;

    // Codec2 packet format: codec(1) + pid(1) + codec2_data(8) = 10 bytes
    if ( Buffer.size() == 10 )
    {
        uint8 codec = Buffer.data()[0];
        *pid = Buffer.data()[1];
        ::memcpy(codec2, &(Buffer.data()[2]), CODEC2_FRAME_SIZE);
        valid = (codec == CODEC_CODEC2);
    }

    return valid;
}

bool CStream::IsValidImbeFramePacket(const CBuffer &Buffer, uint8 *pid, uint8 *imbe)
{
    bool valid = false;

    // IMBE packet format: codec(1) + pid(1) + imbe_data(11) = 13 bytes
    if ( Buffer.size() == 13 )
    {
        uint8 codec = Buffer.data()[0];
        *pid = Buffer.data()[1];
        ::memcpy(imbe, &(Buffer.data()[2]), IMBE_FRAME_SIZE);
        valid = (codec == CODEC_IMBE);
    }

    return valid;
}

////////////////////////////////////////////////////////////////////////////////////////
// packet encoding helpers

void CStream::EncodeDvFramePacket(CBuffer *Buffer, uint8 Pid, uint8 *Ambe, uint8 *Codec2, uint8 *Imbe)
{
    Buffer->clear();

    // Combined response format:
    // codec1(1) + pid(1) + ambe1(9) + codec2(1) + codec2_data(8) + codec3(1) + imbe(11) = 32 bytes
    // First output codec (AMBE+ or AMBE2+)
    Buffer->Append((uint8)GetCodecOut1());
    Buffer->Append((uint8)Pid);
    Buffer->Append(Ambe, AMBE_FRAME_SIZE);

    // Second output codec (Codec2 for M17)
    Buffer->Append((uint8)GetCodecOut2());
    Buffer->Append(Codec2, CODEC2_FRAME_SIZE);

    // Third output codec (IMBE for P25)
    Buffer->Append((uint8)GetCodecOut3());
    Buffer->Append(Imbe, IMBE_FRAME_SIZE);
}

