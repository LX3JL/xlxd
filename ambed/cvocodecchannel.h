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

#include "cpacketqueue.h"
#include "cagc.h"

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
    CAGC& GetAGC()                          { return m_AGC; };
    
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

    // operators
    //virtual bool operator ==(const CVocodecChannel &) const   { return false; }
    
protected:
    // queues helpers
    void PurgeAllQueues(void);
    
protected:
    // status
    bool                m_bOpen;

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
    
    // settings
    int                 m_iSpeechGain;
    
private:
    CAGC m_AGC;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cvocodecchannel_h */
