//
//  cdextrapeer.cpp
//  xlxd
//
//  Created for DExtra/XRF Reflector peering support
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
#include <string.h>
#include "cdextrapeer.h"
#include "creflector.h"

////////////////////////////////////////////////////////////////////////////////////////
// constructor

CDextraPeer::CDextraPeer()
{
    m_iConnectionState = DEXTRA_PEER_STATE_DISCONNECTED;
    m_cLocalModule = ' ';
    m_cRemoteModule = ' ';
    m_uiPort = DEXTRA_PORT;
}

CDextraPeer::CDextraPeer(const CCallsign &callsign, const CIp &ip, char *modules, const CVersion &version)
    : CPeer(callsign, ip, modules, version)
{
    m_iConnectionState = DEXTRA_PEER_STATE_DISCONNECTED;
    m_uiPort = DEXTRA_PORT;

    // Parse modules string - first char is local, second is remote
    if ( modules != NULL && ::strlen(modules) >= 2 )
    {
        // Validate that modules are uppercase letters A-Z
        if ( ::isupper(modules[0]) && ::isupper(modules[1]) )
        {
            m_cLocalModule = modules[0];
            m_cRemoteModule = modules[1];
        }
        else
        {
            m_cLocalModule = ' ';
            m_cRemoteModule = ' ';
        }
    }
    else if ( modules != NULL && ::strlen(modules) == 1 )
    {
        // Single module specified - use same for both
        if ( ::isupper(modules[0]) )
        {
            m_cLocalModule = modules[0];
            m_cRemoteModule = modules[0];
        }
        else
        {
            m_cLocalModule = ' ';
            m_cRemoteModule = ' ';
        }
    }
    else
    {
        m_cLocalModule = ' ';
        m_cRemoteModule = ' ';
    }
    // Note: Peer client is created in protocol handler when connection succeeds,
    // not here, to avoid duplicate client creation
}

CDextraPeer::CDextraPeer(const CDextraPeer &peer)
    : CPeer(peer)
{
    m_iConnectionState = peer.m_iConnectionState;
    m_cLocalModule = peer.m_cLocalModule;
    m_cRemoteModule = peer.m_cRemoteModule;
    m_uiPort = peer.m_uiPort;
    m_ConnectTime = peer.m_ConnectTime;
}

////////////////////////////////////////////////////////////////////////////////////////
// destructor

CDextraPeer::~CDextraPeer()
{
}

////////////////////////////////////////////////////////////////////////////////////////
// status

bool CDextraPeer::IsAlive(void) const
{
    // Peers are considered alive if they've responded recently
    // Use peer-specific keepalive timeout
    return (m_LastKeepaliveTime.DurationSinceNow() < DEXTRA_PEER_KEEPALIVE_TIMEOUT);
}

void CDextraPeer::Alive(void)
{
    m_LastKeepaliveTime.Now();
}
