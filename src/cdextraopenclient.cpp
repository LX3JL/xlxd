//
//  cdextraopenclient.cpp
//  xlxd
//
//  Created by Antony Chazapis (SV9OAN) on 19/12/2018.
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
#include "cdextraopenclient.h"


////////////////////////////////////////////////////////////////////////////////////////
// constructors

CDextraOpenClient::CDextraOpenClient()
    : CDextraClient()
{}

CDextraOpenClient::CDextraOpenClient(const CCallsign &callsign, const CIp &ip, char reflectorModule, int protRev)
    : CDextraClient(callsign, ip, reflectorModule, protRev)
{}

CDextraOpenClient::CDextraOpenClient(const CDextraClient &client)
    : CDextraClient(client)
{}
