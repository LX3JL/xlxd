//
//  cdmrmmdvmclient.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 04/03/2017.
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

#ifndef cdmrmmdvmclient_h
#define cdmrmmdvmclient_h

#include "cclient.h"

////////////////////////////////////////////////////////////////////////////////////////
// define


////////////////////////////////////////////////////////////////////////////////////////
// class

class CDmrmmdvmClient : public CClient
{
public:
    // constructors
    CDmrmmdvmClient();
    CDmrmmdvmClient(const CCallsign &, const CIp &, char = ' ');
    CDmrmmdvmClient(const CDmrmmdvmClient &);
    
    // destructor
    virtual ~CDmrmmdvmClient() {};
    
    // identity
    int GetProtocol(void) const                 { return PROTOCOL_DMRMMDVM; }
    const char *GetProtocolName(void) const     { return "DMRMmdvm"; }
    bool IsNode(void) const                     { return true; }
    
    // status
    bool IsAlive(void) const;
};

////////////////////////////////////////////////////////////////////////////////////////

#endif /* cdmrmmdvmclient_h */
