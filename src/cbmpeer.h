//
//  cbmpeer.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 20/01/2017.
//  Copyright © 2016 Jean-Luc Deltombe (LX3JL). All rights reserved.
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

#ifndef cbmpeer_h
#define cbmpeer_h


#include "cpeer.h"
#include "cbmclient.h"

////////////////////////////////////////////////////////////////////////////////////////
// define


////////////////////////////////////////////////////////////////////////////////////////
// class

class CBmPeer : public CPeer
{
public:
    // constructors
    CBmPeer();
    CBmPeer(const CCallsign &, const CIp &, const char *, const CVersion &);
    CBmPeer(const CBmPeer &);
    
    // destructor
    ~CBmPeer();
    
    // status
    bool IsAlive(void) const;
    
    // identity
    int GetProtocol(void) const                 { return PROTOCOL_XLX; }
    const char *GetProtocolName(void) const     { return "XLX"; }
    
    // revision helper
    static int GetProtocolRevision(const CVersion &);
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cbmpeer_h */
