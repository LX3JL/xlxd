//
//  ctranscoder.h
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

#ifndef ctranscoder_h
#define ctranscoder_h

#include <atomic>
#include "csemaphore.h"
#include "ccodecstream.h"
#include "cudpsocket.h"

////////////////////////////////////////////////////////////////////////////////////////
// define

// extended open (PCM) capability of the connected transcoder
#define PCMCAPS_IDLE                0   // not probed on this connection
#define PCMCAPS_PROBING             1   // CAPS sent, waiting for reply
#define PCMCAPS_NONE                2   // no or invalid reply: legacy only
#define PCMCAPS_OK                  3   // extended open available

////////////////////////////////////////////////////////////////////////////////////////
// class

class CPacketStream;

class CTranscoder
{
public:
    // constructor
    CTranscoder();
    
    // destructor
    virtual ~CTranscoder();

    // initialization
    bool Init(void);
    void Close(void);

    // locks
    void Lock(void)                     { m_Mutex.lock(); }
    void Unlock(void)                   { m_Mutex.unlock(); }
    
    // status
    bool IsConnected(void) const        { return m_bConnected.load(); }
    void SetPcmWanted(void)             { m_bPcmWanted = true; }
    
    // manage streams
    CCodecStream *GetStream(CPacketStream *, uint8);
    CCodecStream *GetStream(CPacketStream *, uint8, uint32);
    void ReleaseStream(CCodecStream *);
    
    // task
    static void Thread(CTranscoder *);
    void Task(void);

protected:
    // manage streams
    CCodecStream *OpenStream(CPacketStream *, const CBuffer &, uint32, uint8, const uint8 *);
    bool IsPcmCapable(uint8);

    // keepalive helpers
    void HandleKeepalives(void);

    // packet decoding helpers
    bool IsValidKeepAlivePacket(const CBuffer &);
    bool IsValidStreamDescrPacket(const CBuffer &, uint16 *, uint16 *, uint32 *);
    bool IsValidNoStreamAvailablePacket(const CBuffer&, uint32 *);
    bool IsValidCapsPacket(const CBuffer &, uint8 *, uint32 *);

    // packet encoding helpers
    void EncodeKeepAlivePacket(CBuffer *);
    void EncodeCapsPacket(CBuffer *);
    void EncodeOpenstreamPacket(CBuffer *, uint8, uint8, uint8, uint8);
    void EncodeOpenstreamPacket(CBuffer *, uint8, const uint8 *, uint32);
    void EncodeClosestreamPacket(CBuffer *, uint16);
    
protected:
    // streams
    std::mutex                  m_Mutex;
    std::vector<CCodecStream *> m_Streams;

    // sync objects for Openstream
    CSemaphore      m_SemaphoreOpenStream;
    std::atomic<bool> m_bStreamOpened;
    std::atomic<bool> m_bWaitingForStream;  // true while GetStream is waiting for a response
    std::atomic<uint16> m_StreamidOpenStream;
    std::atomic<uint16> m_PortOpenStream;
    std::atomic<uint32> m_uiOpenTransaction;    // 0 = legacy open
    std::atomic<uint32> m_uiLastTransaction;

    // extended open capability, probed once per connection
    std::atomic<bool> m_bPcmWanted;
    std::atomic<int> m_iPcmCaps;
    std::atomic<uint8> m_uiPcmMaxOutputs;
    std::atomic<uint32> m_uiPcmCodecs;
    std::atomic<bool> m_bPcmUnavailableLogged;
    std::atomic<bool> m_bPcmPartialLogged;
    CTimePoint      m_PcmCapsTime;
    
    // thread
    std::atomic<bool> m_bStopThread;
    std::thread     *m_pThread;

    // socket
    CIp             m_Ip;
    CUdpSocket      m_Socket;
    std::atomic<bool> m_bConnected;

    // time
    CTimePoint      m_LastKeepaliveTime;
    CTimePoint      m_LastActivityTime;
};


////////////////////////////////////////////////////////////////////////////////////////
#endif /* ctranscoder_h */
