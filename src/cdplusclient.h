//
//  cdplusclient.h
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

#ifndef cdplusclient_h
#define cdplusclient_h

#include "cclient.h"

////////////////////////////////////////////////////////////////////////////////////////
// define


////////////////////////////////////////////////////////////////////////////////////////
// class

class CDplusClient : public CClient
{
public:
    // constructors
    CDplusClient();
    CDplusClient(const CCallsign &, const CIp &, char = ' ');
    CDplusClient(const CDplusClient &);
    
    // destructor
    virtual ~CDplusClient() {};
    
    // identity
    int GetProtocol(void) const                 { return PROTOCOL_DPLUS; }
    const char *GetProtocolName(void) const     { return "DPlus"; }
    bool IsNode(void) const                     { return true; }
    bool IsDextraDongle(void) const             { return m_bDextraDongle; }
    void SetDextraDongle(void)                  { m_bDextraDongle = true; }
    
    // status
    bool IsAlive(void) const;
    void SetMasterOfModule(char);
    
protected:
    // data
    bool m_bDextraDongle;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cdplusclient_h */
