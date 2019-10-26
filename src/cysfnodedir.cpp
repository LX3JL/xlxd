//
//  cysfnodedir.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 08/10/2019.
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

#include <string.h>
#include "main.h"
#include "creflector.h"
#include "cysfnodedir.h"


////////////////////////////////////////////////////////////////////////////////////////
// constructor & destructor

CYsfNodeDir::CYsfNodeDir()
{
    m_bStopThread = false;
    m_pThread = NULL;
}

CYsfNodeDir::~CYsfNodeDir()
{
    // kill threads
    m_bStopThread = true;
    if ( m_pThread != NULL )
    {
        m_pThread->join();
        delete m_pThread;
    }
}


////////////////////////////////////////////////////////////////////////////////////////
// init & close

bool CYsfNodeDir::Init(void)
{
    // load content
    Reload();
    
    // reset stop flag
    m_bStopThread = false;
    
    // start  thread;
    m_pThread = new std::thread(CYsfNodeDir::Thread, this);

    return true;
}

void CYsfNodeDir::Close(void)
{
    m_bStopThread = true;
    if ( m_pThread != NULL )
    {
        m_pThread->join();
        delete m_pThread;
        m_pThread = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// thread

void CYsfNodeDir::Thread(CYsfNodeDir *This)
{
    while ( !This->m_bStopThread )
    {
        // Wait 30 seconds
        CTimePoint::TaskSleepFor(YSFNODEDB_REFRESH_RATE * 60000);

        // have lists files changed ?
        if ( This->NeedReload() )
        {
               This->Reload();
        }
     }
}

////////////////////////////////////////////////////////////////////////////////////////
// Reload

bool CYsfNodeDir::Reload(void)
{
    CBuffer buffer;
    bool ok = false;
    
    if ( LoadContent(&buffer) )
    {
        Lock();
        {
            ok = RefreshContent(buffer);
        }
        Unlock();
    }
    return ok;
}

////////////////////////////////////////////////////////////////////////////////////////
// find

bool CYsfNodeDir::FindFrequencies(const CCallsign &callsign, uint32 *txfreq, uint32 *rxfreq)
{
    auto found = find(callsign);
    if ( found != end() )
    {
        *txfreq = found->second.GetTxFrequency();
        *rxfreq = found->second.GetRxFrequency();
        return true;
    }
    else
    {
        *txfreq = YSF_DEFAULT_NODE_TX_FREQ;
        *rxfreq = YSF_DEFAULT_NODE_RX_FREQ;
        return false;
    }
}
