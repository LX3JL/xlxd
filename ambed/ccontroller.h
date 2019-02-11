//
//  ccontroller.h
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

#ifndef ccontroller_h
#define ccontroller_h

#include "ccallsign.h"
#include "cstream.h"

////////////////////////////////////////////////////////////////////////////////////////
// class

class CController
{
public:
    // constructors
    CController();
    
    // destructor
    virtual ~CController();
    
    // initialization
    bool Init(void);
    void Close(void);

    // locks
    void Lock(void)                     { m_Mutex.lock(); }
    void Unlock(void)                   { m_Mutex.unlock(); }
    
    // get
    const CIp &GetListenIp(void) const  { return (const CIp &)m_Ip; }

    // set
    void SetListenIp(const CIp &ip)     { m_Ip = ip; }

    // streams management
    CStream *OpenStream(const CCallsign &, const CIp &, uint8, uint8);
    void CloseStream(CStream *);
    void CloseStream(uint16);
    
    // task
    static void Thread(CController *);
    void Task(void);

protected:
    // packet decoding helpers    
    bool IsValidKeepAlivePacket(const CBuffer &, CCallsign *);
    bool IsValidOpenstreamPacket(const CBuffer &, CCallsign *, uint8 *, uint8 *);
    bool IsValidClosestreamPacket(const CBuffer &, uint16 *);
    
    // packet encoding helpers
    void EncodeKeepAlivePacket(CBuffer *);
    void EncodeStreamDescrPacket(CBuffer *, const CStream &);
    void EncodeNoStreamAvailablePacket(CBuffer *);

    // codec helpers
    bool IsValidCodecIn(uint8);
    bool IsValidCodecsOut(uint8);
    
protected:
    // control socket
    CIp             m_Ip;
    CUdpSocket      m_Socket;
    
    // streams
    uint16                 m_uiLastStreamId;
    std::mutex             m_Mutex;
    std::vector<CStream *> m_Streams;

    // thread
    bool            m_bStopThread;
    std::thread     *m_pThread;
    
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* ccontroller_h */
