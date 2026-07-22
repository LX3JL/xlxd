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
#include <stdio.h>
#include <vector>
#include <chrono>
#include <future>
#include <algorithm>
#include "ccodecstream.h"
#include "cdvframepacket.h"
#include "cpacketstream.h"
#include "creflector.h"

////////////////////////////////////////////////////////////////////////////////////////
// startup timing

// Maximum wall-clock wait for the codec thread to set m_bConnected after
// construction. 500ms is generous — the codec thread's first action is to
// set the flag, so in practice this completes in microseconds.
#define CODEC_STREAM_START_TIMEOUT_MS   500

// Maximum wait for the codec thread to exit on the startup-failure path.
// If it doesn't join in this window, we leak the object rather than hang.
#define CODEC_STREAM_JOIN_TIMEOUT_MS    1000

// Join a codec thread and report whether it exited within a target
// timeout. Returns true if the join completed inside `timeoutMs`, false
// if it took longer.
//
// IMPORTANT: this function ALWAYS waits for the thread to finish — it is
// not a "try-join-with-give-up" primitive. The `std::future` returned by
// `std::async(launch::async, ...)` has a blocking destructor (C++ spec)
// that waits for the shared state to be ready, which means when this
// function returns, the thread has definitively exited regardless of
// the return value. The caller can therefore safely delete the
// std::thread object whether we return true or false.
//
// We deliberately do NOT use a detach-based "true timeout" pattern here
// because the codec thread holds `this` — if we detached and let the
// caller destroy the CCodecStream, the still-running thread would
// perform use-after-free. Blocking-until-thread-exits is memory-safe;
// the timeout is a pure indicator for logging.
//
// In practice the codec thread's loop is bounded by its 5ms socket
// receive, so this returns quickly once `m_bStopThread` is set. A
// long wait here indicates a system-level problem.
static bool BoundedJoin(std::thread *t, int timeoutMs)
{
    if ( t == NULL ) return true;
    if ( !t->joinable() ) return true;

    auto future = std::async(std::launch::async, [t]() { t->join(); });
    return future.wait_for(std::chrono::milliseconds(timeoutMs)) ==
           std::future_status::ready;
}

////////////////////////////////////////////////////////////////////////////////////////
// define



////////////////////////////////////////////////////////////////////////////////////////
// constructor

CCodecStream::CCodecStream(CPacketStream *PacketStream, uint16 uiId, uint8 uiCodecIn, uint8 uiCodecOut1, uint8 uiCodecOut2, uint8 uiCodecOut3)
{
    m_bStopThread = false;
    m_pThread = NULL;
    m_uiStreamId = uiId;
    m_uiPid = 0;
    m_uiCodecIn = uiCodecIn;
    m_uiCodecOut1 = uiCodecOut1;
    m_uiCodecOut2 = uiCodecOut2;
    m_uiCodecOut3 = uiCodecOut3;
    m_bConnected = false;
    m_fPingMin = -1;
    m_fPingMax = -1;
    m_fPingSum = 0;
    m_fPingCount = 0;
    m_uiTotalPackets = 0;
    m_uiTimeoutPackets = 0;
    m_uiReturnedPackets = 0;
    m_uiResponseLookupMisses = 0;
    m_uiUnfilledReleases = 0;
    m_PacketStream = PacketStream;
    m_bJitterBufferStarted = false;
    m_bFirstReleaseDone = false;
    m_iInFlightPackets = 0;
    m_bSlowDataInit = false;
    m_uiSlowDataCycle = 0;
    m_pLastVoiceInJitter = NULL;

    // Adaptive jitter buffer state — fresh per stream, no
    // cross-stream learning. Cold-start at JITTER_BUFFER_DELAY_MS
    // (the safe default); RecomputeJitterTarget will adapt downward
    // toward MIN_JITTER_DELAY_MS once the warmup samples accumulate
    // and conditions are observed to be benign.
    m_RttSamplesUs.fill(0);
    m_RttSampleIdx = 0;
    m_RttSampleCount = 0;
    m_ArrivalSamplesUs.fill(0);
    m_ArrivalSampleIdx = 0;
    m_ArrivalSampleCount = 0;
    m_bHasLastPushTime = false;
    m_CurrentJitterDelayMs = JITTER_BUFFER_DELAY_MS;
    m_FramesSinceRecompute = 0;
    m_AgreeingDownAdapts = 0;
    m_TargetMin = 0;
    m_TargetMax = 0;
    m_TargetSum = 0;
    m_TargetSamples = 0;
    m_uiOverrunDrops = 0;

    // Diagnostic instrumentation — fresh per stream.
    m_uiPeakBufferOccupancy = 0;
    m_uiArrivalMaxUs = 0;
    m_uiFirstDropFrame = 0;
    m_uiLastDropFrame = 0;
}

////////////////////////////////////////////////////////////////////////////////////////
// destructor

CCodecStream::~CCodecStream()
{
    // Stop the thread first so it doesn't use the socket or queues.
    // BoundedJoin always waits for the thread to exit; the return value
    // tells us only whether it exited within the expected window.
    m_bStopThread = true;
    if ( !BoundedJoin(m_pThread, CODEC_STREAM_JOIN_TIMEOUT_MS) )
    {
        std::cout << "Warning: codec thread took longer than "
                  << CODEC_STREAM_JOIN_TIMEOUT_MS
                  << "ms to exit at destructor" << std::endl;
    }
    delete m_pThread;
    m_pThread = NULL;

    // close socket only after thread is gone
    m_Socket.Close();

    // empty main queue — these are packets pushed by CPacketStream that
    // never made it through Phase 3 of Task() (i.e. never sent to ambed
    // and never inserted into the jitter buffer). Owned by us, must
    // delete.
    int mainQueueLost = 0;
    while ( !empty() )
    {
        delete front();
        pop();
        mainQueueLost++;
    }

    // empty jitter buffer — these are packets in flight to listeners.
    //
    // First clear the pre-EoT marker tracking pointer (it aliases a
    // packet about to be deleted) and the lookup index (entries are
    // non-owning aliases of the same packets). Order matters: we must
    // not leave dangling pointers in either tracking structure when
    // the delete below runs. m_PendingTranscode.clear() is safe before
    // the deletes because the map doesn't own the packets.
    m_pLastVoiceInJitter = NULL;
    m_PendingTranscode.clear();
    int jitterQueueLost = 0;
    while ( !m_JitterBuffer.empty() )
    {
        delete m_JitterBuffer.front();
        m_JitterBuffer.pop();
        jitterQueueLost++;
    }

    // Log if any packets were lost at stream close. With the post-fix
    // architecture (jitter buffer drains on its 20 ms timer regardless
    // of ambed state) the close drain in CReflector::CloseStream waits
    // for both queues to empty before invoking us, so a non-zero count
    // here means either the drain hit MAX_DRAIN_WAIT_MS (2000 ms cap —
    // pathologically large jitter occupancy) or the destructor was
    // reached on a forced shutdown path. Either way it represents
    // real audio loss to listeners.
    int totalLost = mainQueueLost + jitterQueueLost;
    if ( totalLost > 0 )
    {
        std::cout << "ambed WARNING: " << totalLost << " packet" << (totalLost > 1 ? "s" : "")
                  << " lost at stream close ("
                  << mainQueueLost << " unsent, "
                  << jitterQueueLost << " in jitter buffer)" << std::endl;
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
        // flush any stale packets from previous stream on this port
        CBuffer flushBuffer;
        CIp flushIp;
        while ( m_Socket.Receive(&flushBuffer, &flushIp, 0) != -1 )
        {
            // discard stale packets
        }

        // init timers
        m_TimeoutTimer.Now();

        // Start thread and wait for it to be ready before accepting packets.
        // Use a wall-clock timeout rather than a yield counter so a heavily
        // loaded or virtualised host that doesn't schedule the new thread
        // inside 1000 yields doesn't falsely flag startup as failed.
        m_pThread = new std::thread(CCodecStream::Thread, this);
        const auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::milliseconds(CODEC_STREAM_START_TIMEOUT_MS);
        while ( !m_bConnected && std::chrono::steady_clock::now() < deadline )
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if ( !m_bConnected )
        {
            std::cout << "Error: codec thread failed to start on port UDP" << uiPort
                      << " within " << CODEC_STREAM_START_TIMEOUT_MS << "ms" << std::endl;
            m_bStopThread = true;
            // Bound the join's expected duration. BoundedJoin always waits
            // for the thread to exit; the return value just tells us if it
            // was prompt. We can safely delete either way.
            if ( !BoundedJoin(m_pThread, CODEC_STREAM_JOIN_TIMEOUT_MS) )
            {
                std::cout << "Warning: codec thread on port UDP" << uiPort
                          << " took longer than " << CODEC_STREAM_JOIN_TIMEOUT_MS
                          << "ms to exit during startup-failure cleanup" << std::endl;
            }
            delete m_pThread;
            m_pThread = NULL;
            m_Socket.Close();
            ok = false;
        }
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

    // kill threads; BoundedJoin always waits for the thread to exit
    m_bStopThread = true;
    if ( !BoundedJoin(m_pThread, CODEC_STREAM_JOIN_TIMEOUT_MS) )
    {
        std::cout << "Warning: codec thread took longer than "
                  << CODEC_STREAM_JOIN_TIMEOUT_MS
                  << "ms to exit at Close()" << std::endl;
    }
    delete m_pThread;
    m_pThread = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////
// get

bool CCodecStream::IsEmpty(void) const
{
    // Two queues left after the architectural change: the inherited
    // base queue (packets pushed by CPacketStream waiting for Task()
    // Phase 3 to send to ambed AND insert into the jitter buffer) and
    // m_JitterBuffer (packets in flight to listeners on the 20 ms
    // release timer). m_PendingTranscode is a non-owning index into
    // m_JitterBuffer; checking it would be redundant with checking
    // m_JitterBuffer.
    //
    // Use const_cast since this is a const method but we need the
    // mutex (Task() thread modifies these queues concurrently).
    CCodecStream *pThis = const_cast<CCodecStream *>(this);

    pThis->Lock();
    bool result = empty() && m_JitterBuffer.empty();
    pThis->Unlock();

    // Also check for packets in-flight between jitter buffer and stream push
    // (popped from jitter buffer under codec lock, not yet pushed to stream)
    if ( result && m_iInFlightPackets.load() > 0 )
    {
        result = false;
    }

    // CPacketStream::IsEmpty() checks its own queue before calling us,
    // so we only need to report on the transcoder pipeline queues
    return result;
}

// Count packets currently anywhere in the transcoder pipeline. Mirrors
// the field set IsEmpty() inspects, summed rather than AND-reduced.
// Used by CReflector::CloseStream's drain loop to wait for the jitter
// buffer to flush before tearing down the stream — depth decreases
// monotonically at one packet per JITTER_BUFFER_FRAME_MS (20 ms) once
// the source stops pushing new frames, regardless of ambed state.
size_t CCodecStream::GetDepth(void) const
{
    CCodecStream *pThis = const_cast<CCodecStream *>(this);

    pThis->Lock();
    size_t depth = size() + m_JitterBuffer.size();
    pThis->Unlock();

    // m_iInFlightPackets covers the jitter-pop → stream-push gap (popped
    // from jitter under codec lock, not yet pushed into m_PacketStream).
    // Atomic read, no lock needed.
    int inFlight = m_iInFlightPackets.load();
    if ( inFlight > 0 )
    {
        depth += (size_t)inFlight;
    }
    return depth;
}

////////////////////////////////////////////////////////////////////////////////////////
// slow-data initialisation
//
// Only relevant when we're transcoding *to* AMBE+ (i.e. cross-mode → D-Star).
// Reads the stream's opening header for the source-mode tag (4-byte SUFFIX
// field set by the ingress protocol handler — "NXDN", "DMR ", "YSF ",
// "P25 ", "M17 ", or a user-set 4-char radio ID for native D-Star) and
// composes a 20-character text message of the form:
//
//     "<MODE> via <REFLECTOR> <MODULE>"
//
// Example for an NXDN caller on XLX672 module B:
//
//     "NXDN via XLX672 B   "
//
// 20 chars maximum — D-Star slow-data cannot carry more. If the reflector
// callsign is longer than 6 chars the module may get pushed out. The text
// is forwarded into m_SlowData, whose GetSlowData() then emits the
// XOR-scrambled 3-byte segments on every non-sync frame of the 21-frame
// cycle. A strict Icom RP2C decoder uses this text + repeating sync
// marker to achieve voice-frame lock; without it the AMBE+ stream is
// correct but gets silently muted on reception.

void CCodecStream::InitSlowData(void)
{
    // Defensive fallback. Should never trigger in practice — CCodecStream
    // is always constructed with a valid CPacketStream — but guards against
    // a future refactor that forgets to wire it. When the stream is absent
    // we can't arm the header-sync buffer (no source for the 41-byte header)
    // so only the text buffer is populated and GetSlowData() will emit
    // filler on any header-mode cycles.
    if ( m_PacketStream == NULL )
    {
        char blank[DSTAR_SLOW_DATA_TEXT_LEN + 1];
        ::memset(blank, ' ', DSTAR_SLOW_DATA_TEXT_LEN);
        blank[DSTAR_SLOW_DATA_TEXT_LEN] = '\0';
        m_SlowData.SetText(blank);
        // Cycle counter stays at 0 — cycle 0 = text per the alternation policy.
        return;
    }

    // Compose the 20-char text via the shared helper. The same helper is
    // used by CDcsProtocol::EncodeDvPacket to populate the DCS voice-frame
    // text field at offset 64-83, so slow-data and text-field always carry
    // identical bytes on the wire.
    //
    // No locks required — m_DvHeader is immutable for the stream's lifetime
    // once Open() returns.
    const CDvHeaderPacket &hdr = m_PacketStream->GetDvHeader();
    char text[DSTAR_SLOW_DATA_TEXT_LEN + 1];
    CDStarSlowData::ComposeText(hdr, text);
    text[DSTAR_SLOW_DATA_TEXT_LEN] = '\0';   // SetText strlens its input
    m_SlowData.SetText(text);

    // Arm the header-sync buffer from the same cached header. Native D-Star
    // radios emit this on alternating cycles to give strict receivers a
    // copy of the RF header — without it, g2_link → Icom RP2C silently
    // mutes our cross-mode audio even though AMBE+ decodes correctly on
    // software receivers. See cdstarslowdata.h for format details.
    m_SlowData.SetHeaderSync(hdr);

    // Start at cycle 0 (text) — matches the observed native-radio pattern
    // where the first post-header cycle is always text-mode content.
    m_SlowData.BeginTextCycle();
    m_uiSlowDataCycle = 0;
}

////////////////////////////////////////////////////////////////////////////////////////
// adaptive jitter buffer recompute
//
// Called from Phase 3 every JITTER_RECOMPUTE_FRAMES frames once the
// RTT sample window is at least half-full. Caller must hold the
// CCodecStream::Lock() (we're touching m_CurrentJitterDelayMs and
// the sample arrays which other phases also touch).
//
// Algorithm:
//   1. RTT P95 over the sample window (5th-largest of 100). Robust
//      against single-sample outliers (one cosmic-ray ambed pause
//      shouldn't pin the buffer for the next second).
//   2. Source inter-arrival max over the sample window. Single-late
//      delivery here HAS to be absorbed by the buffer or a frame
//      gets stranded — no other safety net — so use max.
//   3. Target = max(rtt_p95 + safety, arrival_max, MIN_floor),
//      clamped to MAX_ceiling.
//   4. Apply asymmetric hysteresis:
//        - delta > +20 ms (target growing): apply immediately. Network
//          degrading is an emergency; lookup-misses cost listeners audio.
//        - delta < -20 ms (target shrinking): require 3 consecutive
//          recomputes to all want a smaller target. Improving
//          conditions can wait; oversized buffer only adds latency.
//        - within hysteresis: no change, reset down-agree counter.
void CCodecStream::RecomputeJitterTarget(void)
{
    // P95 of RTT samples (count * 95 / 100 in sorted ascending = 5th-
    // largest for count=100). For partial windows during warmup, the
    // formula falls back gracefully (count=20 → idx=19 = max).
    //
    // The m_RttSampleCount > 0 check is defence-in-depth — the call
    // site in Phase 3 already gates on m_RttSampleCount >=
    // JITTER_SAMPLE_WINDOW / 2 (50), so this branch never executes
    // with count=0 in practice. Keep the guard so a future change to
    // the call-site condition can't trigger UB on an empty sample
    // window.
    unsigned int rttP95Us = 0;
    if ( m_RttSampleCount > 0 )
    {
        std::array<uint32, JITTER_SAMPLE_WINDOW> tmp;
        std::copy(m_RttSamplesUs.begin(),
                  m_RttSamplesUs.begin() + m_RttSampleCount,
                  tmp.begin());
        unsigned int idx = (m_RttSampleCount * 95) / 100;
        if ( idx >= m_RttSampleCount ) idx = m_RttSampleCount - 1;
        std::nth_element(tmp.begin(), tmp.begin() + idx,
                         tmp.begin() + m_RttSampleCount);
        rttP95Us = tmp[idx];
    }

    // Max of source inter-arrival samples.
    unsigned int arrivalMaxUs = 0;
    for ( unsigned int i = 0; i < m_ArrivalSampleCount; i++ )
    {
        if ( m_ArrivalSamplesUs[i] > arrivalMaxUs )
        {
            arrivalMaxUs = m_ArrivalSamplesUs[i];
        }
    }

    // Convert to ms (round up — better to over-buffer than under).
    unsigned int rttP95Ms = (rttP95Us + 999) / 1000;
    unsigned int arrivalMaxMs = (arrivalMaxUs + 999) / 1000;

    // Compute new target.
    unsigned int newTarget = rttP95Ms + JITTER_RTT_SAFETY_MARGIN_MS;
    if ( arrivalMaxMs > newTarget )    newTarget = arrivalMaxMs;
    if ( newTarget < MIN_JITTER_DELAY_MS ) newTarget = MIN_JITTER_DELAY_MS;
    if ( newTarget > MAX_JITTER_DELAY_MS ) newTarget = MAX_JITTER_DELAY_MS;

    // Apply with asymmetric hysteresis.
    int delta = (int)newTarget - (int)m_CurrentJitterDelayMs;
    if ( delta > (int)JITTER_RECOMPUTE_HYSTERESIS_MS )
    {
        // Growing — apply immediately. Reset down-agree counter.
        m_CurrentJitterDelayMs = newTarget;
        m_AgreeingDownAdapts = 0;
    }
    else if ( delta < -(int)JITTER_RECOMPUTE_HYSTERESIS_MS )
    {
        // Shrinking — wait for sustained agreement.
        m_AgreeingDownAdapts++;
        if ( m_AgreeingDownAdapts >= JITTER_DOWN_ADAPT_AGREE_COUNT )
        {
            m_CurrentJitterDelayMs = newTarget;
            m_AgreeingDownAdapts = 0;
        }
    }
    else
    {
        // Within hysteresis — no change, reset down-agree counter.
        m_AgreeingDownAdapts = 0;
    }

    // Track stats for the close-time log line.
    if ( m_TargetSamples == 0 || m_CurrentJitterDelayMs < m_TargetMin )
    {
        m_TargetMin = m_CurrentJitterDelayMs;
    }
    if ( m_CurrentJitterDelayMs > m_TargetMax )
    {
        m_TargetMax = m_CurrentJitterDelayMs;
    }
    m_TargetSum += m_CurrentJitterDelayMs;
    m_TargetSamples++;
}

////////////////////////////////////////////////////////////////////////////////////////
// thread

void CCodecStream::Thread(CCodecStream *This)
{
    // Signal that the thread is running and ready to process packets.
    // Init() spin-waits on this flag before returning, ensuring no packets
    // bypass the transcoder due to a race between thread startup and
    // incoming voice frames (e.g. NXDN's 4-frame batches).
    This->m_bConnected = true;

    while ( !This->m_bStopThread )
    {
        This->Task();
    }
}

// Architectural note for the four phases below:
//
// ambed is treated as ENRICHMENT, not a GATE. Voice frames flow through
// the jitter buffer to listeners on a fixed 20 ms cadence regardless of
// what ambed is doing. ambed responses fill in transcoded codec slots
// (Codec2 / IMBE / AMBE+2) in-place while the packet still sits in the
// jitter buffer; if a response arrives in time the slots are filled, if
// not the slots stay zero and other-mode listeners hear ~20 ms of
// silence for that frame. The source-codec slot (e.g. AMBE+ for a
// D-Star source) is filled at Push time and is independent of ambed —
// so D-Star pass-through audio reaches D-Star listeners in lockstep
// regardless of UDP loss to ambed, ambed being slow, or ambed being
// completely offline.
//
// Pre-fix (commit history before this change), packets entered the
// jitter buffer ONLY after ambed responded. UDP loss to/from ambed
// caused the packet to pile up in m_LocalQueue, never reach the jitter
// buffer, and be deleted at stream close — silently destroying the
// source AMBE+ data that D-Star listeners didn't need ambed to play.
// Symptoms were "broken tails" (last 1-3 frames missing on D-Star) and
// elevated reported BER on D-Star receivers (FEC alignment errors at
// the abrupt cut-off). See git history of this file for context.
//
// The four phases of Task():
//   Phase 1 — handle ambed response if any (lookup-fill, no queue ops).
//   Phase 2 — release one or more packets from jitter buffer to the
//             packet stream on the 20 ms timer.
//   Phase 3 — drain the inbound queue: synthesise slow-data, track the
//             pre-EoT marker candidate, push the packet into the jitter
//             buffer + lookup index, send the AMBE+ data to ambed.
//   Phase 4 — bookkeeping for slow-ambed stats.
//
// All shared state (m_JitterBuffer, m_PendingTranscode, m_pLastVoiceInJitter,
// m_uiReturnedPackets, m_uiResponseLookupMisses, m_uiUnfilledReleases) is
// protected by CCodecStream::Lock(). The single-threaded counters touched
// only here in Task() (m_uiTotalPackets, m_uiTimeoutPackets, m_uiPid,
// m_StatsTimer, m_TimeoutTimer, m_SlowData, m_uiSlowDataCycle) are not
// locked because Task() runs on a single thread.

void CCodecStream::Task(void)
{
    CBuffer Buffer;
    CIp     Ip;
    uint8   Ambe1[AMBE_FRAME_SIZE];
    uint8   Ambe2[AMBE_FRAME_SIZE];
    uint8   Ambe3[IMBE_SIZE];
    uint8   DStarSync[] = { 0x55,0x2D,0x16 };

    // ---- Phase 1: handle ambed response (lookup + slot-fill in place) ----
    //
    // ambed echoes the per-packet PID at offset 1 of the response. Look
    // up the packet in m_PendingTranscode by that PID. If found and still
    // in the jitter buffer, fill the transcoded slots in-place. If not
    // found (the jitter timer has already released the packet), the
    // response is too late — count for the health metric and discard.
    //
    // PID-keyed lookup also fixes a latent bug in the previous FIFO-pop
    // design: a single dropped ambed return packet would silently
    // mis-pair every subsequent packet's transcoded slots until end of
    // stream. With keyed lookup, each response goes to its own packet
    // regardless of arrival order or interleaved drops.
    if ( m_Socket.Receive(&Buffer, &Ip, 5) != -1 )
    {
        if ( IsValidAmbePacket(Buffer, Ambe1, Ambe2, Ambe3) )
        {
            m_TimeoutTimer.Now();

            // update ping statistics
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

            uint8 responsePid = Buffer.data()[TRANSCODER_PACKET_PID_OFFSET];

            Lock();

            // Push RTT into the adaptive jitter buffer's sliding-window
            // sample ring. ping is in seconds (per CTimePoint); convert
            // to microseconds for storage. uint32 fits ~71 minutes —
            // way more than any sane RTT.
            //
            // Guard against negative ping: m_StatsTimer uses steady_clock
            // so ping should never go backwards, but if a future change
            // ever switches to system_clock and the wall clock steps
            // backwards mid-stream, the negative cast to uint32 becomes
            // a huge value and pins the buffer at the ceiling for ~2
            // seconds. Cheap guard.
            double pingClamped = (ping > 0.0) ? ping : 0.0;
            uint32 pingUs = (uint32)(pingClamped * 1.0e6);
            m_RttSamplesUs[m_RttSampleIdx] = pingUs;
            m_RttSampleIdx = (m_RttSampleIdx + 1) % JITTER_SAMPLE_WINDOW;
            if ( m_RttSampleCount < JITTER_SAMPLE_WINDOW )
            {
                m_RttSampleCount++;
            }

            auto it = m_PendingTranscode.find(responsePid);
            if ( it != m_PendingTranscode.end() )
            {
                CDvFramePacket *Packet = it->second;
                // Per-slot Has-flag guard. Slots that arrived
                // pre-populated from a multi-codec wire frame
                // (XLX interlink in particular — see CXlxProtocol
                // ::IsValidDvFramePacket for the rev 2/3 cases that
                // set m_bHasAmbe and m_bHasAmbePlus) must NOT be
                // overwritten by re-transcoded versions: doing so
                // would add an extra vocoder hop and degrade
                // downstream audio quality.
                //
                // We only requested transcoding because some
                // codec(s) we need are missing; the rest of the
                // outputs from ambed are byproducts of the
                // 3-output transcoder API and should be discarded
                // when the slot already has authoritative data.
                // Field-naming caveat (legacy):
                //   HasAmbeData()     ↔ m_bHasAmbe     ↔ AMBE+  (D-Star)
                //   HasAmbePlusData() ↔ m_bHasAmbePlus ↔ AMBE+2 (DMR family)
                // The "Plus" suffix in m_uiAmbePlus refers to "+2", not
                // "+". Easy to misread; don't swap them.
                auto fillIfMissing = [&](uint8 codec, uint8 *data) {
                    switch (codec)
                    {
                        case CODEC_AMBEPLUS:
                            if ( !Packet->HasAmbeData() )
                                Packet->SetAmbe(codec, data);
                            break;
                        case CODEC_AMBE2PLUS:
                            if ( !Packet->HasAmbePlusData() )
                                Packet->SetAmbe(codec, data);
                            break;
                        case CODEC_CODEC2:
                            if ( !Packet->HasCodec2Data() )
                                Packet->SetAmbe(codec, data);
                            break;
                        case CODEC_IMBE:
                            if ( !Packet->HasImbeData() )
                                Packet->SetAmbe(codec, data);
                            break;
                    }
                };
                fillIfMissing(m_uiCodecOut1, Ambe1);
                fillIfMissing(m_uiCodecOut2, Ambe2);
                fillIfMissing(m_uiCodecOut3, Ambe3);
                m_PendingTranscode.erase(it);
            }
            else
            {
                // Late response — packet was already released by the
                // jitter timer with empty transcoded slots. Other-mode
                // listeners heard ~20 ms of silence for that frame.
                // D-Star pass-through audio was unaffected. Count for
                // the close-time health log.
                m_uiResponseLookupMisses++;
            }
            Unlock();
        }
    }

    // ---- Phase 2: release jitter-buffer packets to the packet stream ----
    //
    // Fixed 20 ms cadence. Independent of ambed state — packets release
    // whether ambed responded or not. When a packet pops, also clean up
    // its m_PendingTranscode entry if still present (means ambed never
    // responded — count as an unfilled release for the close-time log).
    //
    // Two-step lock pattern (collect under Lock, push without Lock) is
    // unchanged from the previous design — avoids a potential
    // CCodecStream::Lock() → CPacketStream::Lock() inversion.
    std::vector<CPacket *> toRelease;
    Lock();
    if ( m_bJitterBufferStarted && !m_JitterBuffer.empty() )
    {
        // Overrun protection: if the jitter buffer is bloating beyond
        // the target capacity by JITTER_OVERRUN_RATIO_NUM /
        // JITTER_OVERRUN_RATIO_DEN (currently 2.0x), the source has
        // been delivering faster than the buffer can drain. Drop
        // oldest packets back toward the target. Listener hears one
        // 20 ms cadence stutter per drop; alternative would be
        // permanent latency growth.
        //
        // Drop-OLDEST chosen over drop-newest because (a) old audio
        // has already been delayed by m_CurrentJitterDelayMs so it's
        // closer to "stale" than freshly-arrived audio, and (b) drop-
        // newest would corrupt cadence at the front of the queue.
        //
        // Cleanup mirrors the release path: clear m_pLastVoiceInJitter
        // if it aliases the dropped packet, erase the m_PendingTranscode
        // entry if present (otherwise ambed's eventual response would
        // count as a lookup-miss for a packet that never reached
        // listeners). Increment m_uiOverrunDrops for the close-time
        // diagnostic.
        //
        // Initial-fill exemption: while m_bFirstReleaseDone is false
        // we're still in the JITTER_BUFFER_DELAY_MS warmup window
        // before any packet has been released to listeners. The
        // buffer is allowed to grow during this window because the
        // pre-codec ring buffer in CPacketStream (capped at 100
        // frames, ~2 s of audio) deliberately replays accumulated
        // frames in microseconds when AttachCodecStream completes
        // after the AMBEd handshake. That replay burst is *intended*
        // to preserve audio across the handshake delay; running
        // overrun protection during it self-defeats — listener gets
        // stutters instead of the deferred-but-intact audio we
        // designed for. Once Phase 2 has released anything we know
        // we're past the handshake replay and overrun is meaningful.
        //
        // Drop cap is JITTER_OVERRUN_DROP_CAP (8) per Phase 2 tick.
        // Intentionally smaller than worst-case excess so a single
        // large overflow bleeds across multiple ticks rather than
        // creating a single huge cadence-stutter event. See the
        // constant's header comment for the trade-off rationale.
        //
        // Threshold computation: hybrid of multiplicative ratio and
        // additive burst headroom — see JITTER_OVERRUN_RATIO_* and
        // JITTER_OVERRUN_BURST_HEADROOM in the header for the full
        // rationale. The larger of the two paths wins.
        //
        // Ratio path: ms first, then divide by frame-ms. Doing it the
        // other way around (packets first, then ratio) truncates twice
        // and drifts away from the nominal ratio for any target that
        // isn't a multiple of 20 ms.
        //
        // Additive path: target_pkts is the integer-truncated target.
        // We add headroom in packets directly. Truncation is one-sided
        // (down) so the threshold is at most 1 packet looser than the
        // continuous-math version — same direction as the ratio path
        // and acceptable for the same reason.
        //
        // Crossover point where the two paths agree:
        //   target_pkts * NUM/DEN == target_pkts + HEADROOM
        //   => target_pkts == HEADROOM * DEN / (NUM - DEN)
        //   => at NUM=2,DEN=1,HEADROOM=12: target_pkts = 12 (240 ms).
        // Below crossover, additive dominates (low-target burst
        // protection); above crossover, ratio dominates (large-target
        // bound on absolute peak buffered audio).
        if ( m_bFirstReleaseDone )
        {
            size_t targetPkts = m_CurrentJitterDelayMs / JITTER_BUFFER_FRAME_MS;
            size_t overrunRatioMs =
                ((size_t)m_CurrentJitterDelayMs * JITTER_OVERRUN_RATIO_NUM) /
                JITTER_OVERRUN_RATIO_DEN;
            size_t ratioPkts = overrunRatioMs / JITTER_BUFFER_FRAME_MS;
            size_t additivePkts = targetPkts + JITTER_OVERRUN_BURST_HEADROOM;
            size_t overrunThreshold = std::max<size_t>(ratioPkts, additivePkts);
            // Belt-and-braces: if anyone ever lowers MIN_JITTER_DELAY_MS
            // below FRAME_MS, the ratio path floors to zero. The
            // additive path's +HEADROOM keeps overall threshold above
            // zero in practice, but keep the guard for any future
            // change that lowers headroom too.
            if ( overrunThreshold == 0 ) overrunThreshold = 1;
            int drops = 0;
            while ( m_JitterBuffer.size() > overrunThreshold &&
                    drops < JITTER_OVERRUN_DROP_CAP )
            {
                CPacket *oldest = m_JitterBuffer.front();

                if ( oldest == m_pLastVoiceInJitter )
                {
                    m_pLastVoiceInJitter = NULL;
                }

                for ( auto mIt = m_PendingTranscode.begin();
                      mIt != m_PendingTranscode.end(); ++mIt )
                {
                    if ( mIt->second == (CDvFramePacket *)oldest )
                    {
                        m_PendingTranscode.erase(mIt);
                        break;
                    }
                }

                m_JitterBuffer.pop();
                delete oldest;
                m_uiOverrunDrops++;

                // Record drop position for diagnostics. m_uiTotalPackets
                // is the count of packets pushed to ambed so far — i.e.
                // "drop occurred after the Nth sent frame." First non-zero
                // sets first; every drop overwrites last. Tells us at a
                // glance whether drops cluster at start, mid, or end.
                if ( m_uiFirstDropFrame == 0 )
                {
                    m_uiFirstDropFrame = m_uiTotalPackets;
                }
                m_uiLastDropFrame = m_uiTotalPackets;

                drops++;
            }
        }

        auto now = std::chrono::steady_clock::now();
        int released = 0;
        // Cap at 3 releases per call to prevent bursting on catch-up
        while ( !m_JitterBuffer.empty() && now >= m_NextReleaseTime && released < 3 )
        {
            CPacket *p = m_JitterBuffer.front();

            // Clear the pre-EoT tracking pointer if it aliases this
            // packet — once a packet leaves the jitter buffer we can
            // no longer safely mutate its slow-data (it may be handed
            // off, freed, or on the wire by the time a later EoT
            // arrives).
            if ( p == m_pLastVoiceInJitter )
            {
                m_pLastVoiceInJitter = NULL;
            }

            // Erase the lookup entry if still present. If still here, it
            // means ambed never responded for this packet — the
            // transcoded slots stayed zero. Count as an unfilled
            // release for the close-time health log. Map size is
            // bounded ~8 at steady state, so the linear scan is cheap.
            //
            // The `break` after the first match is correct under the
            // unique-target invariant: each packet pointer can appear
            // at most ONCE in m_PendingTranscode, because every insert
            // (Phase 3 below, line `m_PendingTranscode[sendPid] = fp`)
            // uses a distinct PID key for a distinct packet, and PID
            // wraparound is bounded by ~256 frames while jitter
            // occupancy is ~50 frames at worst — collision is
            // impossible in the live working set.
            for ( auto mIt = m_PendingTranscode.begin(); mIt != m_PendingTranscode.end(); ++mIt )
            {
                if ( mIt->second == (CDvFramePacket *)p )
                {
                    m_PendingTranscode.erase(mIt);
                    m_uiUnfilledReleases++;
                    break;
                }
            }

            toRelease.push_back(p);
            m_JitterBuffer.pop();
            m_uiReturnedPackets++;
            m_NextReleaseTime += std::chrono::milliseconds(JITTER_BUFFER_FRAME_MS);
            released++;
        }
        // Mark packets as in-flight before releasing the lock
        m_iInFlightPackets += (int)toRelease.size();

        // Latch first-release-done. Gates the overrun-drop block above
        // so the initial pre-codec replay burst (which can dump up to
        // PRE_CODEC_BUFFER_MAX = 100 frames into the buffer in micro-
        // seconds following the AMBEd handshake) isn't pruned. Once
        // Phase 2 has released anything we're past the replay window
        // and overrun protection becomes meaningful.
        if ( !m_bFirstReleaseDone && !toRelease.empty() )
        {
            m_bFirstReleaseDone = true;
        }
    }
    Unlock();

    // Push to packet stream without holding CCodecStream lock
    for ( CPacket *p : toRelease )
    {
        m_PacketStream->Lock();
        m_PacketStream->push(p);
        m_PacketStream->Unlock();
        m_iInFlightPackets--;
    }

    // ---- Phase 3: drain main queue, synthesise slow-data, push to ----
    // ----          jitter buffer + lookup index, send to ambed.       ----
    //
    // The slow-data synthesis and pre-EoT marker tracking that used to
    // live in Phase 1 (the response handler) have moved here, because
    // the packet enters the jitter buffer in this phase rather than in
    // Phase 1. Both writes are to fields the listener-side egress reads
    // — they have to happen before the packet is visible to the jitter
    // release timer, OR they have to be guarded against the release
    // taking a half-written packet. The cleanest approach is to do
    // them here, before the m_JitterBuffer.push() inside the lock.
    //
    // The PID we send to ambed is captured BEFORE EncodeAmbePacket runs
    // (which increments m_uiPid as a side effect). That captured value
    // is the lookup key used by Phase 1 when ambed's response arrives.
    std::vector<CPacket *> toSend;
    Lock();
    while ( !empty() )
    {
        toSend.push_back(front());
        pop();
    }
    Unlock();

    for ( CPacket *Packet : toSend )
    {
        CDvFramePacket *fp = (CDvFramePacket *)Packet;
        m_StatsTimer.Now();
        m_uiTotalPackets++;

        // ---- D-Star slow-data synthesis (cross-mode → AMBE+ egress) ----
        //
        // A strict D-Star decoder (Icom RP2C / G3 hardware) silently
        // mutes the AMBE+ audio if it can't lock onto the 21-frame
        // slow-data cycle. Software decoders (ircDDBGateway, MMDVM)
        // tolerate missing slow-data; Icom hardware does not.
        //
        // Wire format produced here (see cdstarslowdata.h for detail):
        //   frame 0  of cycle : sync marker 0x55 0x2D 0x16 (unscrambled)
        //   cycle 0 — TEXT cycle (emitted exactly once per transmission):
        //     frames 1..8  : 4 × 6-byte scrambled text segments
        //     frames 9..20 : null-fill 0x16 0x29 0xF5
        //   cycles 1+ — HEADER-SYNC cycle (emitted on every subsequent cycle):
        //     frames 1..18 : 9 × 6-byte scrambled header-sync elements
        //     frames 19..20: null-fill
        //
        // Policy "text once at cycle 0, header-sync continuously
        // thereafter" matches the native D-Star radio pattern confirmed
        // by multi-cycle pcap of a Pi-Star source. Strict Icom RP2C
        // hardware (via g2_link) appears to validate header-sync
        // continuity and mutes audio if it sees cycles without it.
        //
        // m_SlowData and m_uiSlowDataCycle are touched only here in
        // Phase 3 of Task() — single-threaded, no lock needed.
        if ( m_uiCodecOut1 == CODEC_AMBEPLUS || m_uiCodecOut2 == CODEC_AMBEPLUS )
        {
            // Lazy-init on the first AMBE+ output frame of the stream —
            // the header is guaranteed to be cached in the packet
            // stream by this point.
            if ( !m_bSlowDataInit )
            {
                InitSlowData();
                m_bSlowDataInit = true;
            }

            uint8 dvdata[3];
            if ( (fp->GetPacketId() % 21) == 0 )
            {
                // Sync frame — emit the unscrambled 21-frame sync
                // marker and select slow-data mode for the 20 non-
                // sync frames that follow.
                ::memcpy(dvdata, DStarSync, sizeof(dvdata));
                if ( m_uiSlowDataCycle == 0U )
                {
                    // Cycle 0: text message, emitted once.
                    m_SlowData.BeginTextCycle();
                }
                else
                {
                    // Cycles 1+: header-sync, emitted continuously.
                    m_SlowData.BeginHeaderCycle();
                }
                m_uiSlowDataCycle++;
            }
            else
            {
                m_SlowData.GetSlowData(dvdata);
            }
            fp->SetDvData(dvdata);
        }

        // Capture the PID we'll send to ambed BEFORE EncodeAmbePacket
        // increments m_uiPid as a side effect. This is the key for the
        // m_PendingTranscode lookup when ambed's response arrives.
        uint8 sendPid = m_uiPid;

        Lock();

        // ---- Pre-EoT marker tracking (universal, all egress paths) ----
        //
        // Native D-Star radios emit slow-data wire bytes 55 55 55 on
        // the voice frame immediately before the EoT frame; strict
        // Icom decoders use this as a "stream is closing cleanly"
        // signal. For the streams that need this synthesised
        // (cross-mode AMBE+ output AND XLX-interlinked AMBE+ pass-
        // through whose source bridge doesn't emit the marker
        // naturally), we mutate the most-recent voice frame's DvData
        // in-place when the EoT arrives. For non-D-Star egress
        // (DMR, M17, P25, NXDN, YSF) the egress encoder doesn't
        // read m_uiDvData at all — the override is harmless on
        // those paths.
        //
        // m_pLastVoiceInJitter aliases a packet that lives in
        // m_JitterBuffer; cleared on jitter-pop (Phase 2 above) and
        // here on EoT consumption.
        if ( fp->IsLastPacket() )
        {
            if ( m_pLastVoiceInJitter != NULL )
            {
                static const uint8 preEoTSlowData[3] = { 0x55, 0x55, 0x55 };
                m_pLastVoiceInJitter->SetDvData(
                    const_cast<uint8 *>(preEoTSlowData));
                m_pLastVoiceInJitter = NULL;
            }
        }
        else
        {
            // Regular voice frame. Track it as the most recent
            // override candidate; superseded by each subsequent
            // voice frame, cleared on jitter-pop or on EoT
            // consumption.
            m_pLastVoiceInJitter = fp;
        }

        // Detect empty-buffer-resume BEFORE the push for the
        // adaptive-jitter re-arm below. "Empty" means both the jitter
        // buffer is empty AND no map entries are pending (defence-in-
        // depth: a stranded map entry from some bug would otherwise
        // suppress the re-arm and re-create the silence-gap pathology).
        bool wasEmpty = m_JitterBuffer.empty() && m_PendingTranscode.empty();

        // Source inter-arrival sample for the adaptive jitter buffer.
        // Skip the first push of the stream (no previous time to delta
        // against) and skip the first push after a silence gap (the
        // delta would be the gap duration, which is signal noise rather
        // than jitter — we don't want a 1-second silence to pin the
        // buffer at 1000 ms for the next minute).
        auto pushNow = std::chrono::steady_clock::now();
        if ( m_bHasLastPushTime && !wasEmpty )
        {
            auto deltaUs = std::chrono::duration_cast<std::chrono::microseconds>(
                                pushNow - m_LastPushTime).count();
            // Clamp to a sane range. Negative shouldn't happen on
            // steady_clock, but bounds-check anyway. Upper bound
            // suppresses stalls (silence gaps, source PTT release)
            // from looking like enormous jitter — see
            // JITTER_ARRIVAL_MAX_SAMPLE_US comment in the header.
            if ( deltaUs > 0 && deltaUs < JITTER_ARRIVAL_MAX_SAMPLE_US )
            {
                m_ArrivalSamplesUs[m_ArrivalSampleIdx] = (uint32)deltaUs;
                m_ArrivalSampleIdx = (m_ArrivalSampleIdx + 1) % JITTER_SAMPLE_WINDOW;
                if ( m_ArrivalSampleCount < JITTER_SAMPLE_WINDOW )
                {
                    m_ArrivalSampleCount++;
                }
                // Diagnostic: live max of admitted (non-stall) inter-
                // arrival deltas across the entire stream. The ring
                // buffer that feeds RecomputeJitterTarget rotates and
                // can lose the historical peak; this field is monotonic
                // so the close-time stats line shows the worst burst
                // gap observed during the whole stream.
                if ( (uint32)deltaUs > m_uiArrivalMaxUs )
                {
                    m_uiArrivalMaxUs = (uint32)deltaUs;
                }
            }
        }
        m_LastPushTime = pushNow;
        m_bHasLastPushTime = true;

        // Push into jitter buffer + lookup index. Both happen under
        // the same lock so a concurrent Phase 1 response handler can't
        // see the map entry without the corresponding jitter-buffer
        // entry (would otherwise race a "lookup-found, packet not in
        // jitter" inconsistency).
        m_JitterBuffer.push(Packet);
        m_PendingTranscode[sendPid] = fp;

        // Diagnostic: track peak jitter-buffer occupancy seen during
        // the stream. Sampled here (immediately after every push,
        // before Phase 2 has had a chance to drain) — this is the
        // post-push peak for each frame, which is the most relevant
        // depth for sizing overrun thresholds.
        if ( m_JitterBuffer.size() > m_uiPeakBufferOccupancy )
        {
            m_uiPeakBufferOccupancy = (uint32)m_JitterBuffer.size();
        }

        // Establish the jitter release schedule. Two cases:
        //   - First push of the stream (m_bJitterBufferStarted false):
        //     start the cadence at now + the current adaptive target.
        //   - Push to a previously-empty buffer (silence-gap recovery):
        //     re-arm to now + current target. Without this re-arm,
        //     m_NextReleaseTime would still be at the time-of-last-pop
        //     (frozen during the gap because Phase 2 only advances it
        //     when popping, and during silence there's nothing to pop).
        //     Post-gap frames would then match `now >= m_NextReleaseTime`
        //     immediately and pop without sitting in the jitter buffer
        //     at all, bypassing the entire absorption window for the
        //     rest of the stream.
        if ( !m_bJitterBufferStarted || wasEmpty )
        {
            m_NextReleaseTime = pushNow +
                                std::chrono::milliseconds(m_CurrentJitterDelayMs);
            m_bJitterBufferStarted = true;
        }

        // Recompute the adaptive target every JITTER_RECOMPUTE_FRAMES
        // pushes once the warmup window is at least half-full of RTT
        // samples. The half-full threshold (50 of 100) ensures the
        // statistic is meaningful before we start shrinking from the
        // safe default.
        m_FramesSinceRecompute++;
        if ( m_FramesSinceRecompute >= JITTER_RECOMPUTE_FRAMES &&
             m_RttSampleCount >= JITTER_SAMPLE_WINDOW / 2 )
        {
            RecomputeJitterTarget();
            m_FramesSinceRecompute = 0;
        }

        Unlock();

        // Send to ambed. EncodeAmbePacket reads m_uiPid (matching the
        // sendPid we captured above) and increments it after appending.
        // Both happen outside the lock — m_uiPid is touched only here
        // in Phase 3 (single thread).
        const uint8 *ambeData = fp->GetAmbe(m_uiCodecIn);
        if ( ambeData != NULL )
        {
            EncodeAmbePacket(&Buffer, ambeData);
            m_Socket.Send(Buffer, m_Ip, m_uiPort);
        }
        else
        {
            // No source-codec data on this packet — should never
            // happen for a valid voice frame. The packet still proceeds
            // through the jitter buffer with empty transcoded slots
            // (the m_PendingTranscode entry will be erased at jitter-
            // pop with an unfilled-release count). m_uiPid is NOT
            // incremented because we never called EncodeAmbePacket;
            // the next valid packet picks up the same sendPid + 1
            // numbering, which is harmless because the inserted map
            // entry will never be matched (no response will arrive
            // for a request we never sent).
        }
    }

    // ---- Phase 4: timeout bookkeeping ----
    //
    // Count slow ambed responses for stats. m_PendingTranscode entries
    // older than TRANSCODER_AMBEPACKET_TIMEOUT count as "ambed is slow"
    // signals. This is informational — the jitter timer continues to
    // release packets regardless, so a slow ambed never blocks audio.
    if ( !m_PendingTranscode.empty() &&
         (m_TimeoutTimer.DurationSinceNow() >= (TRANSCODER_AMBEPACKET_TIMEOUT/1000.0f)) )
    {
        m_uiTimeoutPackets++;
        m_TimeoutTimer.Now();
    }
}

////////////////////////////////////////////////////////////////////////////////////////
/// packet decoding helpers

bool CCodecStream::IsValidAmbePacket(const CBuffer &Buffer, uint8 *Ambe1, uint8 *Ambe2, uint8 *Ambe3)
{
    bool valid = false;

    // New 3-codec response format:
    // codec1(1) + pid(1) + ambe1(9) + codec2(1) + codec2_data(8) + codec3(1) + imbe(11) = 32 bytes
    if ( Buffer.size() == TRANSCODER_PACKET_SIZE_3CODEC )
    {
        uint8 codec1 = Buffer.data()[TRANSCODER_PACKET_CODEC_OFFSET];
        uint8 codec2 = Buffer.data()[TRANSCODER_PACKET_CODEC2_OFFSET];
        uint8 codec3 = Buffer.data()[TRANSCODER_PACKET_CODEC3_OFFSET];

        // verify codecs match what we requested
        if ( codec1 == m_uiCodecOut1 && codec2 == m_uiCodecOut2 && codec3 == m_uiCodecOut3 )
        {
            // first output is 9 bytes (AMBE+ or AMBE2+)
            ::memcpy(Ambe1, &(Buffer.data()[TRANSCODER_PACKET_AMBE1_OFFSET]), AMBE_FRAME_SIZE);
            // second output is 8 bytes (Codec2)
            ::memcpy(Ambe2, &(Buffer.data()[TRANSCODER_PACKET_AMBE2_OFFSET]), CODEC2_FRAME_SIZE);
            // third output is 11 bytes (IMBE)
            ::memcpy(Ambe3, &(Buffer.data()[TRANSCODER_PACKET_AMBE3_OFFSET]), IMBE_SIZE);
            valid = true;
        }
    }
    // IMBE input response: codec1(1) + pid(1) + ambe1(9) + codec2(1) + ambe2(9) + codec3(1) + codec2_data(8) = 30 bytes
    // Outputs are AMBE+, AMBE2+, and Codec2 for D-Star, DMR, M17
    // Note: IMBEIN has different offsets than 3CODEC because ambe2 is 9 bytes (vs 8 byte codec2_data in 3CODEC)
    //   IMBEIN: codec3 at offset 21, codec2_data at offset 22
    //   3CODEC: codec3 at offset 20, imbe at offset 21
    else if ( Buffer.size() == TRANSCODER_PACKET_SIZE_IMBEIN )
    {
        uint8 codec1 = Buffer.data()[TRANSCODER_PACKET_CODEC_OFFSET];
        uint8 codec2 = Buffer.data()[TRANSCODER_PACKET_CODEC2_OFFSET];
        uint8 codec3 = Buffer.data()[21];  // IMBEIN: codec3 at offset 21 (after 9-byte ambe2)

        // verify codecs match what we requested
        if ( codec1 == m_uiCodecOut1 && codec2 == m_uiCodecOut2 && codec3 == m_uiCodecOut3 )
        {
            // first output is 9 bytes (AMBE+)
            ::memcpy(Ambe1, &(Buffer.data()[TRANSCODER_PACKET_AMBE1_OFFSET]), AMBE_FRAME_SIZE);
            // second output is 9 bytes (AMBE2+)
            ::memcpy(Ambe2, &(Buffer.data()[TRANSCODER_PACKET_AMBE2_OFFSET]), AMBE_FRAME_SIZE);
            // third output is 8 bytes (Codec2) at offset 22
            ::memcpy(Ambe3, &(Buffer.data()[22]), CODEC2_FRAME_SIZE);
            valid = true;
        }
    }
    // Codec2 input 3-codec response: codec1(1) + pid(1) + ambe1(9) + codec2(1) + ambe2(9) + codec3(1) + imbe(11) = 33 bytes
    // Outputs are AMBE+, AMBE2+, and IMBE for D-Star, DMR, P25
    else if ( Buffer.size() == TRANSCODER_PACKET_SIZE_C2IN_3CODEC )
    {
        uint8 codec1 = Buffer.data()[TRANSCODER_PACKET_CODEC_OFFSET];
        uint8 codec2 = Buffer.data()[TRANSCODER_PACKET_CODEC2_OFFSET];
        uint8 codec3 = Buffer.data()[21];  // C2IN_3CODEC: codec3 at offset 21 (after 9-byte ambe2)

        // verify codecs match what we requested
        if ( codec1 == m_uiCodecOut1 && codec2 == m_uiCodecOut2 && codec3 == m_uiCodecOut3 )
        {
            // first output is 9 bytes (AMBE+)
            ::memcpy(Ambe1, &(Buffer.data()[TRANSCODER_PACKET_AMBE1_OFFSET]), AMBE_FRAME_SIZE);
            // second output is 9 bytes (AMBE2+)
            ::memcpy(Ambe2, &(Buffer.data()[TRANSCODER_PACKET_AMBE2_OFFSET]), AMBE_FRAME_SIZE);
            // third output is 11 bytes (IMBE) at offset 22
            ::memcpy(Ambe3, &(Buffer.data()[22]), IMBE_SIZE);
            valid = true;
        }
    }
    // Codec2 input 2-codec response (legacy): codec1(1) + pid(1) + ambe1(9) + codec2(1) + ambe2(9) = 21 bytes
    // Both outputs are AMBE codecs (AMBE+ and AMBE2+)
    else if ( Buffer.size() == TRANSCODER_PACKET_SIZE_C2IN )
    {
        uint8 codec1 = Buffer.data()[TRANSCODER_PACKET_CODEC_OFFSET];
        uint8 codec2 = Buffer.data()[TRANSCODER_PACKET_CODEC2_OFFSET];

        // verify codecs match what we requested
        if ( codec1 == m_uiCodecOut1 && codec2 == m_uiCodecOut2 )
        {
            // first output is 9 bytes (AMBE+ or AMBE2+)
            ::memcpy(Ambe1, &(Buffer.data()[TRANSCODER_PACKET_AMBE1_OFFSET]), AMBE_FRAME_SIZE);
            // second output is also 9 bytes (AMBE+ or AMBE2+)
            ::memcpy(Ambe2, &(Buffer.data()[TRANSCODER_PACKET_AMBE2_OFFSET]), AMBE_FRAME_SIZE);
            // no third codec
            ::memset(Ambe3, 0, IMBE_SIZE);
            valid = true;
        }
    }
    // AMBE input response: codec1(1) + pid(1) + ambe1(9) + codec2(1) + codec2(8) = 20 bytes
    // First output is AMBE, second output is Codec2
    else if ( Buffer.size() == TRANSCODER_PACKET_SIZE_MULTI )
    {
        uint8 codec1 = Buffer.data()[TRANSCODER_PACKET_CODEC_OFFSET];
        uint8 codec2 = Buffer.data()[TRANSCODER_PACKET_CODEC2_OFFSET];

        // verify codecs match what we requested
        if ( codec1 == m_uiCodecOut1 && codec2 == m_uiCodecOut2 )
        {
            // first output is 9 bytes (AMBE+ or AMBE2+)
            ::memcpy(Ambe1, &(Buffer.data()[TRANSCODER_PACKET_AMBE1_OFFSET]), AMBE_FRAME_SIZE);
            // second output is 8 bytes (Codec2)
            ::memcpy(Ambe2, &(Buffer.data()[TRANSCODER_PACKET_AMBE2_OFFSET]), CODEC2_FRAME_SIZE);
            // no third codec
            ::memset(Ambe3, 0, IMBE_SIZE);
            valid = true;
        }
    }
    // Legacy single-codec response for backward compatibility
    else if ( Buffer.size() == TRANSCODER_PACKET_SIZE_LEGACY )
    {
        uint8 codec = Buffer.data()[TRANSCODER_PACKET_CODEC_OFFSET];
        if ( codec == m_uiCodecOut1 )
        {
            ::memcpy(Ambe1, &(Buffer.data()[TRANSCODER_PACKET_AMBE1_OFFSET]), AMBE_FRAME_SIZE);
            ::memset(Ambe2, 0, CODEC2_FRAME_SIZE);
            ::memset(Ambe3, 0, IMBE_SIZE);
            valid = true;
        }
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

    // Use correct size based on input codec
    if ( m_uiCodecIn == CODEC_CODEC2 )
    {
        Buffer->Append((uint8 *)Ambe, CODEC2_FRAME_SIZE);  // 8 bytes for Codec2
    }
    else if ( m_uiCodecIn == CODEC_IMBE )
    {
        Buffer->Append((uint8 *)Ambe, IMBE_SIZE);          // 11 bytes for IMBE
    }
    else
    {
        Buffer->Append((uint8 *)Ambe, AMBE_FRAME_SIZE);    // 9 bytes for AMBE
    }

    // increment PID for next packet (wraps at 256)
    m_uiPid++;
}
