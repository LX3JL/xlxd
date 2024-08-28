//
//  cxlxpeer.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 10/12/2016.
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

#ifndef cxlxpeer_h
#define cxlxpeer_h

#include "cpeer.h"
#include "cxlxclient.h"

////////////////////////////////////////////////////////////////////////////////////////
// define


////////////////////////////////////////////////////////////////////////////////////////
// class

class CXlxPeer : public CPeer
{
public:
    // constructors
    CXlxPeer();
    CXlxPeer(const CCallsign &, const CIp &, char *, const CVersion &);
    CXlxPeer(const CXlxPeer &);
    
    // destructor
    ~CXlxPeer();
    
    // status
    bool IsAlive(void) const;
    
    // identity
    int GetProtocol(void) const                 { return PROTOCOL_XLX; }
    const char *GetProtocolName(void) const     { return "XLX"; }
    
    // revision helper
    static int GetProtocolRevision(const CVersion &);
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cxlxpeer_h */
