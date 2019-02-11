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

////////////////////////////////////////////////////////////////////////////////////////
// class

class CVocodecInterface;

class CVocodecChannel
{
public:
    // constructors
    CVocodecChannel(CVocodecInterface *, int, CVocodecInterface *, int, CVocodecInterface *, int, int);
    
    // destructor
    virtual ~CVocodecChannel();
    
    // manage group
    void AddGroupChannel(CVocodecChannel *);
    bool IsAvailable(void) const;

    // open & close
    bool Open(void);
    void Close(void);
    
    // get
    uint8 GetCodecIn(void) const;
    uint8 GetCodecOut1(void) const;
    uint8 GetCodecOut2(void) const;
    uint8 GetCodecsOut(void) const;
    int   GetChannelIn(void) const          { return m_iChannelIn; }
    int   GetChannelOut1(void) const        { return m_iChannelOut1; }
    int   GetChannelOut2(void) const        { return m_iChannelOut2; }
    int   GetSpeechGain(void) const         { return m_iSpeechGain; }
    
    // interfaces
    bool IsInterfaceIn(const CVocodecInterface *interface)      { return (interface == m_InterfaceIn); }
    bool IsInterfaceOut1(const CVocodecInterface *interface)    { return (interface == m_InterfaceOut1); }
    bool IsInterfaceOut2(const CVocodecInterface *interface)    { return (interface == m_InterfaceOut2); }

    // queues
    CPacketQueue *GetPacketQueueIn(void)    { m_QueuePacketIn.Lock(); return &m_QueuePacketIn; }
    void ReleasePacketQueueIn(void)         { m_QueuePacketIn.Unlock(); }
    CPacketQueue *GetPacketQueueOut1(void)  { m_QueuePacketOut1.Lock(); return &m_QueuePacketOut1; }
    void ReleasePacketQueueOut1(void)       { m_QueuePacketOut1.Unlock(); }
    CPacketQueue *GetPacketQueueOut2(void)  { m_QueuePacketOut2.Lock(); return &m_QueuePacketOut2; }
    void ReleasePacketQueueOut2(void)       { m_QueuePacketOut2.Unlock(); }
    CPacketQueue *GetVoiceQueue1(void)      { m_QueueVoice1.Lock(); return &m_QueueVoice1; }
    void ReleaseVoiceQueue1(void)           { m_QueueVoice1.Unlock(); }
    CPacketQueue *GetVoiceQueue2(void)      { m_QueueVoice2.Lock(); return &m_QueueVoice2; }
    void ReleaseVoiceQueue2(void)           { m_QueueVoice2.Unlock(); }

    // operators
    //virtual bool operator ==(const CVocodecChannel &) const   { return false; }
    
protected:
    // queues helpers
    void PurgeAllQueues(void);
    
protected:
    // status
    bool                            m_bOpen;

    // array of grouped channels
    std::vector<CVocodecChannel *>  m_GroupChannels;

    // connected interfaces
    CVocodecInterface               *m_InterfaceIn;
    int                             m_iChannelIn;
    CVocodecInterface               *m_InterfaceOut1;
    int                             m_iChannelOut1;
    CVocodecInterface               *m_InterfaceOut2;
    int                             m_iChannelOut2;
    
    // ambe queues
    CPacketQueue                    m_QueuePacketIn;
    CPacketQueue                    m_QueuePacketOut1;
    CPacketQueue                    m_QueuePacketOut2;
    // voice queues
    CPacketQueue                    m_QueueVoice1;
    CPacketQueue                    m_QueueVoice2;
    
    // settings
    int                             m_iSpeechGain;
    
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cvocodecchannel_h */
