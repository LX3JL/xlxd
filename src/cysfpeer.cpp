//
//  cysfpeer.cpp
//  xlxd
//
//  Created for YSF Reflector peering support
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
#include "cysfpeer.h"
#include "cysfpeerclient.h"


////////////////////////////////////////////////////////////////////////////////////////
// constructor

CYsfPeer::CYsfPeer()
{
    m_uiYsfId = 0;
}

CYsfPeer::CYsfPeer(const CCallsign &callsign, const CIp &ip, char *modules, const CVersion &version)
: CPeer(callsign, ip, modules, version)
{
    m_uiYsfId = 0;

    std::cout << "Adding YSF peer" << std::endl;

    // YSF reflectors only support a single module (they are single-room)
    // Create one client for the single bridged module
    if ( modules != NULL && ::strlen(modules) >= 1 )
    {
        // Only use the first module character (validation ensures single module)
        char module = modules[0];
        CYsfPeerClient *client = new CYsfPeerClient(callsign, ip, module);
        m_Clients.push_back(client);
    }
}

CYsfPeer::CYsfPeer(const CYsfPeer &peer)
: CPeer(peer)
{
    m_uiYsfId = peer.m_uiYsfId;

    for ( int i = 0; i < peer.m_Clients.size(); i++ )
    {
        CYsfPeerClient *client = new CYsfPeerClient((const CYsfPeerClient &)*(peer.m_Clients[i]));
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

CYsfPeer::~CYsfPeer()
{
}

////////////////////////////////////////////////////////////////////////////////////////
// status

bool CYsfPeer::IsAlive(void) const
{
    return (m_LastKeepaliveTime.DurationSinceNow() < YSF_PEER_KEEPALIVE_TIMEOUT);
}

void CYsfPeer::Alive(void)
{
    m_LastKeepaliveTime.Now();
}
