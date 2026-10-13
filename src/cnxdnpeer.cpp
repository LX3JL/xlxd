//
//  cnxdnpeer.cpp
//  xlxd
//
//  Created for NXDN Reflector peering support
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
#include "creflector.h"
#include "cnxdnpeer.h"
#include "cnxdnpeerclient.h"


////////////////////////////////////////////////////////////////////////////////////////
// constructor

CNxdnPeer::CNxdnPeer()
{
    m_uiNxdnId = 0;
}

CNxdnPeer::CNxdnPeer(const CCallsign &callsign, const CIp &ip, char *modules, const CVersion &version)
: CPeer(callsign, ip, modules, version)
{
    m_uiNxdnId = 0;

    // NXDN reflectors only support a single module (they are single-room)
    // Create one client for the single bridged module
    if ( modules != NULL && ::strlen(modules) >= 1 )
    {
        // Only use the first module character (validation ensures single module)
        char module = modules[0];
        CNxdnPeerClient *client = new CNxdnPeerClient(callsign, ip, module);
        m_Clients.push_back(client);
    }
}

CNxdnPeer::CNxdnPeer(const CNxdnPeer &peer)
: CPeer(peer)
{
    m_uiNxdnId = peer.m_uiNxdnId;

    for ( int i = 0; i < peer.m_Clients.size(); i++ )
    {
        CNxdnPeerClient *client = new CNxdnPeerClient((const CNxdnPeerClient &)*(peer.m_Clients[i]));
        // grow vector capacity if needed
        if ( m_Clients.capacity() == m_Clients.size() )
        {
            m_Clients.reserve(m_Clients.capacity()+10);
        }
        // and append
        m_Clients.push_back(client);
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// destructor

CNxdnPeer::~CNxdnPeer()
{
}

////////////////////////////////////////////////////////////////////////////////////////
// status

bool CNxdnPeer::IsAlive(void) const
{
    return (m_LastKeepaliveTime.DurationSinceNow() < NXDN_PEER_KEEPALIVE_TIMEOUT);
}

void CNxdnPeer::Alive(void)
{
    m_LastKeepaliveTime.Now();
}
