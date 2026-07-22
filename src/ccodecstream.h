//
//  ccodecstream.h
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

#ifndef ccodecstream_h
#define ccodecstream_h

#include <atomic>
#include <array>
#include <queue>
#include <chrono>
#include <unordered_map>
#include "csemaphore.h"
#include "cudpsocket.h"
#include "cpacketqueue.h"
#include "cdstarslowdata.h"

////////////////////////////////////////////////////////////////////////////////////////
// forward declarations

class CDvFramePacket;  // pointer-only use for m_pLastVoiceInJitter

////////////////////////////////////////////////////////////////////////////////////////
// define

// Jitter buffer configuration
//
// Adaptive jitter buffer — sizes itself per-stream based on observed
// ambed RTT and source-side inter-arrival jitter. The buffer's job is
// to absorb both: provide enough lead-time that ambed responses fill
// in transcoded slots before Phase 2 releases the packet to listeners,
// and absorb bursty source delivery (BM-bridged DMR can deliver
// frames in 100-500 ms bursts) so listener output stays at 50 fps.
//
// Cold-start: each new stream begins at JITTER_BUFFER_DELAY_MS
// (the safe default) until the warmup samples accumulate. After
// warmup, target is recomputed every JITTER_RECOMPUTE_FRAMES from
// the sliding-window samples.
//
// Adaptation policy (asymmetric hysteresis):
//   - Increase target immediately when conditions degrade (delta > +20 ms).
//     Network getting worse is an emergency; lookup-misses pile up
//     fast if we lag.
//   - Decrease target only after JITTER_DOWN_ADAPT_AGREE_COUNT
//     consecutive recomputes agree (delta < -20 ms each time).
//     Improving conditions can wait; oversized buffer only adds
//     latency, doesn't break anything.
//
// Floor: MIN_JITTER_DELAY_MS keeps a safety margin even on a
// near-perfect LAN — first-frame ambed RTT after a cold-start can
// spike well above the steady-state mean.
//
// Ceiling: MAX_JITTER_DELAY_MS prevents pathological network jitter
// from inflating audio latency without bound.

// Initial buffering delay used at stream open and during the warmup
// window before adaptation kicks in. Adaptation will shrink this
// when measured conditions allow. 160 ms = ~2x typical ambed RTT
// for the original deployments this code was tuned for.
#define JITTER_BUFFER_DELAY_MS                  160

// Adaptation floor — never let the buffer drop below this even on a
// pristine LAN. 80 ms is 2x typical ambed RTT (~40 ms) plus margin
// for the first frame after a silence-gap, where ambed's USB
// pipeline may not be primed.
#define MIN_JITTER_DELAY_MS                     80

// Adaptation ceiling — pathological network jitter shouldn't be
// allowed to inflate audio latency unboundedly. Above 400 ms the
// audio is so delayed that real-time conversation breaks down anyway.
#define MAX_JITTER_DELAY_MS                     400

// Sliding window size for both RTT and source-arrival samples. 100
// samples = ~2 seconds at 50 fps, long enough for stable percentile
// estimates without lagging too far behind real network conditions.
#define JITTER_SAMPLE_WINDOW                    100

// Recompute the target every N frames. 50 = once per second. Smaller
// = more responsive but more CPU. 50 was chosen to match the warmup
// threshold so the first recompute happens at exactly the moment we
// have enough samples to be statistically meaningful.
#define JITTER_RECOMPUTE_FRAMES                 50

// Hysteresis threshold for applying a new target (in ms). Smaller =
// more sensitive but risks oscillation around marginal thresholds.
#define JITTER_RECOMPUTE_HYSTERESIS_MS          20

// Asymmetric down-adapt: require this many consecutive recomputes to
// all want a smaller target before applying one. Damps oscillation
// when network is borderline. 3 recomputes = 3 seconds of consistent
// improvement before we shrink.
#define JITTER_DOWN_ADAPT_AGREE_COUNT           3

// Safety margin (in ms) added to the RTT P95 when computing the new
// target. Covers measurement quantisation and short-term variability
// not captured by the percentile.
#define JITTER_RTT_SAFETY_MARGIN_MS             30

// Overrun protection — if buffer occupancy exceeds the threshold,
// drop the oldest packet to drain back toward target. Listener hears
// one 20 ms gap; cadence recovers.
//
// Threshold is the MAX of two computations:
//
//   ratio    = target_pkts × (NUM/DEN) = target_pkts × 2.0
//   additive = target_pkts + JITTER_OVERRUN_BURST_HEADROOM
//
// The two paths target different operating regions and the larger
// of the two wins:
//
//   - At small targets (e.g. 160 ms = 8 pkts cold-start, where the
//     adaptive buffer spends most time on healthy LANs), 2.0x = 16
//     and additive = 8+12 = 20. Additive wins — gives enough burst
//     headroom to absorb a one-shot WAN catch-up spike of 9-10
//     frames without dropping. (Production observed peak=17 at
//     target=8 with arrival_max=159 ms; 12 headroom gives 3-packet
//     margin against larger gaps that the adaptive recompute hasn't
//     seen yet.) Crossover where ratio meets additive: target_pkts
//     = JITTER_OVERRUN_BURST_HEADROOM, i.e. target = 240 ms.
//
//   - At large targets (e.g. 400 ms ceiling on noisy WAN), 2.0x =
//     40 and additive = 20+12 = 32. Ratio wins — the headroom is
//     already absorbed into the larger target itself, so the
//     ratio's "scale-with-target" property dominates. Peak buffered
//     audio at MAX target is 40 × 20 ms = 800 ms. Above that audio
//     latency breaks real-time conversation regardless.
//
// Why 2.0x not 1.5x for the ratio: typical multi-frame source bursts
// (NXDN 4-per-UDP, YSF 5-per-UDP) tipped a near-target buffer over a
// 1.5x threshold regularly under normal traffic, dropping audio that
// didn't need to be dropped. 2.0x is the upper bound of what's still
// useable for real-time conversation at the MAX target.
//
// Why not higher (e.g. 2.5x): peak buffered audio at MAX target is
// target × ratio. 2.0x → 800 ms peak (tolerable for non-realtime
// comms), 2.5x → 1000 ms (breaks conversational feel), 3.0x → 1200
// ms (unusable). If you raise the ratio, also lower
// MAX_JITTER_DELAY_MS or accept poor real-time conversation
// behaviour on noisy WAN paths.
#define JITTER_OVERRUN_RATIO_NUM                2
#define JITTER_OVERRUN_RATIO_DEN                1

// Additive burst headroom (in packets) above target. Sized to absorb
// a single WAN-side catch-up burst when source delivery has paused.
// 12 packets = 240 ms of buffered audio on top of target. Production
// captured peak=17 at target=8 with arrival_max=159 ms (excess of 9
// over target); 12 leaves 3 packets of margin against larger gaps
// (e.g. a 200 ms WAN pause delivering 10 catch-up frames). Smaller
// values (8-10) work for the observed case but leave a thin margin
// for the next-larger burst event.
#define JITTER_OVERRUN_BURST_HEADROOM           12

// Cap on overrun-drops per Phase 2 invocation. Higher than the
// release cap (3) because dropping is cheaper than encoding+sending,
// so we can drain harder than we release.
//
// Intentionally smaller than worst-case excess (e.g. a one-shot
// 25-packet burst at MAX-target threshold of 40 packets won't be
// fully drained in one Phase 2 tick). Excess bleeds across multiple
// ticks at this rate. Don't raise this thinking it'll "fix" overrun
// faster — the slow-bleed behaviour limits cadence-stutter density
// for the listener.
#define JITTER_OVERRUN_DROP_CAP                 8

// Maximum inter-arrival sample we treat as jitter (microseconds).
// Anything above this is a stall (silence gap, network outage,
// source PTT release) rather than jitter — admitting such a sample
// would pin the buffer at MAX for the rest of the window. Suppress
// instead. 500 ms is well past any reasonable per-frame variability
// and well short of typical conversational pauses.
#define JITTER_ARRIVAL_MAX_SAMPLE_US            500000

// JITTER_BUFFER_FRAME_MS: Interval between releasing packets (voice frame duration).
// Constant — listeners expect 50 fps output cadence regardless of source.
#define JITTER_BUFFER_FRAME_MS                  20

// frame sizes
#define AMBE_SIZE           9
#define AMBEPLUS_SIZE       9
#define IMBE_SIZE           11

// transcoder packet offsets
#define TRANSCODER_PACKET_CODEC_OFFSET      0
#define TRANSCODER_PACKET_PID_OFFSET        1
#define TRANSCODER_PACKET_AMBE1_OFFSET      2
#define TRANSCODER_PACKET_CODEC2_OFFSET     11
#define TRANSCODER_PACKET_AMBE2_OFFSET      12
#define TRANSCODER_PACKET_CODEC3_OFFSET     20
#define TRANSCODER_PACKET_AMBE3_OFFSET      21

// transcoder packet sizes
#define TRANSCODER_PACKET_SIZE_LEGACY       11      // codec(1) + pid(1) + ambe(9)
#define TRANSCODER_PACKET_SIZE_MULTI        20      // codec1(1) + pid(1) + ambe1(9) + codec2(1) + codec2_data(8)
#define TRANSCODER_PACKET_SIZE_C2IN         21      // codec1(1) + pid(1) + ambe1(9) + codec2(1) + ambe2(9)
#define TRANSCODER_PACKET_SIZE_IMBEIN       30      // codec1(1) + pid(1) + ambe1(9) + codec2(1) + ambe2(9) + codec3(1) + codec2_data(8)
#define TRANSCODER_PACKET_SIZE_3CODEC       32      // codec1(1) + pid(1) + ambe1(9) + codec2(1) + codec2_data(8) + codec3(1) + imbe(11)
#define TRANSCODER_PACKET_SIZE_C2IN_3CODEC  33      // codec1(1) + pid(1) + ambe1(9) + codec2(1) + ambe2(9) + codec3(1) + imbe(11)


////////////////////////////////////////////////////////////////////////////////////////
// class

class CPacketStream;

class CCodecStream : public CPacketQueue
{
public:
    // constructor
    CCodecStream(CPacketStream *, uint16, uint8, uint8, uint8, uint8);

    // destructor
    virtual ~CCodecStream();
    
    // initialization
    bool Init(uint16);
    void Close(void);
    
    // get
    bool   IsConnected(void) const          { return m_bConnected; }
    uint16 GetStreamId(void) const          { return m_uiStreamId; }
    double GetPingMin(void) const           { return m_fPingMin; }
    double GetPingMax(void) const           { return m_fPingMax; }
    double GetPingAve(void) const           { return (m_fPingCount != 0) ? m_fPingSum/m_fPingCount : 0; }
    uint32 GetTotalPackets(void) const      { return m_uiTotalPackets; }
    uint32 GetTimeoutPackets(void) const    { return m_uiTimeoutPackets; }
    uint32 GetReturnedPackets(void) const   { return m_uiReturnedPackets; }
    uint32 GetResponseLookupMisses(void) const { return m_uiResponseLookupMisses; }
    uint32 GetUnfilledReleases(void) const     { return m_uiUnfilledReleases; }
    // Adaptive jitter buffer stats. Min/avg/max of the active target
    // jitter delay over the stream's lifetime; an unchanged value
    // across the stream means conditions never deviated from the
    // initial estimate. Overrun drop count = packets evicted from the
    // jitter buffer's head because occupancy exceeded the hybrid
    // threshold max(2.0x target, target + JITTER_OVERRUN_BURST_HEADROOM).
    unsigned int GetJitterTargetMin(void) const { return m_TargetSamples > 0 ? m_TargetMin : m_CurrentJitterDelayMs; }
    unsigned int GetJitterTargetMax(void) const { return m_TargetSamples > 0 ? m_TargetMax : m_CurrentJitterDelayMs; }
    unsigned int GetJitterTargetAvg(void) const { return m_TargetSamples > 0 ? (unsigned int)(m_TargetSum / m_TargetSamples) : m_CurrentJitterDelayMs; }
    uint32       GetOverrunDrops(void) const   { return m_uiOverrunDrops; }
    // Diagnostic instrumentation. Peak jitter-buffer occupancy seen during
    // the stream (max snapshot at every Phase 3 push). Source-arrival max
    // in ms (live-tracked from the same samples that feed the adaptive
    // jitter recompute). First/last frame index at which an overrun drop
    // fired — the index is m_uiTotalPackets at drop time, i.e. "the Nth
    // sent packet was the last frame before the drop." Both frame fields
    // are zero if no drops occurred this stream.
    uint32       GetPeakBufferOccupancy(void) const { return m_uiPeakBufferOccupancy; }
    uint32       GetArrivalMaxMs(void) const        { return (m_uiArrivalMaxUs + 999) / 1000; }
    uint32       GetFirstDropFrame(void) const      { return m_uiFirstDropFrame; }
    uint32       GetLastDropFrame(void) const       { return m_uiLastDropFrame; }
    bool   IsEmpty(void) const;
    size_t GetDepth(void) const;

    // task
    static void Thread(CCodecStream *);
    void Task(void);
    

protected:
    // packet decoding helpers
    bool IsValidAmbePacket(const CBuffer &, uint8 *, uint8 *, uint8 *);

    // packet encoding helpers
    void EncodeAmbePacket(CBuffer *, const uint8 *);


protected:
    // data
    uint16          m_uiStreamId;
    uint16          m_uiPort;
    uint8           m_uiPid;
    uint8           m_uiCodecIn;
    uint8           m_uiCodecOut1;
    uint8           m_uiCodecOut2;
    uint8           m_uiCodecOut3;

    // socket
    CIp             m_Ip;
    CUdpSocket      m_Socket;
    std::atomic<bool> m_bConnected;
    
    // associated packet stream
    CPacketStream   *m_PacketStream;

    // thread
    std::atomic<bool> m_bStopThread;
    std::thread     *m_pThread;
    CTimePoint      m_TimeoutTimer;
    CTimePoint      m_StatsTimer;
    
    // statistics
    double          m_fPingMin;
    double          m_fPingMax;
    double          m_fPingSum;
    double          m_fPingCount;
    uint32          m_uiTotalPackets;
    uint32          m_uiTimeoutPackets;
    uint32          m_uiReturnedPackets;

    // Health metrics (reported in the ambed-stats log line at stream close).
    // m_uiResponseLookupMisses counts ambed responses that arrived for a
    // packet already released from the jitter buffer (network loss + slow
    // ambed combined, or ambed RTT > JITTER_BUFFER_DELAY_MS). High values
    // indicate ambed is unhealthy or the LAN is dropping return packets.
    // m_uiUnfilledReleases counts jitter pops where ambed never responded
    // (transcoded slots stayed zero). High values indicate outbound xlxd
    // → ambed loss or ambed unable to keep up; transcoded listeners hear
    // silence on those frames, but D-Star pass-through audio is intact.
    //
    // Atomic: written from Task() (the codec thread) and read from the
    // ctranscoder.cpp close-stats path (which runs on whichever protocol
    // thread invoked CloseStream). Read is unlocked; using std::atomic
    // ensures a torn read can't happen on the rare worker-architecture
    // where uint32 isn't naturally atomic.
    std::atomic<uint32> m_uiResponseLookupMisses;
    std::atomic<uint32> m_uiUnfilledReleases;

    // Jitter buffer - smooths out variable ambed latency.
    //
    // Packets enter immediately on Push (NOT after ambed responds — see
    // the architectural note at the top of CCodecStream::Task). Each
    // packet starts with only its source-codec slot filled (e.g. AMBE+
    // for a D-Star source). ambed responses fill in the other codec
    // slots in-place via lookup through m_PendingTranscode while the
    // packet is still in the jitter buffer. When the release timer
    // fires (every JITTER_BUFFER_FRAME_MS = 20 ms), the front packet
    // is released to the packet stream — whatever transcoded slots
    // happened to be filled by then are filled, the rest are zero.
    //
    // Net effect: ambed is enrichment, not a gate. D-Star pass-through
    // audio reaches D-Star listeners on the regular cadence regardless
    // of UDP loss to/from ambed or ambed being totally offline. Other-
    // mode listeners hear silence on frames whose transcoded slots
    // didn't get filled in time.
    std::queue<CPacket *>  m_JitterBuffer;
    std::chrono::steady_clock::time_point m_NextReleaseTime;
    bool            m_bJitterBufferStarted;

    // Latches true at the first successful Phase 2 release. Gate for
    // the overrun-drop logic: while this is false the buffer is
    // accepting the initial fill (which legitimately includes the
    // pre-codec ring buffer replay following the AMBEd handshake —
    // up to 100 frames pushed in microseconds), and we don't want
    // to drop any of that. Once Phase 2 has released anything we're
    // in normal operation and overrun protection becomes meaningful.
    bool            m_bFirstReleaseDone;

    // Lookup index from the uint8 PID echoed in ambed responses to the
    // CDvFramePacket currently sitting in m_JitterBuffer. Non-owning —
    // entries always alias a packet whose lifetime is bounded by the
    // jitter buffer (entered at Push, removed at jitter-pop or close-
    // purge). Both reads and writes happen under CCodecStream::Lock(),
    // same as m_JitterBuffer itself.
    //
    // PID is the m_uiPid value at send-to-ambed time (CCodecStream's
    // monotonic uint8 counter, wraps at 256). Map size is bounded by
    // the in-flight jitter-buffer occupancy (~JITTER_BUFFER_DELAY_MS /
    // JITTER_BUFFER_FRAME_MS = 8 frames at steady state, max ~50 in a
    // pathological stall) which is far below the 256-PID space — so
    // wraparound never causes key collision in the live working set.
    //
    // Side benefit over the previous FIFO-pop design: out-of-order or
    // partially-lost ambed responses are matched to the correct packet
    // by PID. The previous m_LocalQueue.pop_front() approach silently
    // mis-paired transcoded slots when any single response was dropped.
    std::unordered_map<uint8, CDvFramePacket *> m_PendingTranscode;

    // Adaptive jitter buffer state — see the JITTER_* constants block
    // at the top of this file for the algorithm rationale.
    //
    // Both sample rings are circular buffers (m_*SampleIdx is the
    // next-write position; m_*SampleCount tracks how many have been
    // written so far, capped at JITTER_SAMPLE_WINDOW). All access is
    // under CCodecStream::Lock() — the same critical section that
    // already protects m_JitterBuffer and m_PendingTranscode.
    //
    // Units: microseconds for sample storage (uint32 fits ~71 minutes),
    // milliseconds for the active target. The unit-in-the-name
    // convention prevents the "is this μs or ms?" debugging pitfall.
    std::array<uint32, JITTER_SAMPLE_WINDOW>   m_RttSamplesUs;
    unsigned int                               m_RttSampleIdx;
    unsigned int                               m_RttSampleCount;

    std::array<uint32, JITTER_SAMPLE_WINDOW>   m_ArrivalSamplesUs;
    unsigned int                               m_ArrivalSampleIdx;
    unsigned int                               m_ArrivalSampleCount;

    // Inter-arrival timing. m_LastPushTime is the wall-clock of the
    // previous Phase 3 push; the delta to "now" is one inter-arrival
    // sample. m_bHasLastPushTime guards against (a) the very first
    // push of the stream where there is no previous time, and (b) the
    // first push after a silence-gap (where the delta would be the
    // gap duration, which is signal noise rather than jitter — we
    // suppress sampling for that frame).
    std::chrono::steady_clock::time_point      m_LastPushTime;
    bool                                       m_bHasLastPushTime;

    // Currently-active jitter delay. Initialised to JITTER_BUFFER_DELAY_MS
    // (the safe default) at construction; the cold-start window before
    // adaptation kicks in keeps it there while RTT samples accumulate.
    // After warmup, RecomputeJitterTarget() updates it every
    // JITTER_RECOMPUTE_FRAMES frames using the asymmetric hysteresis
    // policy described above the JITTER_* constants block.
    unsigned int                               m_CurrentJitterDelayMs;

    // Frame counter for the recompute trigger. Incremented per Phase 3
    // push; recompute fires when this reaches JITTER_RECOMPUTE_FRAMES
    // AND we have at least JITTER_SAMPLE_WINDOW/2 RTT samples.
    unsigned int                               m_FramesSinceRecompute;

    // Counts consecutive recomputes that all wanted a smaller target.
    // Reset to 0 on any recompute that wants larger or within-
    // hysteresis. Down-adapt is applied only when this reaches
    // JITTER_DOWN_ADAPT_AGREE_COUNT (slow-down asymmetry).
    unsigned int                               m_AgreeingDownAdapts;

    // Stats tracking for the close-time log line. min/avg/max of the
    // active jitter delay seen during this stream's lifetime, plus a
    // count of overrun-drops (Phase 2 dropped a packet because buffer
    // exceeded the hybrid threshold — see JITTER_OVERRUN_RATIO_NUM
    // and JITTER_OVERRUN_BURST_HEADROOM in the constants block).
    unsigned int                               m_TargetMin;
    unsigned int                               m_TargetMax;
    uint64_t                                   m_TargetSum;
    unsigned int                               m_TargetSamples;
    uint32                                     m_uiOverrunDrops;

    // Diagnostic instrumentation for the close-time stats line. All four
    // are touched only under CCodecStream::Lock() — same critical section
    // as the existing jitter state — so no separate synchronisation
    // needed. See the public getters' header comment for semantics.
    uint32                                     m_uiPeakBufferOccupancy;
    uint32                                     m_uiArrivalMaxUs;
    uint32                                     m_uiFirstDropFrame;
    uint32                                     m_uiLastDropFrame;

    // In-flight counter: packets popped from jitter buffer but not yet
    // pushed to the packet stream. Prevents IsEmpty() from returning true
    // during the Phase 2 gap between jitter-pop and stream-push.
    std::atomic<int> m_iInFlightPackets;

    // D-Star slow-data synthesiser. Only used on AMBE+ egress (i.e. when
    // transcoding from a non-D-Star source to D-Star). For every AMBE+
    // output frame, the Task() loop stamps either the sync marker (frame
    // 0 of the 21-frame cycle) or 3 bytes of scrambled slow-data (frames
    // 1–20) into the outgoing DvData field. Initialised lazily on the
    // first transcoded frame via InitSlowData().
    CDStarSlowData  m_SlowData;
    bool            m_bSlowDataInit;

    // Cycle index for slow-data scheduling. Cycle 0 carries the text
    // message (BeginTextCycle); cycles 1+ carry header-sync continuously
    // (BeginHeaderCycle), matching the native D-Star radio pattern
    // confirmed by multi-cycle pcap of a Pi-Star source. Incremented on
    // each sync-frame boundary in Task(); reset to 0 by InitSlowData()
    // at the start of each stream.
    uint32          m_uiSlowDataCycle;

    // Pre-EoT slow-data marker support (all egress paths).
    //
    // Native D-Star radios emit a specific slow-data pattern in the
    // slow-data slot of the last voice frame immediately before the EoT
    // frame: wire bytes 0x55 0x55 0x55 (plaintext 0x25 0x1A 0xC6, the
    // mirror of the EoT frame's own slow-data pattern). Strict Icom
    // decoders (RP2C via g2_link) appear to use this as the "stream is
    // about to close cleanly" signal; without it, subsequent TXs can be
    // rejected/muted until the RP2C's activity timeout fires.
    //
    // Tracking and override now run for ALL streams that pass through
    // CCodecStream — both cross-mode AMBE+ output (where slow-data is
    // synthesised) and AMBE+ pass-through paths (native D-Star, DCS
    // source, XLX-interlinked AMBE+ traffic such as BM-to-XLX DMR
    // bridges that don't emit the marker themselves). For non-D-Star
    // egress the DvData mutation is harmless because those encoders
    // don't read DvData. See ccodecstream.cpp:512-578 for the full
    // rationale.
    //
    // We don't know which voice frame is the last one until the EoT
    // arrives. So we track a non-owning pointer to the most recently
    // pushed voice frame while it is still sitting in m_JitterBuffer
    // (it sits there for at least JITTER_BUFFER_DELAY_MS = 160ms before
    // first release). When an EoT arrives in Task() Phase 1, if the
    // pointer is non-NULL we overwrite that parked packet's slow-data
    // in-place with the pre-EoT marker before releasing it. The pointer
    // is cleared when the packet is popped from m_JitterBuffer (Phase 2)
    // or when the EoT override has already consumed it.
    //
    // Access is serialised by m_Mutex (CCodecStream::Lock()): all three
    // touch points (Phase 1 push-to-jitter, Phase 1 EoT override,
    // Phase 2 pop-from-jitter) hold the lock for the entire operation.
    // The pointer is strictly a tracking aid — it never owns the packet
    // and is always cleared before the packet is destroyed.
    CDvFramePacket *m_pLastVoiceInJitter;

    // Build the 20-char text + 9-element header-sync from the packet
    // stream's cached header and g_Reflector state, push them into
    // m_SlowData via SetText() and SetHeaderSync(). Safe to call
    // repeatedly — re-seeds both buffers each time.
    void InitSlowData(void);

    // Adaptive jitter buffer recompute. Called from Phase 3 every
    // JITTER_RECOMPUTE_FRAMES frames once the warmup window has at
    // least JITTER_SAMPLE_WINDOW/2 RTT samples. Reads m_RttSamplesUs
    // and m_ArrivalSamplesUs, computes the new target, applies it
    // via the asymmetric hysteresis policy. Caller must hold
    // CCodecStream::Lock().
    void RecomputeJitterTarget(void);
};


////////////////////////////////////////////////////////////////////////////////////////
#endif /* ccodecstream_h */
