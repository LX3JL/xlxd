//
//  cysfclient.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 20/05/2018.
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

#ifndef cysfclient_h
#define cysfclient_h

#include "cclient.h"

////////////////////////////////////////////////////////////////////////////////////////
// define


////////////////////////////////////////////////////////////////////////////////////////
// class

class CYsfClient : public CClient
{
public:
    // constructors
    CYsfClient();
    CYsfClient(const CCallsign &, const CIp &, char = ' ');
    CYsfClient(const CYsfClient &);
    
    // destructor
    virtual ~CYsfClient() {};
    
    // identity
    int GetProtocol(void) const                 { return PROTOCOL_YSF; }
    const char *GetProtocolName(void) const     { return "YSF"; }
    int GetCodec(void) const                    { return CODEC_AMBE2PLUS; }
    bool IsNode(void) const                     { return true; }
    
    // status
    bool IsAlive(void) const;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cysfclient_h */
