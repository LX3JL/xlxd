//
//  cp25peer.cpp
//  xlxd
//
//  Created for P25 Reflector peering support
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
#include "cp25peer.h"
#include "cp25peerclient.h"


////////////////////////////////////////////////////////////////////////////////////////
// constructor

CP25Peer::CP25Peer()
{
    m_uiP25Id = 0;
    m_uiP25Tg = 0;
}

CP25Peer::CP25Peer(const CCallsign &callsign, const CIp &ip, char *modules, const CVersion &version)
: CPeer(callsign, ip, modules, version)
{
    m_uiP25Id = 0;
    m_uiP25Tg = 0;

    // P25 reflectors only support a single module (they are single-room)
    // Create one client for the single bridged module
    if ( modules != NULL && ::strlen(modules) >= 1 )
    {
        // Only use the first module character (validation ensures single module)
        char module = modules[0];
        CP25PeerClient *client = new CP25PeerClient(callsign, ip, module);
        m_Clients.push_back(client);
    }
}

CP25Peer::CP25Peer(const CP25Peer &peer)
: CPeer(peer)
{
    m_uiP25Id = peer.m_uiP25Id;
    m_uiP25Tg = peer.m_uiP25Tg;

    for ( int i = 0; i < peer.m_Clients.size(); i++ )
    {
        CP25PeerClient *client = new CP25PeerClient((const CP25PeerClient &)*(peer.m_Clients[i]));
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

CP25Peer::~CP25Peer()
{
}

////////////////////////////////////////////////////////////////////////////////////////
// status

bool CP25Peer::IsAlive(void) const
{
    return (m_LastKeepaliveTime.DurationSinceNow() < P25_PEER_KEEPALIVE_TIMEOUT);
}

void CP25Peer::Alive(void)
{
    m_LastKeepaliveTime.Now();
}
