//
//  cuser.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 13/11/2015.
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

#ifndef cuser_h
#define cuser_h

#include "ccallsign.h"
#include "cbuffer.h"

////////////////////////////////////////////////////////////////////////////////////////

class CUser
{
public:
    // constructor
    CUser();
    CUser(const CCallsign &, const CCallsign &, const CCallsign &, const CCallsign &);
    CUser(const CUser &);
    
    // destructor
    ~CUser() {}
    
    // operation
    void HeardNow(void)     { m_LastHeardTime = std::time(NULL); }

    // operators
    bool operator ==(const CUser &) const;
    bool operator <(const CUser &) const;

    // reporting
    void WriteXml(std::ofstream &);
    void GetJsonObject(char *);

protected:
    // data
    CCallsign   m_My;
    CCallsign   m_Rpt1;
    CCallsign   m_Rpt2;
    CCallsign   m_Xlx;
    std::time_t m_LastHeardTime;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cuser_h */
