//
//  cusrpprotocol.h
//  xlxd
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

#ifndef cusrpprotocol_h
#define cusrpprotocol_h

#include "cprotocol.h"
#include "cclient.h"

////////////////////////////////////////////////////////////////////////////////////////

// note on USRP:
//
//  Each line of USRPOPTIONS_PATH binds one reflector module to one
//  app_rpt chan_usrp channel:  module bind_ip bind_port peer_ip peer_port callsign
//  Voice is raw PCM (S16LE, 8 kHz, 20 ms per frame); the transcoder converts
//  it with the extended (PCM capable) open. There is no login or keepalive:
//  the endpoint is a permanent client that only accepts its configured peer.

////////////////////////////////////////////////////////////////////////////////////////
// class

class CUsrpClient : public CClient
{
public:
    // constructor
    CUsrpClient(const CCallsign &callsign, const CIp &ip, char module) : CClient(callsign, ip, module) {}

    // identity
    int GetProtocol(void) const                 { return PROTOCOL_USRP; }
    const char *GetProtocolName(void) const     { return "USRP"; }
    int GetCodec(void) const                    { return CODEC_PCM_S16LE_8K; }
    bool IsNode(void) const                     { return true; }

    // status: permanent while configured
    bool IsAlive(void) const                    { return true; }
};

class CUsrpEndpoint
{
public:
    // constructor
    CUsrpEndpoint() : m_cModule(' '), m_bRejectLogged(false), m_uiRxStreamId(0), m_bRxOpen(false), m_uiRxPid(0),
                      m_uiTxStreamId(0), m_bTxEcho(false), m_bTxKeyed(false), m_uiTxSequence(0) {}

    // config
    char        m_cModule;
    CIp         m_Bind;
    CIp         m_Peer;
    CCallsign   m_Callsign;
    CUdpSocket  m_Socket;
    bool        m_bRejectLogged;

    // rx side (RX thread); the id of the last stream from the peer is kept
    // after it ends and read by the TX thread for echo suppression
    std::atomic<uint16> m_uiRxStreamId;
    bool        m_bRxOpen;
    uint32      m_uiRxPid;
    CTimePoint  m_LastRxTime;

    // tx side (TX thread)
    uint16      m_uiTxStreamId;
    bool        m_bTxEcho;
    bool        m_bTxKeyed;
    uint32      m_uiTxSequence;
    CTimePoint  m_LastTxTime;
};

class CUsrpProtocol : public CProtocol
{
public:
    // destructor
    virtual ~CUsrpProtocol();

    // initialization
    bool Init(void);
    void Close(void);

    // task (split-thread mode)
    bool UsesSplitThreads(void) const           { return true; }
    void RxTask(void);
    void TxTask(void);

protected:
    // config
    void ReadOptions(void);

    // stream helpers
    void OnVoiceIn(CUsrpEndpoint *, const uint8 *);
    void EndRxStream(CUsrpEndpoint *);
    bool OnDvHeaderPacketIn(CDvHeaderPacket *, const CIp &);

    // queue helper
    void HandleQueue(void);
    void SendUnkey(CUsrpEndpoint *);

    // packet decoding helpers
    bool IsValidUsrpPacket(const CBuffer &, bool *);

    // packet encoding helpers
    void EncodeUsrpPacket(CBuffer *, uint32, const uint8 *);

protected:
    // endpoints, fixed after Init() until Close()
    std::vector<CUsrpEndpoint *> m_Endpoints;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cusrpprotocol_h */
