//
//  cg3client.h
//  xlxd
//
//  Created by Marius Petrescu (YO2LOJ) on 03/06/2019.
//  Copyright © 2019 Marius Petrescu (YO2LOJ). All rights reserved.
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

#ifndef cg3client_h
#define cg3client_h

#include "cclient.h"

////////////////////////////////////////////////////////////////////////////////////////
// define


////////////////////////////////////////////////////////////////////////////////////////
// class

class CG3Client : public CClient
{
public:
    // constructors
    CG3Client();
    CG3Client(const CCallsign &, const CIp &, char = ' ');
    CG3Client(const CG3Client &);
    
    // destructor
    virtual ~CG3Client() {};
    
    // identity
    int GetProtocol(void) const                 { return PROTOCOL_G3; }
    const char *GetProtocolName(void) const     { return "Terminal/AP"; }
    int GetCodec(void) const                    { return CODEC_AMBEPLUS; }
    bool IsNode(void) const                     { return true; }
    
    // status
    bool IsAlive(void) const;

protected:
    // data
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cg3client_h */
