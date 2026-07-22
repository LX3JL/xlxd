//
//  cvocodecchannel.h
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


#ifndef cvocodecchannel_h
#define cvocodecchannel_h

#include <mutex>
#include <queue>
#include <vector>
#include <thread>
#include <chrono>
#include <condition_variable>
#include <atomic>
#include "cpacketqueue.h"
#include "csignalprocessor.h"
#include "cvoicepacket.h"
#include "codec2/codec2.h"
#include "imbe/imbe_vocoder.h"

////////////////////////////////////////////////////////////////////////////////////////
// PID FIFO desync-recovery constants
//
// Max age of a FIFO entry before PopPid prunes it as orphaned.
// Healthy ambed RTT is 40-100 ms (max observed ~144 ms in production
// captures); 500 ms is well above that and well below conversational
// silences. After this many ms, the entry is assumed to correspond
// to an input packet whose hardware response was dropped, and is
// pruned to recover FIFO alignment.
#define PID_FIFO_TIMEOUT_MS         500

// Max FIFO occupancy before PushPid drops oldest. Bounds memory if
// xlxd ever sustains input above DVStick throughput. 200 = 4 seconds
// of frames at 50 fps; well above any realistic burst size (xlxd's
// pre-codec buffer is paced at ~50 fps after replay, and YSF/NXDN
// per-UDP burst sizes are 4-5 frames at most).
#define PID_FIFO_MAX_DEPTH          200

////////////////////////////////////////////////////////////////////////////////////////
// class

class CVocodecInterface;

class CVocodecChannel
{
public:
    // constructors
    CVocodecChannel(CVocodecInterface *, int, CVocodecInterface *, int, int);

    // destructor
    virtual ~CVocodecChannel();
    
    // open & close
    bool Open(void);
    bool IsOpen(void) const                 { return m_bOpen; }
    void Close(void);
    
    // get
    uint8 GetCodecIn(void) const;
    uint8 GetCodecOut(void) const;
    int   GetChannelIn(void) const          { return m_iChannelIn; }
    int   GetChannelOut(void) const         { return m_iChannelOut; }
    int   GetSpeechGain(void) const         { return m_iSpeechGain; }
    
    //Processing
    void ProcessSignal(CVoicePacket& voicePacket);
    
    // interfaces
    bool IsInterfaceIn(const CVocodecInterface *interface)      { return (interface == m_InterfaceIn); }
    bool IsInterfaceOut(const CVocodecInterface *interface)     { return (interface == m_InterfaceOut); }
    
    // queues
    CPacketQueue *GetPacketQueueIn(void)    { m_QueuePacketIn.Lock(); return &m_QueuePacketIn; }
    void ReleasePacketQueueIn(void)         { m_QueuePacketIn.Unlock(); }
    CPacketQueue *GetPacketQueueOut(void)   { m_QueuePacketOut.Lock(); return &m_QueuePacketOut; }
    void ReleasePacketQueueOut(void)        { m_QueuePacketOut.Unlock(); }
    CPacketQueue *GetVoiceQueue(void)       { m_QueueVoice.Lock(); return &m_QueueVoice; }
    void ReleaseVoiceQueue(void)            { m_QueueVoice.Unlock(); }

    // PID preservation through the DVStick pipeline.
    //
    // The hardware response parser (CUsb3xxx*Interface::IsValidChannelPacket)
    // creates a fresh CAmbePacket from raw USB bytes and has no way to
    // recover the PID that was on the corresponding input packet — the
    // PID is lost the moment the input packet is consumed by the USB
    // write path. Without preservation, every output packet has PID=0
    // and xlxd's PID-keyed response routing breaks (every response after
    // the first looks like a stale duplicate).
    //
    // Approach: each CVocodecChannel holds a FIFO of (pid, pushedAt)
    // entries. The USB interface pushes when consuming an input packet
    // (decoder side for AMBE-input mode, encoder side for Codec2/IMBE-
    // input modes) and pops when producing an output packet. The
    // DVStick processes packets in FIFO order per physical channel, so
    // pop order matches push order — pid round-trips correctly via
    // FIFO pairing rather than via byte-in-the-packet preservation
    // that the hardware protocol doesn't support.
    //
    // The pushedAt timestamp is used for desync recovery. If the
    // hardware ever drops a packet (USB glitch, transient fault), the
    // FIFO would be permanently off by one without intervention.
    // Two protections:
    //
    //   - PopPid prunes orphaned entries (front older than
    //     PID_FIFO_TIMEOUT_MS) before returning. This bounds the
    //     mis-pairing window after a hardware drop to ~timeout
    //     duration; without it, the desync would be permanent for
    //     the rest of the stream. Mid-window mis-pairing is still
    //     possible (a response arriving during the timeout window
    //     pops the stale front entry); see Q1 trace in the design
    //     review for why a tighter timeout doesn't help. 500 ms is
    //     well above healthy ambed RTT (40-100 ms) and well below
    //     conversational silences.
    //
    //   - PushPid caps FIFO occupancy (drops oldest if size >
    //     PID_FIFO_MAX_DEPTH). Memory-safety belt; only fires under
    //     pathological sustained input overrun.
    //
    // Drops are tracked separately for diagnostics — orphan drops
    // (timeout-pruned) and overflow drops (cap-pruned) have different
    // operational meanings.
    //
    // PopPid returns 0 if the FIFO is empty (matches the
    // pre-fix default-PID behaviour and guarantees PopPid never
    // crashes on an unexpected hardware response).
    void   PushPid(uint8 pid);
    uint8  PopPid(void);

    // Diagnostic getters for the close-time stats line. Use .load()
    // because the counters are atomic — see the m_PidFifo* declaration
    // block below for the rationale.
    uint32 GetPidFifoOrphanDrops(void) const   { return m_PidFifoOrphanDrops.load(); }
    uint32 GetPidFifoOverflowDrops(void) const { return m_PidFifoOverflowDrops.load(); }

    // Codec2 queue (for software-encoded Codec2 data)
    void EnableCodec2(bool bEnable);
    bool HasCodec2Data(void);
    bool GetCodec2Data(uint8 *codec2);
    bool IsCodec2Enabled(void) const        { return m_bCodec2Enabled; }

    // IMBE queue (for software-encoded IMBE data)
    void EnableImbe(bool bEnable);
    bool HasImbeData(void);
    bool GetImbeData(uint8 *imbe);
    bool IsImbeEnabled(void) const          { return m_bImbeEnabled; }

    // operators
    //virtual bool operator ==(const CVocodecChannel &) const   { return false; }
    
protected:
    // queues helpers
    void PurgeAllQueues(void);
    
protected:
    // status
    std::atomic<bool>   m_bOpen;

    // connected interfaces
    CVocodecInterface   *m_InterfaceIn;
    int                 m_iChannelIn;
    CVocodecInterface   *m_InterfaceOut;
    int                 m_iChannelOut;
    
    // ambe queues
    CPacketQueue        m_QueuePacketIn;
    CPacketQueue        m_QueuePacketOut;
    // voice queue
    CPacketQueue        m_QueueVoice;

    // PID preservation FIFO — see PushPid/PopPid comment above the
    // public methods. Touched by the USB interface Task() thread
    // (push on input consume, pop on output produce) and cleared by
    // PurgeAllQueues from Open/Close paths on potentially other
    // threads, so a dedicated mutex guards it.
    //
    // Each entry carries a pushedAt timestamp (steady_clock so an NTP
    // wall-clock step doesn't prune the entire FIFO) for the
    // desync-recovery logic in PushPid/PopPid.
    struct PidEntry
    {
        uint8                                  pid;
        std::chrono::steady_clock::time_point  pushedAt;
    };
    std::queue<PidEntry>   m_PidFifo;
    std::mutex             m_PidFifoMutex;

    // Desync drop counters. orphan = entries pruned at PopPid because
    // they exceeded PID_FIFO_TIMEOUT_MS (the response that should have
    // popped them never arrived — hardware drop). overflow = entries
    // pruned at PushPid because the FIFO exceeded PID_FIFO_MAX_DEPTH
    // (sustained input overrun, memory-safety belt). Reset by
    // PurgeAllQueues so each stream gets a fresh accounting epoch.
    //
    // Atomic because the close-time capture path in CStream::Close
    // reads these from a different thread than the USB Task() thread
    // that increments them. Increments still happen under
    // m_PidFifoMutex (the increment site is bracketed by the mutex
    // we already hold for the FIFO operations), so the atomic
    // operations don't add new synchronisation cost — they just
    // make the cross-thread read well-defined under the C++ memory
    // model.
    std::atomic<uint32>    m_PidFifoOrphanDrops;
    std::atomic<uint32>    m_PidFifoOverflowDrops;
    
    // settings
    int                 m_iSpeechGain;

    // Codec2 encoding (software) - runs in separate thread to avoid blocking hardware
    std::atomic<bool>   m_bCodec2Enabled;
    CCodec2             *m_pCodec2;
    std::mutex          m_Codec2Mutex;
    std::queue<std::vector<uint8>> m_Codec2Queue;

    // Async Codec2 encoding thread
    std::thread         *m_pCodec2Thread;
    std::atomic<bool>   m_bCodec2StopThread;
    std::mutex          m_Codec2PcmMutex;
    std::condition_variable m_Codec2PcmCondition;
    std::queue<std::vector<int16_t>> m_Codec2PcmQueue;  // PCM samples waiting to be encoded
    static void Codec2EncoderThread(CVocodecChannel *pChannel);
    void Codec2EncoderTask(void);

    // IMBE encoding (software) - runs in separate thread to avoid blocking hardware
    std::atomic<bool>   m_bImbeEnabled;
    imbe_vocoder        *m_pImbe;
    std::mutex          m_ImbeMutex;
    std::queue<std::vector<uint8>> m_ImbeQueue;

    // Async IMBE encoding thread
    std::thread         *m_pImbeThread;
    std::atomic<bool>   m_bImbeStopThread;
    std::mutex          m_ImbePcmMutex;
    std::condition_variable m_ImbePcmCondition;
    std::queue<std::vector<int16_t>> m_ImbePcmQueue;  // PCM samples waiting to be encoded
    static void ImbeEncoderThread(CVocodecChannel *pChannel);
    void ImbeEncoderTask(void);

private:
    CSignalProcessor* m_signalProcessor;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cvocodecchannel_h */
