//
//  cg3client.cpp
//  xlxd
//
//  Created by Marius Petrescu (YO2LOJ) on 03/06/2019.
//  Copyright © 2019 Marius Petrescu (YO2LOJ). All rights reserved.
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
#include "cg3client.h"


////////////////////////////////////////////////////////////////////////////////////////
// constructors

CG3Client::CG3Client()
{
}

CG3Client::CG3Client(const CCallsign &callsign, const CIp &ip, char reflectorModule)
    : CClient(callsign, ip, reflectorModule)
{

}

CG3Client::CG3Client(const CG3Client &client)
    : CClient(client)
{
}

////////////////////////////////////////////////////////////////////////////////////////
// status

bool CG3Client::IsAlive(void) const
{
    return (m_LastKeepaliveTime.DurationSinceNow() < G3_KEEPALIVE_TIMEOUT);
}

