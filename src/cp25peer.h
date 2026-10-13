//
//  cp25peer.h
//  xlxd
//
//  Created for P25 Reflector peering support
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

#ifndef cp25peer_h
#define cp25peer_h

#include "cpeer.h"
#include "cp25peerclient.h"

////////////////////////////////////////////////////////////////////////////////////////
// class

class CP25Peer : public CPeer
{
public:
    // constructors
    CP25Peer();
    CP25Peer(const CCallsign &, const CIp &, char *, const CVersion &);
    CP25Peer(const CP25Peer &);

    // destructor
    ~CP25Peer();

    // status
    bool IsAlive(void) const;
    void Alive(void);

    // identity
    int GetProtocol(void) const                 { return PROTOCOL_P25; }
    const char *GetProtocolName(void) const     { return "P25"; }

    // get
    uint32_t GetP25Id(void) const               { return m_uiP25Id; }
    uint32_t GetP25Tg(void) const               { return m_uiP25Tg; }

    // set
    void SetP25Id(uint32_t id)                  { m_uiP25Id = id; }
    void SetP25Tg(uint32_t tg)                  { m_uiP25Tg = tg; }

protected:
    // P25 Subscriber ID (24-bit, 1-16777215)
    uint32_t m_uiP25Id;
    // P25 Talk Group (24-bit, 1-16777215)
    uint32_t m_uiP25Tg;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cp25peer_h */
