//
//  cgatekeeper.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 07/11/2015.
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

#include "main.h"
#include "ctimepoint.h"
#include "cgatekeeper.h"

////////////////////////////////////////////////////////////////////////////////////////

CGateKeeper g_GateKeeper;


////////////////////////////////////////////////////////////////////////////////////////
// constructor

CGateKeeper::CGateKeeper()
{
    m_bStopThread = false;
    m_pThread = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////
// destructor

CGateKeeper::~CGateKeeper()
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
// init & clode

bool CGateKeeper::Init(void)
{
    
    // load lists from files
    m_WhiteList.LoadFromFile(WHITELIST_PATH);
    m_BlackList.LoadFromFile(BLACKLIST_PATH);
    
    // reset stop flag
    m_bStopThread = false;
    
    // start  thread;
    m_pThread = new std::thread(CGateKeeper::Thread, this);

    return true;
}

void CGateKeeper::Close(void)
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
// operation

bool CGateKeeper::MayLink(const CCallsign &callsign, const CIp &ip, int protocol) const
{
    bool ok = true;
    
    // first check is IP & callsigned listed OK
    ok &= IsListedOk(callsign, ip);
    
    // then apply any protocol specific authorisation for the operation
    
    // report
    if ( !ok )
    {
        std::cout << "Gatekeeper blocking linking of " << callsign << " @ " << ip << " using protocol " << protocol << std::endl;
    }
    
    // done
    return ok;
}
    
bool CGateKeeper::MayTransmit(const CCallsign &callsign, const CIp &ip, int protocol) const
{
    bool ok = true;
    
    // first check is IP & callsigned listed OK
    ok &= IsListedOk(callsign, ip);
    
    // then apply any protocol specific authorisation for the operation
    
    // report
    if ( !ok )
    {
        std::cout << "Gatekeeper blocking transmiting of " << callsign << " @ " << ip << " using protocol " << protocol << std::endl;
    }
    
    // done
    return ok;
}


////////////////////////////////////////////////////////////////////////////////////////
// thread

void CGateKeeper::Thread(CGateKeeper *This)
{
    while ( !This->m_bStopThread )
    {
        // Wait 30 seconds
        CTimePoint::TaskSleepFor(30000);

        // have lists files changed ?
        if ( This->m_WhiteList.NeedReload() )
        {
            This->m_WhiteList.ReloadFromFile();
        }
        if ( This->m_BlackList.NeedReload() )
        {
            This->m_BlackList.ReloadFromFile();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// operation helpers

bool CGateKeeper::IsListedOk(const CCallsign &callsign, const CIp &ip) const
{
    bool ok = true;
    
    // first check IP
    
    // next, check callsign
    if ( ok )
    {
        // first check if callsign is in white list
        // note if white list is empty, everybody is authorized
        const_cast<CCallsignList &>(m_WhiteList).Lock();
        if ( !m_WhiteList.empty() )
        {
            ok = m_WhiteList.IsListed(callsign);
        }
        const_cast<CCallsignList &>(m_WhiteList).Unlock();
        
        // then check if not blacklisted
        const_cast<CCallsignList &>(m_BlackList).Lock();
        ok &= !m_BlackList.IsListed(callsign);
        const_cast<CCallsignList &>(m_BlackList).Unlock();
    }
    
    // done
    return ok;
    
}
