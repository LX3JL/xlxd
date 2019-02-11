//
//  cdextraopenclient.h
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

#ifndef cdextraopenclient_h
#define cdextraopenclient_h

#include "cdextraclient.h"

////////////////////////////////////////////////////////////////////////////////////////
// define


////////////////////////////////////////////////////////////////////////////////////////
// class

class CDextraOpenClient : public CDextraClient
{
public:
    // constructors
    CDextraOpenClient();
    CDextraOpenClient(const CCallsign &, const CIp &, char = ' ', int = 0);
    CDextraOpenClient(const CDextraClient &);
    
    // destructor
    virtual ~CDextraOpenClient() {};

    // identity
    int GetProtocol(void) const                 { return PROTOCOL_DEXTRA_OPEN; }
    const char *GetProtocolName(void) const     { return "DExtra Open"; }
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cdextraopenclient_h */
