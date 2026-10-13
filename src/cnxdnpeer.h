//
//  cnxdnpeer.h
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

#ifndef cnxdnpeer_h
#define cnxdnpeer_h

#include "cpeer.h"
#include "cnxdnpeerclient.h"

////////////////////////////////////////////////////////////////////////////////////////
// class

class CNxdnPeer : public CPeer
{
public:
    // constructors
    CNxdnPeer();
    CNxdnPeer(const CCallsign &, const CIp &, char *, const CVersion &);
    CNxdnPeer(const CNxdnPeer &);

    // destructor
    ~CNxdnPeer();

    // status
    bool IsAlive(void) const;
    void Alive(void);

    // identity
    int GetProtocol(void) const                 { return PROTOCOL_NXDN; }
    const char *GetProtocolName(void) const     { return "NXDN"; }

    // get
    uint16_t GetNxdnId(void) const              { return m_uiNxdnId; }

    // set
    void SetNxdnId(uint16_t id)                 { m_uiNxdnId = id; }

protected:
    // NXDN Talk Group ID (16-bit, 1-65535)
    uint16_t m_uiNxdnId;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cnxdnpeer_h */
