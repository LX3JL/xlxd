//
//  cwiresxcmd.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 09/10/2019.
//  Copyright © 2019 Jean-Luc Deltombe (LX3JL). All rights reserved.
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


#ifndef cwiresxcmd_h
#define cwiresxcmd_h

#include "ccallsign.h"
#include "cip.h"
#include "ctimepoint.h"

////////////////////////////////////////////////////////////////////////////////////////
// defines

// Wires-X commands
#define WIRESX_CMD_UNKNOWN          0
#define WIRESX_CMD_DX_REQ           1
#define WIRESX_CMD_ALL_REQ          2
#define WIRESX_CMD_SEARCH_REQ       3
#define WIRESX_CMD_CONN_REQ         4
#define WIRESX_CMD_DISC_REQ         5

////////////////////////////////////////////////////////////////////////////////////////
// class

class CWiresxCmd
{
public:
    // constructor
    CWiresxCmd();
    CWiresxCmd(const CIp &, const CCallsign &, int, int);
    CWiresxCmd(const CWiresxCmd &);
    
    // destructor
    virtual ~CWiresxCmd() {}
    
    // get
    const CCallsign &GetCallsign(void) const        { return m_Callsign; }
    const CIp &GetIp(void) const                    { return m_Ip; }
    int   GetCmd(void) const                        { return m_iCmd; }
    int   GetArg(void) const                        { return m_iArg; }
    const CTimePoint &GetTime(void) const           { return m_Time; }
    
protected:
    // data
    CIp         m_Ip;
    CCallsign   m_Callsign;
    int         m_iCmd;
    int         m_iArg;
    CTimePoint  m_Time;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cwiresxcmd_h */
