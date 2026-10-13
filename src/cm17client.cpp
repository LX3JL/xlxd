//
//  cm17client.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 28/11/2025.
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
#include "cm17client.h"


////////////////////////////////////////////////////////////////////////////////////////
// constructors

CM17Client::CM17Client()
{
    m_bListenOnly = false;
}

CM17Client::CM17Client(const CCallsign &callsign, const CIp &ip, char reflectorModule, bool listenOnly)
: CClient(callsign, ip, reflectorModule)
{
    m_bListenOnly = listenOnly;
}

CM17Client::CM17Client(const CM17Client &client)
: CClient(client)
{
    m_bListenOnly = client.m_bListenOnly;
}

////////////////////////////////////////////////////////////////////////////////////////
// status

bool CM17Client::IsAlive(void) const
{
    return (m_LastKeepaliveTime.DurationSinceNow() < M17_KEEPALIVE_TIMEOUT);
}
