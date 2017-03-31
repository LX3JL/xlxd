//
//  cdextraclient.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 31/10/2015.
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

#ifndef cdextraclient_h
#define cdextraclient_h

#include "cclient.h"

////////////////////////////////////////////////////////////////////////////////////////
// define


////////////////////////////////////////////////////////////////////////////////////////
// class

class CDextraClient : public CClient
{
public:
    // constructors
    CDextraClient();
    CDextraClient(const CCallsign &, const CIp &, char = ' ', int = 0);
    CDextraClient(const CDextraClient &);
    
    // destructor
    virtual ~CDextraClient() {};
    
    // identity
    int GetProtocol(void) const                 { return PROTOCOL_DEXTRA; }
    int GetProtocolRevision(void) const         { return m_ProtRev; }
    const char *GetProtocolName(void) const     { return "DExtra"; }
    bool IsNode(void) const                     { return true; }
    
    // status
    bool IsAlive(void) const;

protected:
    // data
    int     m_ProtRev;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cdextraclient_h */
