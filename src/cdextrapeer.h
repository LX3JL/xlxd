//
//  cdextrapeer.h
//  xlxd
//
//  Created by Antony Chazapis (SV9OAN) on 25/2/2018.
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

#ifndef cdextrapeer_h
#define cdextrapeer_h

#include "cpeer.h"
#include "cdextraclient.h"

////////////////////////////////////////////////////////////////////////////////////////
// define


////////////////////////////////////////////////////////////////////////////////////////
// class

class CDextraPeer : public CPeer
{
public:
    // constructors
    CDextraPeer();
    CDextraPeer(const CCallsign &, const CIp &, const char *, const CVersion &);
    CDextraPeer(const CDextraPeer &);
    
    // destructor
    ~CDextraPeer();
    
    // status
    bool IsAlive(void) const;
    
    // identity
    int GetProtocol(void) const                 { return PROTOCOL_DEXTRA; }
    const char *GetProtocolName(void) const     { return "DExtra"; }
    
    // revision helper
    static int GetProtocolRevision(const CVersion &);
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cdextrapeer_h */
