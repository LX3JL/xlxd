//
//  cm17client.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 28/11/2025.
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

#ifndef cm17client_h
#define cm17client_h

#include "cclient.h"

////////////////////////////////////////////////////////////////////////////////////////
// class

class CM17Client : public CClient
{
public:
    // constructors
    CM17Client();
    CM17Client(const CCallsign &, const CIp &, char = ' ', bool = false);
    CM17Client(const CM17Client &);

    // destructor
    virtual ~CM17Client() {};

    // identity
    int GetProtocol(void) const                 { return PROTOCOL_M17; }
    const char *GetProtocolName(void) const     { return "M17"; }
    int GetCodec(void) const                    { return CODEC_CODEC2; }
    bool IsNode(void) const                     { return true; }

    // listen-only status
    bool IsListenOnly(void) const               { return m_bListenOnly; }
    void SetListenOnly(bool b)                  { m_bListenOnly = b; }

    // status
    bool IsAlive(void) const;

protected:
    bool m_bListenOnly;
};

////////////////////////////////////////////////////////////////////////////////////////

#endif /* cm17client_h */
