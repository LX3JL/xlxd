//
//  cvocodecinterface.h
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

#ifndef cvocodecinterface_h
#define cvocodecinterface_h

#include "cvocodecchannel.h"

////////////////////////////////////////////////////////////////////////////////////////
// class

class CVocodecs;

class CVocodecInterface
{
public:
    // constructors
    CVocodecInterface();
    
    // destructor
    virtual ~CVocodecInterface();
    
    // initialization
    virtual bool Init(void);
    
    // get
    virtual const char *GetName(void) const                     { return ""; }
    
    // manage channels
    virtual int   GetNbChannels(void) const                     { return 0; }
    virtual uint8 GetChannelCodec(int) const                    { return CODEC_NONE; }

    // manage open channel state
    void SetChannelWithChannelIn(CVocodecChannel *, int);
    void SetChannelWithChannelOut(CVocodecChannel *, int);
    CVocodecChannel *GetChannelWithChannelIn(int);
    CVocodecChannel *GetChannelWithChannelOut(int);
    
    // task
    static void Thread(CVocodecInterface *);
    virtual void Task(void) {};

    // operators
    virtual bool operator ==(const CVocodecInterface &) const   { return false; }
        
protected:
    // open channel state
    std::mutex                      m_MutexChannels;
    std::vector<CVocodecChannel *>  m_ChannelIn;
    std::vector<CVocodecChannel *>  m_ChannelOut;

    // thread
    bool                            m_bStopThread;
    std::thread                     *m_pThread;
    
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cvocodecinterface_h */
