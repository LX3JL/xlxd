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
#include <cstring>
#include "ctimepoint.h"
#include "cvocodecchannel.h"
#include "cvocodecinterface.h"

////////////////////////////////////////////////////////////////////////////////////////
// constructor

CVocodecChannel::CVocodecChannel(CVocodecInterface *InterfaceIn, int iChIn, CVocodecInterface *InterfaceOut, int iChOut, int iSpeechGain)
{
    m_bOpen = false;
    m_InterfaceIn = InterfaceIn;
    m_iChannelIn = iChIn;
    m_InterfaceOut = InterfaceOut;
    m_iChannelOut = iChOut;
    m_iSpeechGain = iSpeechGain;
    m_signalProcessor = new CSignalProcessor((float)m_iSpeechGain);

    // Codec2 encoder starts disabled, enabled per-stream as needed
    m_bCodec2Enabled = false;
    m_pCodec2 = NULL;
    m_pCodec2Thread = NULL;
    m_bCodec2StopThread = false;

    // IMBE encoder starts disabled, enabled per-stream as needed
    m_bImbeEnabled = false;
    m_pImbe = NULL;
    m_pImbeThread = NULL;
    m_bImbeStopThread = false;

    // PID FIFO desync drop counters — reset by PurgeAllQueues at
    // every Open/Close so each stream gets a fresh accounting epoch.
    m_PidFifoOrphanDrops = 0;
    m_PidFifoOverflowDrops = 0;
}

////////////////////////////////////////////////////////////////////////////////////////
// destructor

CVocodecChannel::~CVocodecChannel()
{
    // Stop Codec2 encoder thread first
    if (m_pCodec2Thread != NULL)
    {
        m_bCodec2StopThread = true;
        m_Codec2PcmCondition.notify_all();
        m_pCodec2Thread->join();
        delete m_pCodec2Thread;
        m_pCodec2Thread = NULL;
    }

    // Stop IMBE encoder thread
    if (m_pImbeThread != NULL)
    {
        m_bImbeStopThread = true;
        m_ImbePcmCondition.notify_all();
        m_pImbeThread->join();
        delete m_pImbeThread;
        m_pImbeThread = NULL;
    }

    PurgeAllQueues();
    delete m_signalProcessor;
    if (m_pCodec2 != NULL)
    {
        delete m_pCodec2;
        m_pCodec2 = NULL;
    }
    if (m_pImbe != NULL)
    {
        delete m_pImbe;
        m_pImbe = NULL;
    }
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
        std::cout << "Vocodec channel " <<
            m_InterfaceIn->GetName() << ":" << m_InterfaceIn->GetSerial() << ":" << (int)m_iChannelIn << " -> " <<
            m_InterfaceOut->GetName() << ":" << m_InterfaceOut->GetSerial() << ":" << (int)m_iChannelOut << " open" << std::endl;
    }
    return ok;
}

void CVocodecChannel::Close(void)
{
    if ( m_bOpen )
    {
        m_bOpen = false;
        PurgeAllQueues();
        std::cout << "Vocodec channel " <<
            m_InterfaceIn->GetName() << ":" << m_InterfaceIn->GetSerial() << ":" << (int)m_iChannelIn << " -> " <<
            m_InterfaceOut->GetName() << ":" << m_InterfaceOut->GetSerial() << ":" << (int)m_iChannelOut << " closed" << std::endl;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// get

uint8 CVocodecChannel::GetCodecIn(void) const
{
    return m_InterfaceIn->GetChannelCodec(m_iChannelIn);
}

uint8 CVocodecChannel::GetCodecOut(void) const
{
    return m_InterfaceOut->GetChannelCodec(m_iChannelOut);
}

////////////////////////////////////////////////////////////////////////////////////////
// processing

void CVocodecChannel::ProcessSignal(CVoicePacket& voicePacket)
{
    m_signalProcessor->Process(voicePacket.GetVoice(), voicePacket.GetVoiceSize());

    // Codec2 encoding runs in a separate thread to avoid blocking the hardware interface.
    // Here we just queue the PCM data for async encoding.
    if (m_bCodec2Enabled && m_pCodec2Thread != NULL)
    {
        int voiceSize = voicePacket.GetVoiceSize();

        if (voiceSize >= 320)  // 160 samples * 2 bytes
        {
            // Convert big-endian PCM to little-endian and queue for async encoding
            uint8 *rawVoice = voicePacket.GetVoice();
            std::vector<int16_t> audio(160);
            for (int i = 0; i < 160; i++)
            {
                audio[i] = (int16_t)((rawVoice[i*2] << 8) | rawVoice[i*2 + 1]);
            }

            // Push to PCM queue and notify encoder thread (drop oldest if full)
            {
                std::lock_guard<std::mutex> lock(m_Codec2PcmMutex);
                if ( m_Codec2PcmQueue.size() >= SOFTWARE_QUEUE_MAX_DEPTH )
                {
                    m_Codec2PcmQueue.pop();
                }
                m_Codec2PcmQueue.push(audio);
            }
            m_Codec2PcmCondition.notify_one();
        }
    }

    // IMBE encoding runs in a separate thread to avoid blocking the hardware interface.
    // Here we just queue the PCM data for async encoding.
    if (m_bImbeEnabled && m_pImbeThread != NULL)
    {
        int voiceSize = voicePacket.GetVoiceSize();

        if (voiceSize >= 320)  // 160 samples * 2 bytes
        {
            // Convert big-endian PCM to little-endian and queue for async encoding
            uint8 *rawVoice = voicePacket.GetVoice();
            std::vector<int16_t> audio(160);
            for (int i = 0; i < 160; i++)
            {
                audio[i] = (int16_t)((rawVoice[i*2] << 8) | rawVoice[i*2 + 1]);
            }

            // Push to PCM queue and notify encoder thread (drop oldest if full)
            {
                std::lock_guard<std::mutex> lock(m_ImbePcmMutex);
                if ( m_ImbePcmQueue.size() >= SOFTWARE_QUEUE_MAX_DEPTH )
                {
                    m_ImbePcmQueue.pop();
                }
                m_ImbePcmQueue.push(audio);
            }
            m_ImbePcmCondition.notify_one();
        }
    }
}


////////////////////////////////////////////////////////////////////////////////////////
// Codec2 encoding control

void CVocodecChannel::EnableCodec2(bool bEnable)
{
    if (bEnable && !m_bCodec2Enabled)
    {
        // Enable Codec2 encoding with async thread
        if (m_pCodec2 == NULL)
        {
            m_pCodec2 = new CCodec2(true);  // true = 3200bps mode (8 bytes per 20ms)
        }

        // Start the async encoder thread
        m_bCodec2StopThread = false;
        m_pCodec2Thread = new std::thread(Codec2EncoderThread, this);

        m_bCodec2Enabled = true;
        std::cout << "Codec2 async encoding enabled for channel" << std::endl;
    }
    else if (!bEnable && m_bCodec2Enabled)
    {
        // Disable Codec2 encoding
        m_bCodec2Enabled = false;

        // Stop the encoder thread
        if (m_pCodec2Thread != NULL)
        {
            m_bCodec2StopThread = true;
            m_Codec2PcmCondition.notify_all();
            m_pCodec2Thread->join();
            delete m_pCodec2Thread;
            m_pCodec2Thread = NULL;
        }

        // Clear any pending data
        {
            std::lock_guard<std::mutex> lock(m_Codec2Mutex);
            while (!m_Codec2Queue.empty())
            {
                m_Codec2Queue.pop();
            }
        }
        {
            std::lock_guard<std::mutex> lock(m_Codec2PcmMutex);
            while (!m_Codec2PcmQueue.empty())
            {
                m_Codec2PcmQueue.pop();
            }
        }
        std::cout << "Codec2 encoding disabled for channel" << std::endl;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// Codec2 queue access

bool CVocodecChannel::HasCodec2Data(void)
{
    std::lock_guard<std::mutex> lock(m_Codec2Mutex);
    return !m_Codec2Queue.empty();
}

bool CVocodecChannel::GetCodec2Data(uint8 *codec2)
{
    std::lock_guard<std::mutex> lock(m_Codec2Mutex);
    if (m_Codec2Queue.empty())
    {
        return false;
    }
    std::vector<uint8> &data = m_Codec2Queue.front();
    if (data.size() < 8) { m_Codec2Queue.pop(); return false; }
    ::memcpy(codec2, data.data(), 8);
    m_Codec2Queue.pop();
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
// IMBE encoding control

void CVocodecChannel::EnableImbe(bool bEnable)
{
    if (bEnable && !m_bImbeEnabled)
    {
        // Enable IMBE encoding with async thread
        if (m_pImbe == NULL)
        {
            m_pImbe = new imbe_vocoder();
        }

        // Start the async encoder thread
        m_bImbeStopThread = false;
        m_pImbeThread = new std::thread(ImbeEncoderThread, this);

        m_bImbeEnabled = true;
        std::cout << "IMBE async encoding enabled for channel" << std::endl;
    }
    else if (!bEnable && m_bImbeEnabled)
    {
        // Disable IMBE encoding
        m_bImbeEnabled = false;

        // Stop the encoder thread
        if (m_pImbeThread != NULL)
        {
            m_bImbeStopThread = true;
            m_ImbePcmCondition.notify_all();
            m_pImbeThread->join();
            delete m_pImbeThread;
            m_pImbeThread = NULL;
        }

        // Clear any pending data
        {
            std::lock_guard<std::mutex> lock(m_ImbeMutex);
            while (!m_ImbeQueue.empty())
            {
                m_ImbeQueue.pop();
            }
        }
        {
            std::lock_guard<std::mutex> lock(m_ImbePcmMutex);
            while (!m_ImbePcmQueue.empty())
            {
                m_ImbePcmQueue.pop();
            }
        }
        std::cout << "IMBE encoding disabled for channel" << std::endl;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// Async Codec2 encoder thread

void CVocodecChannel::Codec2EncoderThread(CVocodecChannel *pChannel)
{
    while (true)
    {
        pChannel->Codec2EncoderTask();

        // Check stop condition atomically with queue state under the same lock
        {
            std::lock_guard<std::mutex> lock(pChannel->m_Codec2PcmMutex);
            if (pChannel->m_bCodec2StopThread && pChannel->m_Codec2PcmQueue.empty())
                break;
        }
    }
}

void CVocodecChannel::Codec2EncoderTask(void)
{
    std::vector<int16_t> pcmData;

    // Wait for PCM data to encode
    {
        std::unique_lock<std::mutex> lock(m_Codec2PcmMutex);
        m_Codec2PcmCondition.wait(lock, [this] {
            return !m_Codec2PcmQueue.empty() || m_bCodec2StopThread;
        });

        // On stop, continue draining queue to avoid tail cutoff
        if (!m_Codec2PcmQueue.empty())
        {
            pcmData = m_Codec2PcmQueue.front();
            m_Codec2PcmQueue.pop();
        }
        else if (m_bCodec2StopThread)
        {
            // Queue is empty and stop requested - exit now
            return;
        }
    }

    // Encode PCM to Codec2 (outside the lock)
    if (!pcmData.empty() && m_pCodec2 != NULL)
    {
        uint8 codec2[8];  // 64 bits = 8 bytes for Codec2 3200
        m_pCodec2->codec2_encode(codec2, pcmData.data());

        // Push encoded Codec2 to output queue (drop oldest if full)
        std::lock_guard<std::mutex> lock(m_Codec2Mutex);
        if ( m_Codec2Queue.size() >= SOFTWARE_QUEUE_MAX_DEPTH )
        {
            m_Codec2Queue.pop();
        }
        std::vector<uint8> c2data(codec2, codec2 + 8);
        m_Codec2Queue.push(c2data);
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// IMBE queue access

bool CVocodecChannel::HasImbeData(void)
{
    std::lock_guard<std::mutex> lock(m_ImbeMutex);
    return !m_ImbeQueue.empty();
}

bool CVocodecChannel::GetImbeData(uint8 *imbe)
{
    std::lock_guard<std::mutex> lock(m_ImbeMutex);
    if (m_ImbeQueue.empty())
    {
        return false;
    }
    std::vector<uint8> &data = m_ImbeQueue.front();
    if (data.size() < IMBE_FRAME_SIZE) { m_ImbeQueue.pop(); return false; }
    ::memcpy(imbe, data.data(), IMBE_FRAME_SIZE);
    m_ImbeQueue.pop();
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
// Async IMBE encoder thread

void CVocodecChannel::ImbeEncoderThread(CVocodecChannel *pChannel)
{
    while (true)
    {
        pChannel->ImbeEncoderTask();

        // Check stop condition atomically with queue state under the same lock
        {
            std::lock_guard<std::mutex> lock(pChannel->m_ImbePcmMutex);
            if (pChannel->m_bImbeStopThread && pChannel->m_ImbePcmQueue.empty())
                break;
        }
    }
}

void CVocodecChannel::ImbeEncoderTask(void)
{
    std::vector<int16_t> pcmData;

    // Wait for PCM data to encode
    {
        std::unique_lock<std::mutex> lock(m_ImbePcmMutex);
        m_ImbePcmCondition.wait(lock, [this] {
            return !m_ImbePcmQueue.empty() || m_bImbeStopThread;
        });

        // On stop, continue draining queue to avoid tail cutoff
        if (!m_ImbePcmQueue.empty())
        {
            pcmData = m_ImbePcmQueue.front();
            m_ImbePcmQueue.pop();
        }
        else if (m_bImbeStopThread)
        {
            // Queue is empty and stop requested - exit now
            return;
        }
    }

    // Encode PCM to IMBE (outside the lock)
    if (!pcmData.empty() && m_pImbe != NULL)
    {
        // Apply IMBE encode gain to PCM before encoding
        static const float imbeEncodeGain = pow(10.0f, IMBE_ENCODE_GAIN / 20.0f);
        for ( size_t i = 0; i < pcmData.size(); i++ )
        {
            int32_t sample = (int32_t)(pcmData[i] * imbeEncodeGain);
            if ( sample > 32767 ) sample = 32767;
            if ( sample < -32768 ) sample = -32768;
            pcmData[i] = (int16_t)sample;
        }

        uint8 imbe[IMBE_FRAME_SIZE];
        m_pImbe->encode_4400(pcmData.data(), imbe);

        // Push encoded IMBE to output queue (drop oldest if full)
        std::lock_guard<std::mutex> lock(m_ImbeMutex);
        if ( m_ImbeQueue.size() >= SOFTWARE_QUEUE_MAX_DEPTH )
        {
            m_ImbeQueue.pop();
        }
        std::vector<uint8> imbedata(imbe, imbe + IMBE_FRAME_SIZE);
        m_ImbeQueue.push(imbedata);
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// queues helpers

void CVocodecChannel::PurgeAllQueues(void)
{
    GetPacketQueueIn()->Purge();
    ReleasePacketQueueIn();
    GetPacketQueueOut()->Purge();
    ReleasePacketQueueOut();
    GetVoiceQueue()->Purge();
    ReleaseVoiceQueue();

    // Clear Codec2 queue
    {
        std::lock_guard<std::mutex> lock(m_Codec2Mutex);
        while (!m_Codec2Queue.empty())
        {
            m_Codec2Queue.pop();
        }
    }

    // Clear Codec2 PCM queue
    {
        std::lock_guard<std::mutex> lock(m_Codec2PcmMutex);
        while (!m_Codec2PcmQueue.empty())
        {
            m_Codec2PcmQueue.pop();
        }
    }

    // Clear IMBE queue
    {
        std::lock_guard<std::mutex> lock(m_ImbeMutex);
        while (!m_ImbeQueue.empty())
        {
            m_ImbeQueue.pop();
        }
    }

    // Clear IMBE PCM queue
    {
        std::lock_guard<std::mutex> lock(m_ImbePcmMutex);
        while (!m_ImbePcmQueue.empty())
        {
            m_ImbePcmQueue.pop();
        }
    }

    // Clear PID-preservation FIFO and the desync drop counters.
    // Important: any pids left in the FIFO here would mis-pair with
    // future input/output once the channel re-opens, because the
    // hardware response queue and FIFO push/pop would be one or more
    // entries out of step. The counter reset gives each stream a
    // fresh accounting epoch — the close-time stats line reports
    // per-stream desync events, not cumulative since process start.
    {
        std::lock_guard<std::mutex> lock(m_PidFifoMutex);
        while ( !m_PidFifo.empty() )
        {
            m_PidFifo.pop();
        }
        m_PidFifoOrphanDrops = 0;
        m_PidFifoOverflowDrops = 0;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// PID preservation FIFO

// Push a pid + timestamp pair. If the FIFO has reached its memory-
// safety cap (PID_FIFO_MAX_DEPTH), drop the oldest entry first to
// bound RAM growth under sustained input overrun. Logs a one-line
// warning the first time overflow drops fire on this stream so an
// operator notices it in real time, then stays silent until the
// counters reset (PurgeAllQueues at next stream open/close).
void CVocodecChannel::PushPid(uint8 pid)
{
    std::lock_guard<std::mutex> lock(m_PidFifoMutex);

    while ( m_PidFifo.size() >= PID_FIFO_MAX_DEPTH )
    {
        m_PidFifo.pop();
        if ( m_PidFifoOverflowDrops == 0 )
        {
            std::cout << "ambed WARNING: PID FIFO overflow drop on channel "
                      << m_iChannelIn << "->" << m_iChannelOut
                      << " (input rate exceeds DVStick throughput; further "
                      << "drops on this stream will be silent until close)"
                      << std::endl;
        }
        m_PidFifoOverflowDrops++;
    }

    PidEntry e;
    e.pid = pid;
    e.pushedAt = std::chrono::steady_clock::now();
    m_PidFifo.push(e);
}

// Pop the front pid. Before returning, prune any front entries that
// are older than PID_FIFO_TIMEOUT_MS — they're orphans whose
// hardware response never arrived (DVStick drop, transient USB
// fault). Without this the FIFO would be permanently off by one
// after any hardware drop, causing every subsequent response to
// pair with the wrong xlxd-side packet for the rest of the stream.
//
// Mid-window mis-pairing is still possible (a response arriving
// during the timeout window pops the stale front entry). The
// timeout bounds the window to ~PID_FIFO_TIMEOUT_MS rather than
// preventing the mis-pair entirely; see the design review trace
// for why a tighter timeout doesn't help. After the window, the
// FIFO re-aligns and pairing is correct again.
//
// Returns 0 if the FIFO is empty (matches pre-fix default behaviour
// — xlxd's lookup will miss and the response counts as "late",
// audio for the would-be-paired packet is unaffected because it's
// already left the jitter buffer).
uint8 CVocodecChannel::PopPid(void)
{
    std::lock_guard<std::mutex> lock(m_PidFifoMutex);

    auto now = std::chrono::steady_clock::now();
    while ( !m_PidFifo.empty() )
    {
        auto ageMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - m_PidFifo.front().pushedAt).count();
        if ( ageMs <= PID_FIFO_TIMEOUT_MS )
        {
            break;
        }
        // Front entry is orphaned — prune it.
        m_PidFifo.pop();
        if ( m_PidFifoOrphanDrops == 0 )
        {
            std::cout << "ambed WARNING: PID FIFO orphan drop on channel "
                      << m_iChannelIn << "->" << m_iChannelOut
                      << " (DVStick dropped a packet's response; further "
                      << "drops on this stream will be silent until close)"
                      << std::endl;
        }
        m_PidFifoOrphanDrops++;
    }

    if ( m_PidFifo.empty() )
    {
        return 0;
    }
    uint8 pid = m_PidFifo.front().pid;
    m_PidFifo.pop();
    return pid;
}



