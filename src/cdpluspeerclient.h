//
//  cdpluspeerclient.h
//  xlxd
//
//  Created for DPlus/REF Reflector peering support
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

#ifndef cdpluspeerclient_h
#define cdpluspeerclient_h

#include "cclient.h"

////////////////////////////////////////////////////////////////////////////////////////
// class

class CDplusPeerClient : public CClient
{
public:
    // constructors
    CDplusPeerClient();
    CDplusPeerClient(const CCallsign &, const CIp &, char = ' ');
    CDplusPeerClient(const CDplusPeerClient &);

    // destructor
    virtual ~CDplusPeerClient() {};

    // identity
    int GetProtocol(void) const                 { return PROTOCOL_DPLUS; }
    int GetProtocolRevision(void) const         { return 0; }
    const char *GetProtocolName(void) const     { return "DPlus"; }
    int GetCodec(void) const                    { return CODEC_AMBEPLUS; }
    bool IsPeer(void) const                     { return m_bIsPeer; }
    void SetPeer(bool b)                        { m_bIsPeer = b; }

    // status
    bool IsAlive(void) const;

    // reporting
    void WriteXml(std::ofstream &) {}

protected:
    bool m_bIsPeer;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cdpluspeerclient_h */
