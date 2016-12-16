//
//  cxlxclient.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 28/01/2016.
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

#include <string.h>
#include "main.h"
#include "cxlxclient.h"


////////////////////////////////////////////////////////////////////////////////////////
// constructors

CXlxClient::CXlxClient()
{
}

CXlxClient::CXlxClient(const CCallsign &callsign, const CIp &ip, char reflectorModule)
: CClient(callsign, ip, reflectorModule)
{
}

CXlxClient::CXlxClient(const CXlxClient &client)
: CClient(client)
{
}

////////////////////////////////////////////////////////////////////////////////////////
// status

bool CXlxClient::IsAlive(void) const
{
    return (m_LastKeepaliveTime.DurationSinceNow() < XLX_KEEPALIVE_TIMEOUT);
}

