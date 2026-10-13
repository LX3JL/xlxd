//
//  cdpluspeer.h
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

#ifndef cdpluspeer_h
#define cdpluspeer_h

#include "cpeer.h"
#include "cdpluspeerclient.h"

////////////////////////////////////////////////////////////////////////////////////////
// define

// Connection states for DPlus peer
#define DPLUS_PEER_STATE_DISCONNECTED   0
#define DPLUS_PEER_STATE_CONNECTING     1   // Sent connect packet, waiting for echo
#define DPLUS_PEER_STATE_LOGGING_IN     2   // Sent login packet, waiting for ack
#define DPLUS_PEER_STATE_CONNECTED      3   // Fully connected

////////////////////////////////////////////////////////////////////////////////////////
// class

class CDplusPeer : public CPeer
{
public:
    // constructors
    CDplusPeer();
    CDplusPeer(const CCallsign &, const CIp &, char *, const CVersion &);
    CDplusPeer(const CDplusPeer &);

    // destructor
    ~CDplusPeer();

    // status
    bool IsAlive(void) const;
    void Alive(void);

    // identity
    int GetProtocol(void) const                 { return PROTOCOL_DPLUS; }
    const char *GetProtocolName(void) const     { return "DPlus"; }

    // connection state
    int GetConnectionState(void) const          { return m_iConnectionState; }
    void SetConnectionState(int state)          { m_iConnectionState = state; }
    bool IsConnected(void) const                { return m_iConnectionState == DPLUS_PEER_STATE_CONNECTED; }
    bool IsConnecting(void) const               { return m_iConnectionState == DPLUS_PEER_STATE_CONNECTING ||
                                                         m_iConnectionState == DPLUS_PEER_STATE_LOGGING_IN; }

    // module mapping (local module on XLX, remote module on REF)
    char GetLocalModule(void) const             { return m_cLocalModule; }
    char GetRemoteModule(void) const            { return m_cRemoteModule; }
    void SetLocalModule(char c)                 { m_cLocalModule = c; }
    void SetRemoteModule(char c)                { m_cRemoteModule = c; }

    // port
    uint16 GetPort(void) const                  { return m_uiPort; }
    void SetPort(uint16 port)                   { m_uiPort = port; }

    // connection timing
    void ResetConnectTimer(void)                { m_ConnectTime.Now(); }
    double GetConnectDuration(void) const       { return m_ConnectTime.DurationSinceNow(); }

protected:
    // connection state machine
    int         m_iConnectionState;

    // module mapping
    char        m_cLocalModule;     // Module on this XLX reflector
    char        m_cRemoteModule;    // Module on remote REF reflector

    // port (may be non-default)
    uint16      m_uiPort;

    // connection timing
    CTimePoint  m_ConnectTime;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cdpluspeer_h */
