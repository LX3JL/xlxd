//
//  cstream.h
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

#ifndef cstream_h
#define cstream_h

#include "cudpsocket.h"
#include "ccallsign.h"
#include "cvocodecchannel.h"

////////////////////////////////////////////////////////////////////////////////////////
// class

class CStream
{
public:
    // constructors
    CStream();
    CStream(uint16, const CCallsign &, const CIp &, uint8, uint8);
    
    // destructor
    virtual ~CStream();
    
    // initialization
    bool Init(uint16);
    void Close(void);
    
    // get
    uint16  GetId(void) const           { return m_uiId; }
    uint16  GetPort(void) const         { return m_uiPort; }
    uint8   GetCodecIn(void) const      { return m_uiCodecIn; }
    uint8   GetCodecsOut(void) const    { return m_uiCodecsOut; }
    
    // activity timer
    bool    IsActive(void) const        { return m_LastActivity.DurationSinceNow() <= STREAM_ACTIVITY_TIMEOUT; }

    // task
    static void Thread(CStream *);
    void Task(void);

protected:
    // packet decoding helpers
    bool IsValidDvFramePacket(const CBuffer &, uint8 *, uint8 *);
    
    // packet encodeing helpers
    void EncodeDvFramePacket(CBuffer *, uint8, uint8, uint8 *, uint8, uint8 *);

    // queues helpers
    void PurgeAllQueues(void);
    
protected:
    // data
    uint16          m_uiId;
    CUdpSocket      m_Socket;
    uint16          m_uiPort;
    uint8           m_uiCodecIn;
    uint8           m_uiCodecsOut;
    CVocodecChannel *m_VocodecChannel;
    
    // client details
    CCallsign       m_Callsign;
    CIp             m_Ip;
    
    // counters
    int             m_iTotalPackets;
    int             m_iLostPackets;
    
    // activity timer
    CTimePoint      m_LastActivity;
    
    // thread
    bool            m_bStopThread;
    std::thread     *m_pThread;

    // outgoing packet queues
    CPacketQueue    m_QueuePacketOut1;
    CPacketQueue    m_QueuePacketOut2;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cstream_h */
