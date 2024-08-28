//
//  cimrsclient.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 29/10/2019.
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

#ifndef cimrsclient_h
#define cimrsclient_h

#include "cclient.h"

////////////////////////////////////////////////////////////////////////////////////////
// define


////////////////////////////////////////////////////////////////////////////////////////
// class

class CImrsClient : public CClient
{
public:
    // constructors
    CImrsClient();
    CImrsClient(const CCallsign &, const CIp &, char = ' ');
    CImrsClient(const CImrsClient &);
    
    // destructor
    virtual ~CImrsClient() {};
    
    // identity
    int GetProtocol(void) const                 { return PROTOCOL_IMRS; }
    const char *GetProtocolName(void) const     { return "IMRS"; }
    int GetCodec(void) const                    { return CODEC_AMBE2PLUS; }
    bool IsNode(void) const                     { return true; }
    
    // status
    bool IsAlive(void) const;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cimrsclient_h */
