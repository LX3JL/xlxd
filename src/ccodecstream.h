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

#include "csemaphore.h"
#include "cudpsocket.h"
#include "cpacketqueue.h"


////////////////////////////////////////////////////////////////////////////////////////
// define

// frame sizes
#define AMBE_SIZE           9
#define AMBEPLUS_SIZE       9


////////////////////////////////////////////////////////////////////////////////////////
// class

class CPacketStream;

class CCodecStream : public CPacketQueue
{
public:
    // constructor
    CCodecStream(CPacketStream *, uint16, uint8, uint8);
    
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
    bool   IsEmpty(void) const;

    // task
    static void Thread(CCodecStream *);
    void Task(void);
    

protected:
    // packet decoding helpers
    bool IsValidAmbePacket(const CBuffer &, uint8 *, uint8 *, uint8 *, uint8 *);
    
    // packet encoding helpers
    void EncodeAmbePacket(CBuffer *, const uint8 *);

    
protected:
    // data
    uint16          m_uiStreamId;
    uint16          m_uiPort;
    uint8           m_uiPid;
    uint8           m_uiCodecIn;
    uint8           m_uiCodecsOut;

    // socket
    CIp             m_Ip;
    CUdpSocket      m_Socket;
    bool            m_bConnected;
    
    // associated packet stream
    CPacketStream   *m_PacketStream;
    CPacketQueue    m_LocalQueue;

    // thread
    bool            m_bStopThread;
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
};


////////////////////////////////////////////////////////////////////////////////////////
#endif /* ccodecstream_h */
