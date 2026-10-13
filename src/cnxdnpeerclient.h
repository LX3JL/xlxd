//
//  cnxdnpeerclient.h
//  xlxd
//
//  Created for NXDN Reflector peering support
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

#ifndef cnxdnpeerclient_h
#define cnxdnpeerclient_h

#include "cclient.h"

////////////////////////////////////////////////////////////////////////////////////////
// class

class CNxdnPeerClient : public CClient
{
public:
    // constructors
    CNxdnPeerClient();
    CNxdnPeerClient(const CCallsign &, const CIp &, char = ' ');
    CNxdnPeerClient(const CNxdnPeerClient &);

    // destructor
    virtual ~CNxdnPeerClient() {};

    // identity
    int GetProtocol(void) const                 { return PROTOCOL_NXDN; }
    int GetProtocolRevision(void) const         { return 0; }
    const char *GetProtocolName(void) const     { return "NXDN"; }
    int GetCodec(void) const                    { return CODEC_AMBE2PLUS; }
    bool IsPeer(void) const                     { return true; }

    // status
    bool IsAlive(void) const;

    // reporting
    void WriteXml(std::ofstream &) {}
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cnxdnpeerclient_h */
