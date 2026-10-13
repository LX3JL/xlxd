//
//  cxlxdmrclient.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 20/01/2017.
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

#ifndef cxlxdmrclient_h
#define cxlxdmrclient_h


#include "cclient.h"
#include "cxlxclient.h"

////////////////////////////////////////////////////////////////////////////////////////
// define


////////////////////////////////////////////////////////////////////////////////////////
// class

class CXlxDmrClient : public CClient
{
public:
    // constructors
    CXlxDmrClient();
    CXlxDmrClient(const CCallsign &, const CIp &, char = ' ');
    CXlxDmrClient(const CXlxDmrClient &);

    // destructor
    virtual ~CXlxDmrClient() {};

    // identity
    int GetProtocol(void) const                 { return PROTOCOL_XLX; }
    int GetProtocolRevision(void) const         { return XLX_PROTOCOL_REVISION_2; }
    const char *GetProtocolName(void) const     { return "XLX"; }
    int GetCodec(void) const                    { return CODEC_AMBE2PLUS; }
    bool IsPeer(void) const                     { return true; }

    // status
    bool IsAlive(void) const;

    // reporting
    void WriteXml(std::ofstream &) {}
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cxlxdmrclient_h */
