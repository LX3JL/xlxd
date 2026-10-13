//
//  cdextrapeerclient.cpp
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

#include <string.h>
#include "main.h"
#include "cdextrapeerclient.h"


////////////////////////////////////////////////////////////////////////////////////////
// constructors

CDextraPeerClient::CDextraPeerClient()
{
    m_bIsPeer = false;
}

CDextraPeerClient::CDextraPeerClient(const CCallsign &callsign, const CIp &ip, char reflectorModule)
    : CClient(callsign, ip, reflectorModule)
{
    m_bIsPeer = true;  // Peer clients are peers by definition
}

CDextraPeerClient::CDextraPeerClient(const CDextraPeerClient &client)
    : CClient(client)
{
    m_bIsPeer = client.m_bIsPeer;
}

////////////////////////////////////////////////////////////////////////////////////////
// status

bool CDextraPeerClient::IsAlive(void) const
{
    return (m_LastKeepaliveTime.DurationSinceNow() < DEXTRA_KEEPALIVE_TIMEOUT);
}
