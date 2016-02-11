//
//  cdcsclient.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 07/11/2015.
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

#ifndef cdcsclient_h
#define cdcsclient_h

#include "cclient.h"

////////////////////////////////////////////////////////////////////////////////////////
// define


////////////////////////////////////////////////////////////////////////////////////////
// class

class CDcsClient : public CClient
{
public:
    // constructors
    CDcsClient();
    CDcsClient(const CCallsign &, const CIp &, char = ' ');
    CDcsClient(const CDcsClient &);
    
    // destructor
    virtual ~CDcsClient() {};
    
    // identity
    int GetProtocol(void) const                 { return PROTOCOL_DCS; }
    const char *GetProtocolName(void) const     { return "DCS"; }
    bool IsNode(void) const                     { return true; }
    
    // status
    bool IsAlive(void) const;
};

////////////////////////////////////////////////////////////////////////////////////////

#endif /* cdcsclient_h */
