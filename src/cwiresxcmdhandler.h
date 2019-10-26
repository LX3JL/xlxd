//
//  cwiresxcmdhandler.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 09/10/2019.
//  Copyright © 2019 Jean-Luc Deltombe (LX3JL). All rights reserved.
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

#ifndef cwiresxcmdhandler_h
#define cwiresxcmdhandler_h

#include "cwiresxinfo.h"
#include "cwiresxcmdqueue.h"
#include "cwiresxpacketqueue.h"

////////////////////////////////////////////////////////////////////////////////////////

#define WIRESX_REPLY_DELAY  (1.000)

////////////////////////////////////////////////////////////////////////////////////////
// class

class CWiresxCmdHandler
{
public:
    // constructor
    CWiresxCmdHandler();
    
    // destructor
    virtual ~CWiresxCmdHandler();
    
    // initialization
    virtual bool Init(void);
    virtual void Close(void);

    // queues
    CWiresxCmdQueue *GetCmdQueue(void)          { m_CmdQueue.Lock(); return &m_CmdQueue; }
    void ReleaseCmdQueue(void)                  { m_CmdQueue.Unlock(); }
    CWiresxPacketQueue *GetPacketQueue(void)    { m_PacketQueue.Lock(); return &m_PacketQueue; }
    void ReleasePacketQueue(void)               { m_PacketQueue.Unlock(); }

    // get
    
    // task
    static void Thread(CWiresxCmdHandler *);
    virtual void Task(void);
    
protected:
    // packet encoding
    bool ReplyToWiresxDxReqPacket(const CIp &, const CWiresxInfo &, char);
    bool ReplyToWiresxAllReqPacket(const CIp &, const CWiresxInfo &, int);
    bool ReplyToWiresxConnReqPacket(const CIp &, const CWiresxInfo &, char);
    bool ReplyToWiresxDiscReqPacket(const CIp &, const CWiresxInfo &);
    
    // packet encoding helpers
    bool EncodeAndSendWiresxPacket(const CIp &, const CBuffer &, const CWiresxInfo &);
    uint8 WiresxCalcFt(uint, uint) const;
    void SendPacket(const CIp &, uint8 *);

    // debug
    bool DebugTestDecodePacket(const CBuffer &);

protected:
    // data
    CWiresxInfo        m_ReflectorWiresxInfo;
    uint8_t            m_seqNo;
    
    // queues
    CWiresxCmdQueue    m_CmdQueue;
    CWiresxPacketQueue m_PacketQueue;
    
    // thread
    bool               m_bStopThread;
    std::thread        *m_pThread;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cwiresxcmdhandler_h */
