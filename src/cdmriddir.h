//
//  cdmriddir.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 08/10/2016.
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

#ifndef cdmriddir_h
#define cdmriddir_h

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "cbuffer.h"
#include "ccallsign.h"


// compare function for std::map::find

struct CallsignCompare
{
    bool operator() (const CCallsign &cs1, const CCallsign &cs2) const
    { return cs1.HasLowerCallsign(cs2);}
};


////////////////////////////////////////////////////////////////////////////////////////

class CDmridDir
{
public:
    // constructor
    CDmridDir() {}
    
    // destructor
    ~CDmridDir() {}
    
    // refresh
    bool RefreshContent(void);
    
    // find
    const CCallsign *FindCallsign(uint32);
    uint32 FindDmrid(const CCallsign &);
    
protected:
    // httpd helpers
    bool HttpGet(const char *, const char *, int, CBuffer *);
    
    // syntax helpers
    bool IsValidDmrid(const char *);
    
protected:
    // directory
    std::map <uint32, CCallsign> m_CallsignMap;
    std::map <CCallsign, uint32, CallsignCompare> m_DmridMap;
};

////////////////////////////////////////////////////////////////////////////////////////

#endif /* cdmriddir_h */
