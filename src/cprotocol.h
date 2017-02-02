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

#include "cudpsocket.h"
#include "cpacketstream.h"
#include "cdvheaderpacket.h"
#include "cdvframepacket.h"
#include "cdvlastframepacket.h"

////////////////////////////////////////////////////////////////////////////////////////


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
    void ReleaseQueue(void)             { m_Queue.Unlock(); }
    
    // get
    const CCallsign &GetReflectorCallsign(void)const { return m_ReflectorCallsign; }
    
    // task
    static void Thread(CProtocol *);
    virtual void Task(void) {};
    
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
    
    // stream handle helpers
    CPacketStream *GetStream(uint16, const CIp * = NULL);
    void CheckStreamsTimeout(void);
    
    // queue helper
    virtual void HandleQueue(void);
    
    // keepalive helpers
    virtual void HandleKeepalives(void) {}

    // syntax helper
    bool IsNumber(char) const;
    bool IsLetter(char) const;
    bool IsSpace(char) const;

protected:
    // socket
    CUdpSocket      m_Socket;
    
    // streams
    std::vector<CPacketStream *> m_Streams;
    
    // queue
    CPacketQueue    m_Queue;
    
    // thread
    bool            m_bStopThread;
    std::thread     *m_pThread;
    
    // identity
    CCallsign       m_ReflectorCallsign;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cprotocol_h */
