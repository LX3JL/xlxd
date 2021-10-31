//
//  cimrsclient.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 29/10/2019.
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
#include "cimrsclient.h"


////////////////////////////////////////////////////////////////////////////////////////
// constructors

CImrsClient::CImrsClient()
{
}

CImrsClient::CImrsClient(const CCallsign &callsign, const CIp &ip, char reflectorModule)
    : CClient(callsign, ip, reflectorModule)
{
}

CImrsClient::CImrsClient(const CImrsClient &client)
    : CClient(client)
{
}

////////////////////////////////////////////////////////////////////////////////////////
// status

bool CImrsClient::IsAlive(void) const
{
    return (m_LastKeepaliveTime.DurationSinceNow() < IMRS_KEEPALIVE_TIMEOUT);
}
