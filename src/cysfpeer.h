//
//  cysfpeer.h
//  xlxd
//
//  Created for YSF Reflector peering support
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

#ifndef cysfpeer_h
#define cysfpeer_h

#include "cpeer.h"
#include "cysfpeerclient.h"

////////////////////////////////////////////////////////////////////////////////////////
// class

class CYsfPeer : public CPeer
{
public:
    // constructors
    CYsfPeer();
    CYsfPeer(const CCallsign &, const CIp &, char *, const CVersion &);
    CYsfPeer(const CYsfPeer &);

    // destructor
    ~CYsfPeer();

    // status
    bool IsAlive(void) const;
    void Alive(void);

    // identity
    int GetProtocol(void) const                 { return PROTOCOL_YSF; }
    const char *GetProtocolName(void) const     { return "YSF"; }

    // get
    uint32_t GetYsfId(void) const               { return m_uiYsfId; }

    // set
    void SetYsfId(uint32_t id)                  { m_uiYsfId = id; }

protected:
    // YSF reflector ID (5 digits, 1-99999)
    uint32_t m_uiYsfId;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cysfpeer_h */
