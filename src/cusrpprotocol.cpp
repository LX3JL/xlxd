//
//  cusrpprotocol.cpp
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

#include "main.h"
#include <string.h>
#include <fstream>
#include <sstream>
#include "cusrpprotocol.h"
#include "creflector.h"
#include "ctranscoder.h"

////////////////////////////////////////////////////////////////////////////////////////
// helper

// strict dotted IPv4 + port, no DNS
static bool MakeIp(const std::string &addr, int port, CIp *ip)
{
    struct sockaddr_in sa;
    ::memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons((uint16)port);
    if ( (port < 1) || (port > 65535) || (inet_pton(AF_INET, addr.c_str(), &sa.sin_addr) != 1) )
    {
        return false;
    }
    ip->SetSockAddr(&sa);
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
// destructor

CUsrpProtocol::~CUsrpProtocol()
{
    Close();
}

////////////////////////////////////////////////////////////////////////////////////////
// operation

bool CUsrpProtocol::Init(void)
{
    // endpoints first: the threads started by the base class use them
    ReadOptions();
    if ( !m_Endpoints.empty() )
    {
        g_Transcoder.SetPcmWanted();
    }

    // base class (threads also drain the queue when no endpoint is configured)
    return CProtocol::Init();
}

void CUsrpProtocol::Close(void)
{
    // base class
    CProtocol::Close();

    // threads are gone
    for ( size_t i = 0; i < m_Endpoints.size(); i++ )
    {
        delete m_Endpoints[i];
    }
    m_Endpoints.clear();
}

void CUsrpProtocol::ReadOptions(void)
{
    std::ifstream file(USRPOPTIONS_PATH);
    std::string line;
    int n = 0;

    // missing or empty file: feature off, no socket
    while ( std::getline(file, line) )
    {
        n++;
        std::istringstream row(line.substr(0, line.find('#')));
        std::string module, bindIp, peerIp, callsign, extra;
        int bindPort = 0, peerPort = 0;
        if ( !(row >> module) )
        {
            continue;
        }

        // module bind_ip bind_port peer_ip peer_port callsign
        CUsrpEndpoint *ep = new CUsrpEndpoint;
        bool ok = (row >> bindIp >> bindPort >> peerIp >> peerPort >> callsign) && !(row >> extra) &&
                  (module.size() == 1) && g_Reflector.IsValidModule(module[0]) &&
                  MakeIp(bindIp, bindPort, &ep->m_Bind) && MakeIp(peerIp, peerPort, &ep->m_Peer) &&
                  (ep->m_Peer.GetAddr() != 0) && !(ep->m_Bind == ep->m_Peer) &&
                  (callsign.size() <= CALLSIGN_LEN);
        if ( ok )
        {
            ep->m_cModule = module[0];
            ep->m_Callsign.SetCallsign(callsign.c_str());
            ok = ep->m_Callsign.IsValid();
        }
        // one endpoint per module and per peer, no bind clash (0.0.0.0 clashes with all)
        for ( size_t i = 0; ok && (i < m_Endpoints.size()); i++ )
        {
            const CUsrpEndpoint *old = m_Endpoints[i];
            ok = (old->m_cModule != ep->m_cModule) && !(old->m_Peer == ep->m_Peer) &&
                 !((old->m_Bind.GetPort() == ep->m_Bind.GetPort()) &&
                   ((old->m_Bind.GetAddr() == ep->m_Bind.GetAddr()) || (old->m_Bind.GetAddr() == 0) || (ep->m_Bind.GetAddr() == 0)));
        }
        if ( ok && !ep->m_Socket.Open((uint16)bindPort, bindIp.c_str()) )
        {
            std::cout << "USRP: cannot bind " << bindIp << ":" << bindPort << std::endl;
            ok = false;
        }
        if ( !ok )
        {
            std::cout << "USRP: ignoring invalid line " << n << " in " << USRPOPTIONS_PATH << std::endl;
            delete ep;
            continue;
        }

        m_Endpoints.push_back(ep);

        // permanent client, owner of the streams coming from this peer
        CClients *clients = g_Reflector.GetClients();
        clients->AddClient(new CUsrpClient(ep->m_Callsign, ep->m_Peer, ep->m_cModule));
        g_Reflector.ReleaseClients();
    }
    if ( !m_Endpoints.empty() )
    {
        std::cout << "USRP: " << m_Endpoints.size() << " endpoint(s) loaded from " << USRPOPTIONS_PATH << std::endl;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// task

void CUsrpProtocol::RxTask(void)
{
    CBuffer     Buffer;
    CIp         Ip;
    bool        keyed;

    if ( m_Endpoints.empty() )
    {
        CTimePoint::TaskSleepFor(20);
        return;
    }

    // wait on all endpoint sockets at once
    fd_set FdSet;
    int fdMax = -1;
    FD_ZERO(&FdSet);
    for ( size_t i = 0; i < m_Endpoints.size(); i++ )
    {
        FD_SET(m_Endpoints[i]->m_Socket.GetSocket(), &FdSet);
        fdMax = MAX(fdMax, m_Endpoints[i]->m_Socket.GetSocket());
    }
    struct timeval tv = { 0, 20000 };
    select(fdMax + 1, &FdSet, 0, 0, &tv);

    for ( size_t i = 0; i < m_Endpoints.size(); i++ )
    {
        CUsrpEndpoint *ep = m_Endpoints[i];
        while ( ep->m_Socket.Receive(&Buffer, &Ip, 0) != -1 )
        {
            // only the configured peer (address and port) feeds this module
            if ( (Ip == ep->m_Peer) && IsValidUsrpPacket(Buffer, &keyed) )
            {
                if ( keyed )
                {
                    OnVoiceIn(ep, &(Buffer.data()[USRP_HEADER_SIZE]));
                }
                else
                {
                    EndRxStream(ep);
                }
            }
            else if ( !ep->m_bRejectLogged )
            {
                // once per endpoint: voice only, other frame types (TLV, DTMF, text) are dropped
                std::cout << "USRP: module " << ep->m_cModule << " dropped unsupported packet (" << Buffer.size()
                          << " bytes) from " << Ip << ":" << ntohs(Ip.GetPort()) << ", further ones not logged" << std::endl;
                ep->m_bRejectLogged = true;
            }
        }

        // no voice for a while ends the stream like an unkey
        if ( ep->m_bRxOpen && (ep->m_LastRxTime.DurationSinceNow() >= (USRP_RX_TIMEOUT/1000.0)) )
        {
            EndRxStream(ep);
        }
    }

    // handle end of streaming timeout
    CheckStreamsTimeout();
}

void CUsrpProtocol::TxTask(void)
{
    CProtocol::TxTask();

    // stream ended without a last frame (or ran dry): unkey the radio
    for ( size_t i = 0; i < m_Endpoints.size(); i++ )
    {
        CUsrpEndpoint *ep = m_Endpoints[i];
        if ( ep->m_bTxKeyed && (ep->m_LastTxTime.DurationSinceNow() >= (USRP_TX_TIMEOUT/1000.0)) )
        {
            SendUnkey(ep);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// streams helpers

void CUsrpProtocol::OnVoiceIn(CUsrpEndpoint *ep, const uint8 *pcm)
{
    ep->m_LastRxTime.Now();

    // first keyed frame opens a stream with a fresh id
    if ( !ep->m_bRxOpen )
    {
        ep->m_uiRxStreamId = AllocateGlobalStreamIdNonce();
        ep->m_bRxOpen = true;
        ep->m_uiRxPid = 0;

        CCallsign rpt1 = ep->m_Callsign;
        rpt1.SetModule('B');
        CCallsign rpt2 = m_ReflectorCallsign;
        rpt2.SetModule(ep->m_cModule);
        CDvHeaderPacket *header = new CDvHeaderPacket(ep->m_Callsign, CCallsign("CQCQCQ"), rpt1, rpt2, ep->m_uiRxStreamId, 0);
        header->SetMySuffix("USRP");
        OnDvHeaderPacketIn(header, ep->m_Peer);
    }
    OnDvFramePacketIn(new CDvFramePacket(ep->m_uiRxStreamId, (uint8)(ep->m_uiRxPid++ % 21), pcm), &ep->m_Peer);
}

void CUsrpProtocol::EndRxStream(CUsrpEndpoint *ep)
{
    static const uint8 silence[PCM_FRAME_SIZE] = { 0 };

    if ( ep->m_bRxOpen )
    {
        // m_uiRxStreamId stays: its tail may still reach the TX side
        OnDvLastFramePacketIn(new CDvLastFramePacket(ep->m_uiRxStreamId, (uint8)(ep->m_uiRxPid++ % 21), silence), &ep->m_Peer);
        ep->m_bRxOpen = false;
    }
}

bool CUsrpProtocol::OnDvHeaderPacketIn(CDvHeaderPacket *Header, const CIp &Ip)
{
    bool newstream = false;
    CCallsign via;
    CCallsign myCallsign = Header->GetMyCallsign();
    CCallsign rpt2Callsign = Header->GetRpt2Callsign();

    // find this client
    CClients *clients = g_Reflector.GetClients();
    CClient *client = clients->FindClient(Ip, PROTOCOL_USRP);
    if ( client != NULL )
    {
        via = client->GetCallsign();

        // try to open the stream; a busy module keeps its first talker
        CPacketStream *stream = g_Reflector.OpenStream(Header, client);
        if ( stream != NULL )
        {
            // keep the handle - Header ownership transferred to stream
            m_Streams.push_back(stream);
            newstream = true;
            Header = NULL;
        }
        else if ( g_Reflector.TryLateEntry(Header, client) )
        {
            Header = NULL;
        }
    }
    g_Reflector.ReleaseClients();

    // update last heard
    if ( newstream )
    {
        g_Reflector.GetUsers()->Hearing(myCallsign, via, rpt2Callsign);
        g_Reflector.ReleaseUsers();
    }

    // delete header if not used
    if ( Header != NULL )
    {
        delete Header;
    }
    return newstream;
}

////////////////////////////////////////////////////////////////////////////////////////
// queue helper

void CUsrpProtocol::HandleQueue(void)
{
    CBuffer buffer;

    // drain queue into local vector
    std::vector<CPacket *> packets;
    m_Queue.Lock();
    while ( !m_Queue.empty() )
    {
        packets.push_back(m_Queue.front());
        m_Queue.pop();
    }
    m_Queue.Unlock();

    for ( size_t pi = 0; pi < packets.size(); pi++ )
    {
        CPacket *packet = packets[pi];

        // the endpoint of this module, if any
        for ( size_t i = 0; i < m_Endpoints.size(); i++ )
        {
            CUsrpEndpoint *ep = m_Endpoints[i];
            if ( ep->m_cModule != packet->GetModuleId() )
            {
                continue;
            }

            // never echo a stream back to the USRP peer it came from
            if ( packet->GetStreamId() != ep->m_uiTxStreamId )
            {
                ep->m_uiTxStreamId = packet->GetStreamId();
                ep->m_bTxEcho = (ep->m_uiTxStreamId == ep->m_uiRxStreamId);
            }
            if ( ep->m_bTxEcho )
            {
                continue;
            }

            if ( packet->IsLastPacket() )
            {
                if ( ep->m_bTxKeyed )
                {
                    SendUnkey(ep);
                }
            }
            else if ( packet->IsDvFrame() && ((const CDvFramePacket *)packet)->HasPcmData() )
            {
                EncodeUsrpPacket(&buffer, ep->m_uiTxSequence++, ((const CDvFramePacket *)packet)->GetPcm());
                ep->m_Socket.SendVoice(buffer, ep->m_Peer);
                ep->m_bTxKeyed = true;
                ep->m_LastTxTime.Now();
            }
        }

        // done
        delete packet;
    }
}

void CUsrpProtocol::SendUnkey(CUsrpEndpoint *ep)
{
    CBuffer buffer;

    EncodeUsrpPacket(&buffer, ep->m_uiTxSequence++, NULL);
    ep->m_Socket.SendVoice(buffer, ep->m_Peer);
    ep->m_bTxKeyed = false;
}

////////////////////////////////////////////////////////////////////////////////////////
// packet decoding helpers

bool CUsrpProtocol::IsValidUsrpPacket(const CBuffer &Buffer, bool *Keyed)
{
    uint8 tag[] = { 'U','S','R','P' };

    // "USRP" + BE32 seq, memory, keyup, talkgroup, type, mpxid, reserved.
    // Voice only (type 0): keyed = 352 bytes with PCM, unkey = 32 bytes.
    if ( ((Buffer.size() != USRP_HEADER_SIZE) && (Buffer.size() != USRP_VOICE_SIZE)) ||
         (Buffer.Compare(tag, sizeof(tag)) != 0) )
    {
        return false;
    }
    for ( int i = 8; i < USRP_HEADER_SIZE; i++ )
    {
        // all fields but seq are zero, keyup low byte is 0 or 1
        if ( (Buffer.data()[i] != 0) && (i != 15) )
        {
            return false;
        }
    }
    *Keyed = (Buffer.data()[15] == 1);
    return (Buffer.data()[15] <= 1) && (*Keyed == (Buffer.size() == USRP_VOICE_SIZE));
}

////////////////////////////////////////////////////////////////////////////////////////
// packet encoding helpers

void CUsrpProtocol::EncodeUsrpPacket(CBuffer *Buffer, uint32 uiSequence, const uint8 *Pcm)
{
    uint8 tag[] = { 'U','S','R','P' };

    // Pcm == NULL encodes the unkey frame
    Buffer->Set(tag, sizeof(tag));
    Buffer->Append((uint32)htonl(uiSequence));
    Buffer->Append((uint8)0, 4);                                // memory
    Buffer->Append((uint32)htonl((Pcm != NULL) ? 1 : 0));       // keyup
    Buffer->Append((uint8)0, 16);                               // talkgroup, type (voice), mpxid, reserved
    if ( Pcm != NULL )
    {
        Buffer->Append((uint8 *)Pcm, PCM_FRAME_SIZE);
    }
}
