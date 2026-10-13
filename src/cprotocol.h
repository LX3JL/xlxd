//
//  cprotocol.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 01/11/2015.
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

#ifndef cprotocol_h
#define cprotocol_h

#include <atomic>
#include <mutex>
#include <condition_variable>
#include "cudpsocket.h"
#include "cpacketstream.h"
#include "cdvheaderpacket.h"
#include "cdvframepacket.h"
#include "cdvlastframepacket.h"

////////////////////////////////////////////////////////////////////////////////////////

// DMR defines
// slot n'
#define DMR_SLOT1                       1
#define DMR_SLOT2                       2
// call type
#define DMR_GROUP_CALL                  0
#define DMR_PRIVATE_CALL                1
// frame type
#define DMR_FRAMETYPE_VOICE             0
#define DMR_FRAMETYPE_VOICESYNC         1
#define DMR_FRAMETYPE_DATA              2
#define DMR_FRAMETYPE_DATASYNC          3
// data type
#define DMR_DT_VOICE_PI_HEADER          0
#define DMR_DT_VOICE_LC_HEADER          1
#define DMR_DT_TERMINATOR_WITH_LC       2
#define DMR_DT_CSBK                     3
#define DMR_DT_DATA_HEADER              6
#define DMR_DT_RATE_12_DATA             7
#define DMR_DT_RATE_34_DATA             8
#define DMR_DT_IDLE                     9
#define DMR_DT_RATE_1_DATA              10
// CRC masks
#define DMR_VOICE_LC_HEADER_CRC_MASK    0x96
#define DMR_TERMINATOR_WITH_LC_CRC_MASK 0x99
#define DMR_PI_HEADER_CRC_MASK          0x69
#define DMR_DATA_HEADER_CRC_MASK        0xCC
#define DMR_CSBK_CRC_MASK               0xA5


////////////////////////////////////////////////////////////////////////////////////////
// class

class CProtocol
{
public:
    // constructor
    CProtocol();
    
    // destructor
    virtual ~CProtocol();
    
    // initialization
    virtual bool Init(void);
    virtual void Close(void);
    
    // queue
    CPacketQueue *GetQueue(void)        { m_Queue.Lock(); return &m_Queue; }
    void ReleaseQueue(void)             { m_Queue.Unlock(); m_QueueCondVar.notify_one(); }

    // get
    const CCallsign &GetReflectorCallsign(void)const { return m_ReflectorCallsign; }

    // task — legacy single-thread mode (used by unconverted protocols)
    static void RxThread(CProtocol *);
    static void TxThread(CProtocol *);
    virtual void Task(void) {};

    // split-thread mode — override these plus UsesSplitThreads() to enable
    virtual bool UsesSplitThreads(void) const { return false; }
    virtual void RxTask(void) {}
    virtual void TxTask(void);
    
protected:
    // packet encoding helpers
    virtual bool EncodeDvPacket(const CPacket &, CBuffer *) const;
    virtual bool EncodeDvHeaderPacket(const CDvHeaderPacket &, CBuffer *) const         { return false; }
    virtual bool EncodeDvFramePacket(const CDvFramePacket &, CBuffer *) const           { return false; }
    virtual bool EncodeDvLastFramePacket(const CDvLastFramePacket &, CBuffer *) const   { return false; }
    
    // stream helpers
    virtual bool OnDvHeaderPacketIn(CDvHeaderPacket *, const CIp &) { return false; }
    virtual void OnDvFramePacketIn(CDvFramePacket *, const CIp * = NULL);
    virtual void OnDvLastFramePacketIn(CDvLastFramePacket *, const CIp * = NULL);
    void HandleStreamLastFrame(CPacketStream *, CDvLastFramePacket *);

    // stream handle helpers
    CPacketStream *GetStream(uint16, const CIp * = NULL);
    void CheckStreamsTimeout(void);

    // shared per-transmission stream-id nonce generator
    //
    // YSF / NXDN / P25 cross-mode allocate fresh stream-ids on each new
    // transmission (see e.g. cysfprotocol.cpp::AllocateNewStreamIdForSource).
    // The nonce source is shared across ALL protocol instances so that:
    //   1. Two different protocols can't allocate the same value for two
    //      simultaneously-active transmissions on the same module — that
    //      would put the same wire DCS sid on two consecutive cross-mode
    //      streams from different sources, ghosting the gateway's stream
    //      tracking the same way same-source rapid re-keys did.
    //   2. The starting offset is randomised per xlxd run, so a gateway
    //      that cached the previous run's last-used sid won't match the
    //      first sid the new run hands out.
    // Returns a non-zero uint16 each call. Atomic — safe to call from
    // any thread without external locking.
    static uint16 AllocateGlobalStreamIdNonce(void);
    
    // queue helper
    virtual void HandleQueue(void);
    
    // keepalive helpers
    virtual void HandleKeepalives(void) {}

    // syntax helper
    bool IsNumber(char) const;
    bool IsLetter(char) const;
    bool IsSpace(char) const;
    
    // dmr DstId to Module helper
    virtual char DmrDstIdToModule(uint32) const;
    virtual uint32 ModuleToDmrDestId(char) const;

protected:
    // socket
    CUdpSocket      m_Socket;
    
    // streams
    std::vector<CPacketStream *> m_Streams;
    
    // queue
    CPacketQueue    m_Queue;
    
    // threads
    std::atomic<bool> m_bStopThread;     // RX thread stop (also used by legacy single-thread)
    std::atomic<bool> m_bStopTxThread;   // TX thread stop
    std::thread     *m_pThread;          // RX thread (or legacy single thread)
    std::thread     *m_pTxThread;        // TX thread (NULL in legacy mode)

    // TX thread wake-up
    std::condition_variable m_QueueCondVar;
    std::mutex              m_QueueCondMutex;
    
    // identity
    CCallsign       m_ReflectorCallsign;
    
    // debug
    CTimePoint      m_DebugTimer;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cprotocol_h */
