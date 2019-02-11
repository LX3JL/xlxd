//
//  cxlxclient.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 28/01/2016.
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

#ifndef cxlxclient_h
#define cxlxclient_h


#include "cclient.h"

////////////////////////////////////////////////////////////////////////////////////////
// define

#define XLX_PROTOCOL_REVISION_0      0       // AMBE only, original connect mechanism
#define XLX_PROTOCOL_REVISION_1      1       // AMBE only, revised connect mechanism
#define XLX_PROTOCOL_REVISION_2      2       // Transcoded AMBE+AMBE2 interlink (use AMBE only)
#define XLX_PROTOCOL_REVISION_3      3       // Transcoded AMBE+AMBE2+Codec 2 interlink


////////////////////////////////////////////////////////////////////////////////////////
// class

class CXlxClient : public CClient
{
public:
    // constructors
    CXlxClient();
    CXlxClient(const CCallsign &, const CIp &, char = ' ', int = XLX_PROTOCOL_REVISION_0);
    CXlxClient(const CXlxClient &);
    
    // destructor
    virtual ~CXlxClient() {};
    
    // identity
    int GetProtocol(void) const                 { return PROTOCOL_XLX; }
    int GetProtocolRevision(void) const         { return m_ProtRev; }
    const char *GetProtocolName(void) const     { return "XLX"; }
    int GetCodec(void) const;
    bool IsPeer(void) const                     { return true; }
    
    // status
    bool IsAlive(void) const;

    // reporting
    void WriteXml(std::ofstream &) {}
    
protected:
    // data
    int     m_ProtRev;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cxlxclient_h */
