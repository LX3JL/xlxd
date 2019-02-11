//
//  cvocodecinterface.cpp
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

#include "main.h"
#include "ctimepoint.h"
#include "cpacketqueue.h"
#include "cvocodecinterface.h"


////////////////////////////////////////////////////////////////////////////////////////
// constructor

CVocodecInterface::CVocodecInterface()
{
    m_bStopThread = false;
    m_pThread = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////
// destructor

CVocodecInterface::~CVocodecInterface()
{
    // stop thread
    m_bStopThread = true;
    if ( m_pThread != NULL )
    {
        m_pThread->join();
        delete m_pThread;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// initialization

bool CVocodecInterface::Init(void)
{
    // no open channel state
    for ( int i = 0; i < GetNbChannels(); i++ )
    {
        m_ChannelIn.push_back(NULL);
        m_ChannelOut.push_back(NULL);
    }

    // reset stop flag
    m_bStopThread = false;
    
    // start  thread;
    m_pThread = new std::thread(CVocodecInterface::Thread, this);
    
    // done
    return true;
}


////////////////////////////////////////////////////////////////////////////////////////
// thread

void CVocodecInterface::Thread(CVocodecInterface *This)
{
    while ( !This->m_bStopThread )
    {
        This->Task();
    }
}


////////////////////////////////////////////////////////////////////////////////////////
// manage open Channel state

void CVocodecInterface::SetChannelWithChannelIn(CVocodecChannel *Channel, int iCh)
{
    m_MutexChannels.lock();
    m_ChannelIn[iCh] = Channel;
    m_ChannelOut[iCh] = NULL;
    m_MutexChannels.unlock();
}

void CVocodecInterface::SetChannelWithChannelOut(CVocodecChannel *Channel, int iCh)
{
    m_MutexChannels.lock();
    m_ChannelIn[iCh] = NULL;
    m_ChannelOut[iCh] = Channel;
    m_MutexChannels.unlock();
}

CVocodecChannel *CVocodecInterface::GetChannelWithChannelIn(int iCh)
{
    CVocodecChannel *Channel;
    m_MutexChannels.lock();
    Channel = m_ChannelIn[iCh];
    m_MutexChannels.unlock();
    return Channel;
}

CVocodecChannel *CVocodecInterface::GetChannelWithChannelOut(int iCh)
{
    CVocodecChannel *Channel;
    m_MutexChannels.lock();
    Channel = m_ChannelOut[iCh];
    m_MutexChannels.unlock();
    return Channel;
}

