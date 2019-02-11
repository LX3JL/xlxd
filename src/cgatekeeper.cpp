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
    m_NodeWhiteList.LoadFromFile(WHITELIST_PATH);
    m_NodeBlackList.LoadFromFile(BLACKLIST_PATH);
    m_PeerList.LoadFromFile(INTERLINKLIST_PATH);
    
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
// authorisations

bool CGateKeeper::MayLink(const CCallsign &callsign, const CIp &ip, int protocol, char *modules) const
{
    bool ok = true;
    
    switch (protocol)
    {
        // repeaters
        case PROTOCOL_DEXTRA:
        case PROTOCOL_DEXTRA_OPEN:
        case PROTOCOL_DPLUS:
        case PROTOCOL_DCS:
        case PROTOCOL_DMRPLUS:
        case PROTOCOL_DMRMMDVM:
            // first check is IP & callsigned listed OK
            ok &= IsNodeListedOk(callsign, ip);
            // todo: then apply any protocol specific authorisation for the operation
            break;
            
        // XLX interlinks
        case PROTOCOL_XLX:
            ok &= IsPeerListedOk(callsign, ip, modules);
            break;
            
        // unsupported
        case PROTOCOL_NONE:
        default:
            ok = false;
            break;
    }
    
    // report
    if ( !ok )
    {
        std::cout << "Gatekeeper blocking linking of " << callsign << " @ " << ip << " using protocol " << protocol << std::endl;
    }
    
    // done
    return ok;
}
    
bool CGateKeeper::MayTransmit(const CCallsign &callsign, const CIp &ip, int protocol, char module) const
{
    bool ok = true;
    
    switch (protocol)
    {
        // repeaters, protocol specific
        case PROTOCOL_ANY:
        case PROTOCOL_DEXTRA:
        case PROTOCOL_DEXTRA_OPEN:
        case PROTOCOL_DPLUS:
        case PROTOCOL_DCS:
        case PROTOCOL_DMRPLUS:
        case PROTOCOL_DMRMMDVM:
            // first check is IP & callsigned listed OK
            ok &= IsNodeListedOk(callsign, ip, module);
            // todo: then apply any protocol specific authorisation for the operation
            break;
            
        // XLX interlinks
        case PROTOCOL_XLX:
            ok &= IsPeerListedOk(callsign, ip, module);
            break;
            
        // unsupported
        case PROTOCOL_NONE:
        default:
            ok = false;
            break;
    }
    
    // report
    if ( !ok )
    {
        std::cout << "Gatekeeper blocking transmitting of " << callsign << " @ " << ip << " using protocol " << protocol << std::endl;
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
        if ( This->m_NodeWhiteList.NeedReload() )
        {
            This->m_NodeWhiteList.ReloadFromFile();
        }
        if ( This->m_NodeBlackList.NeedReload() )
        {
            This->m_NodeBlackList.ReloadFromFile();
        }
        if ( This->m_PeerList.NeedReload() )
        {
            This->m_PeerList.ReloadFromFile();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// operation helpers

bool CGateKeeper::IsNodeListedOk(const CCallsign &callsign, const CIp &ip, char module) const
{
    bool ok = true;
    
    // first check IP
    
    // next, check callsign
    if ( ok )
    {
        // first check if callsign is in white list
        // note if white list is empty, everybody is authorized
        const_cast<CCallsignList &>(m_NodeWhiteList).Lock();
        if ( !m_NodeWhiteList.empty() )
        {
            ok = m_NodeWhiteList.IsCallsignListedWithWildcard(callsign, module);
        }
        const_cast<CCallsignList &>(m_NodeWhiteList).Unlock();
        
        // then check if not blacklisted
        const_cast<CCallsignList &>(m_NodeBlackList).Lock();
        ok &= !m_NodeBlackList.IsCallsignListedWithWildcard(callsign);
        const_cast<CCallsignList &>(m_NodeBlackList).Unlock();
    }
    
    // done
    return ok;
    
}

bool CGateKeeper::IsPeerListedOk(const CCallsign &callsign, const CIp &ip, char module) const
{
    bool ok = true;
    
    // first check IP
    
    // next, check callsign
    if ( ok )
    {
        // look for an exact match in the list
        const_cast<CPeerCallsignList &>(m_PeerList).Lock();
        if ( !m_PeerList.empty() )
        {
            ok = m_PeerList.IsCallsignListed(callsign, module);
        }
        const_cast<CPeerCallsignList &>(m_PeerList).Unlock();
    }
    
    // done
    return ok;
}

bool CGateKeeper::IsPeerListedOk(const CCallsign &callsign, const CIp &ip, char *modules) const
{
    bool ok = true;
    
    // first check IP
    
    // next, check callsign
    if ( ok )
    {
        // look for an exact match in the list
        const_cast<CPeerCallsignList &>(m_PeerList).Lock();
        if ( !m_PeerList.empty() )
        {
            ok = m_PeerList.IsCallsignListed(callsign, modules);
        }
        const_cast<CPeerCallsignList &>(m_PeerList).Unlock();
    }
    
    // done
    return ok;
}

