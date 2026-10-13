//
//  cnxdnpeerclient.cpp
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

#include <string.h>
#include "main.h"
#include "cnxdnpeerclient.h"


////////////////////////////////////////////////////////////////////////////////////////
// constructors

CNxdnPeerClient::CNxdnPeerClient()
{
}

CNxdnPeerClient::CNxdnPeerClient(const CCallsign &callsign, const CIp &ip, char reflectorModule)
    : CClient(callsign, ip, reflectorModule)
{
}

CNxdnPeerClient::CNxdnPeerClient(const CNxdnPeerClient &client)
    : CClient(client)
{
}

////////////////////////////////////////////////////////////////////////////////////////
// status

bool CNxdnPeerClient::IsAlive(void) const
{
    return (m_LastKeepaliveTime.DurationSinceNow() < NXDN_PEER_KEEPALIVE_TIMEOUT);
}
