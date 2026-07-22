//
//  cdplusprotocol.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 01/11/2015.
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

#include "main.h"
#include <string.h>
#include "cclient.h"
#include "cdplusclient.h"
#include "cdplusprotocol.h"
#include "creflector.h"
#include "cgatekeeper.h"

// periodic cleanup interval for stale reject trackers
#define CLEANUP_CHECK_INTERVAL  5.0     // seconds between cleanup sweeps

////////////////////////////////////////////////////////////////////////////////////////
// operation

bool CDplusProtocol::Init(void)
{
    bool ok;
    
    // base class
    ok = CProtocol::Init();
    
    // update the reflector callsign
    m_ReflectorCallsign.PatchCallsign(0, (const uint8 *)"REF", 3);
    
    // create our socket
    ok &= m_Socket.Open(DPLUS_PORT);
    if ( !ok )
    {
        std::cout << "Error opening socket on port UDP" << DPLUS_PORT << " on ip " << g_Reflector.GetListenIp() << std::endl;
    }
    
    // update time
    m_LastKeepaliveTime.Now();
    m_LastDplusPeerLinkTime.Now();
    m_LastDplusPeerKeepaliveTime.Now();

    // done
    return ok;
}



////////////////////////////////////////////////////////////////////////////////////////
// task

void CDplusProtocol::RxTask(void)
{
    CBuffer             Buffer;
    CIp                 Ip;
    CCallsign           Callsign;
    CDvHeaderPacket     *Header;
    CDvFramePacket      *Frame;
    CDvLastFramePacket  *LastFrame;
    
    // handle incoming packets
    if ( m_Socket.Receive(&Buffer, &Ip, 20) != -1 )
    {
        // crack the packet
        if ( (Frame = IsValidDvFramePacket(Buffer)) != NULL )
        {
            if ( IsEchoOfOutboundStream(Frame->GetStreamId(), Ip) )
                delete Frame;
            else
                OnDvFramePacketIn(Frame, &Ip);
        }
        else if ( (Header = IsValidDvHeaderPacket(Buffer)) != NULL )
        {
            if ( IsEchoOfOutboundStream(Header->GetStreamId(), Ip) )
            {
                delete Header;
            }
            else
            {
                // Skip gatekeeper for peer connections (we initiated these)
                bool isPeer = false;
                {
                    CPeers *peers = g_Reflector.GetPeers();
                    isPeer = (peers->FindPeer(Ip, PROTOCOL_DPLUS) != NULL);
                    g_Reflector.ReleasePeers();
                }

                // Peer traffic bypasses MayTransmit by design, but is
                // still subject to the per-callsign loop block — see
                // cysfprotocol.cpp for the rationale.
                if ( isPeer )
                {
                    if ( g_GateKeeper.IsCallsignLoopBlocked(
                            Header->GetMyCallsign(), "DPlus peer") )
                    {
                        delete Header;
                        Header = NULL;
                    }
                }
                else if ( !g_GateKeeper.MayTransmit(
                            Header->GetMyCallsign(), Ip,
                            PROTOCOL_DPLUS, Header->GetRpt2Module()) )
                {
                    delete Header;
                    Header = NULL;
                }

                if ( Header != NULL )
                {
                    OnDvHeaderPacketIn(Header, Ip);
                }
            }
        }
        else if ( (LastFrame = IsValidDvLastFramePacket(Buffer)) != NULL )
        {
            if ( IsEchoOfOutboundStream(LastFrame->GetStreamId(), Ip) )
                delete LastFrame;
            else
                OnDvLastFramePacketIn(LastFrame, &Ip);
        }
        else if ( IsValidConnectPacket(Buffer) )
        {
            // Check if this is an echo from a REF peer we're connecting to
            CPeers *peers = g_Reflector.GetPeers();
            CPeer *existingPeer = peers->FindPeer(Ip, PROTOCOL_DPLUS);

            if ( existingPeer != NULL )
            {
                CDplusPeer *dplusPeer = dynamic_cast<CDplusPeer *>(existingPeer);
                if ( dplusPeer != NULL && dplusPeer->GetConnectionState() == DPLUS_PEER_STATE_CONNECTING )
                {
                    // This is the connect echo - send login packet with local and remote modules
                    std::cout << "DPlus connect echo from peer " << dplusPeer->GetCallsign() << std::endl;
                    CBuffer loginBuffer;
                    EncodeLoginPacket(&loginBuffer, GetReflectorCallsign(),
                                      dplusPeer->GetLocalModule(), dplusPeer->GetRemoteModule());
                    m_Socket.Send(loginBuffer, Ip, dplusPeer->GetPort());
                    dplusPeer->SetConnectionState(DPLUS_PEER_STATE_LOGGING_IN);
                    dplusPeer->ResetConnectTimer();
                }
                g_Reflector.ReleasePeers();
            }
            else
            {
                g_Reflector.ReleasePeers();

                // This is a connect request from a client
                std::cout << "DPlus connect request packet from " << Ip << std::endl;

                // acknowledge the request
                m_Socket.Send(Buffer, Ip);
            }
        }
        else if ( IsValidLoginAckPacket(Buffer) )
        {
            // Check if this is an ack from a REF peer we're connecting to
            CPeers *peers = g_Reflector.GetPeers();
            CPeer *existingPeer = peers->FindPeer(Ip, PROTOCOL_DPLUS);

            if ( existingPeer != NULL )
            {
                CDplusPeer *dplusPeer = dynamic_cast<CDplusPeer *>(existingPeer);
                if ( dplusPeer != NULL && dplusPeer->GetConnectionState() == DPLUS_PEER_STATE_LOGGING_IN )
                {
                    std::cout << "DPlus login ack from peer " << dplusPeer->GetCallsign()
                              << " - sending link request for module " << dplusPeer->GetRemoteModule() << std::endl;

                    // Send link packet to request the specific module
                    // Format: 05 00 04 00 [module ASCII]
                    CBuffer linkBuffer;
                    EncodeLinkPacket(&linkBuffer, dplusPeer->GetRemoteModule());
                    m_Socket.Send(linkBuffer, Ip, dplusPeer->GetPort());

                    dplusPeer->SetConnectionState(DPLUS_PEER_STATE_CONNECTED);
                    dplusPeer->Alive();

                    // Add peer client to global clients list for voice routing
                    CClients *clients = g_Reflector.GetClients();
                    CIp peerClientIp = dplusPeer->GetIp();
                    peerClientIp.SetPort(htons(dplusPeer->GetPort()));
                    CDplusPeerClient *peerClient = new CDplusPeerClient(dplusPeer->GetCallsign(), peerClientIp, dplusPeer->GetLocalModule());
                    peerClient->SetPeer(true);
                    clients->AddClient(peerClient);
                    g_Reflector.ReleaseClients();

                    std::cout << "DPlus peer " << dplusPeer->GetCallsign()
                              << " connected (local " << dplusPeer->GetLocalModule()
                              << " -> remote " << dplusPeer->GetRemoteModule() << ")" << std::endl;

                    // Send a short silent transmission to establish the link on remote module
                    SendPeerLinkAnnouncement(dplusPeer);
                }
            }
            g_Reflector.ReleasePeers();
        }
        else if ( IsValidLoginNackPacket(Buffer) )
        {
            // Check if this is a nack from a REF peer we're connecting to
            CPeers *peers = g_Reflector.GetPeers();
            CPeer *existingPeer = peers->FindPeer(Ip, PROTOCOL_DPLUS);

            if ( existingPeer != NULL )
            {
                CDplusPeer *dplusPeer = dynamic_cast<CDplusPeer *>(existingPeer);
                if ( dplusPeer != NULL )
                {
                    std::cout << "DPlus login nack from peer " << dplusPeer->GetCallsign()
                              << " - connection refused" << std::endl;

                    // If peer was already connected, remove its peer client first
                    if ( dplusPeer->IsConnected() )
                    {
                        CClients *clients = g_Reflector.GetClients();
                        int clientIndex = -1;
                        CClient *client = NULL;
                        while ( (client = clients->FindNextClient(PROTOCOL_DPLUS, &clientIndex)) != NULL )
                        {
                            if ( client->IsPeer() && (client->GetIp().GetAddr() == Ip.GetAddr()) )
                            {
                                g_Reflector.ReleaseStreamOwner(client);
                                clients->RemoveClient(client);
                                break;
                            }
                        }
                        g_Reflector.ReleaseClients();
                    }
                }
                peers->RemovePeer(existingPeer);
            }
            g_Reflector.ReleasePeers();
        }
        else if ( IsValidLoginPacket(Buffer, &Callsign) )
        {
            // Already connected? Just ACK immediately (retry from client)
            bool alreadyConnected = false;
            {
                CClients *clients = g_Reflector.GetClients();
                CClient *existing = clients->FindClient(Ip, PROTOCOL_DPLUS);
                if ( existing != NULL )
                {
                    existing->Alive();
                    alreadyConnected = true;
                }
                g_Reflector.ReleaseClients();
            }

            if ( alreadyConnected )
            {
                CBuffer ackBuf;
                EncodeLoginAckPacket(&ackBuf);
                m_Socket.Send(ackBuf, Ip);
            }
            else
            {
                std::cout << "DPlus login packet from " << Callsign << " at " << Ip << std::endl;

                // Queue pending ack with random delay to stagger connection timing
                CDplusPendingAck pendingAck;
                pendingAck.m_Ip = Ip;
                pendingAck.m_Callsign = Callsign;
                pendingAck.m_CreatedTime.Now();
                pendingAck.m_DelaySeconds = CClient::GetRandomJitter(CONNECT_ACK_JITTER_MAX);
                pendingAck.m_bAccepted = g_GateKeeper.MayLink(Callsign, Ip, PROTOCOL_DPLUS);
                {
                    std::lock_guard<std::mutex> lock(m_PendingAcksMutex);
                    bool alreadyPending = false;
                    for ( const auto &pending : m_PendingAcks )
                    {
                        if ( pending.m_Ip == Ip )
                        {
                            alreadyPending = true;
                            break;
                        }
                    }
                    if ( !alreadyPending )
                    {
                        m_PendingAcks.push_back(pendingAck);
                    }
                }
            }
        }
        else if ( IsValidDisconnectPacket(Buffer) )
        {
            std::cout << "DPlus disconnect packet from " << Ip << std::endl;
            
            // find client
            CClients *clients = g_Reflector.GetClients();
            CClient *client = clients->FindClient(Ip, PROTOCOL_DPLUS);
            if ( client != NULL )
            {
                // remove it
                clients->RemoveClient(client);
                // and acknowledge the disconnect
                EncodeDisconnectPacket(&Buffer);
                m_Socket.Send(Buffer, Ip);
            }
            g_Reflector.ReleaseClients();
        }
        else if ( IsValidKeepAlivePacket(Buffer) )
        {
            //std::cout << "DPlus keepalive packet from " << Ip << std::endl;

            // Check if this is from a REF peer
            CPeers *peers = g_Reflector.GetPeers();
            CPeer *existingPeer = peers->FindPeer(Ip, PROTOCOL_DPLUS);
            if ( existingPeer != NULL )
            {
                existingPeer->Alive();
            }
            g_Reflector.ReleasePeers();

            // find all clients with that callsign & ip and keep them alive
            CClients *clients = g_Reflector.GetClients();
            int index = -1;
            CClient *client = NULL;
            while ( (client = clients->FindNextClient(Ip, PROTOCOL_DPLUS, &index)) != NULL )
            {
                client->Alive();
            }
            // Also keep peer clients alive by IP address (ignoring port)
            // since peer clients may have been created with different port
            index = -1;
            while ( (client = clients->FindNextClient(PROTOCOL_DPLUS, &index)) != NULL )
            {
                if ( client->IsPeer() && (client->GetIp().GetAddr() == Ip.GetAddr()) )
                {
                    client->Alive();
                }
            }
            g_Reflector.ReleaseClients();
        }
        else
        {
            // Check for persistent invalid login attempts (28-byte packets with login tag)
            uint8 loginTag[] = { 0x1C, 0xC0, 0x04, 0x00 };
            if ( Buffer.size() == 28 && ::memcmp(Buffer.data(), loginTag, sizeof(loginTag)) == 0 )
            {
                uint32 addr = Ip.GetAddr();
                if ( m_RejectTrackers.find(addr) == m_RejectTrackers.end() &&
                     m_RejectTrackers.size() >= MAX_REJECT_TRACKERS )
                {
                    // map full — silently drop
                }
                else
                {
                CRejectTracker &tracker = m_RejectTrackers[addr];

                if ( tracker.m_nStrike > 0 &&
                     tracker.m_IgnoreStart.DurationSinceNow() < tracker.m_dIgnoreDuration )
                {
                    // still ignoring
                }
                else
                {
                    tracker.m_nCount++;

                    if ( tracker.m_nCount <= 3 )
                    {
                        // sanitize for log output — prevent terminal injection
                        char cs[9];
                        for ( int j = 0; j < 8; j++ )
                            cs[j] = (Buffer.data()[4+j] >= 0x20 && Buffer.data()[4+j] < 0x7F) ? (char)Buffer.data()[4+j] : '.';
                        cs[8] = 0;

                        std::cout << "DPlus login rejected from " << Ip
                                  << " - callsign: '" << cs << "'"
                                  << " (" << tracker.m_nCount << "/3)" << std::endl;

                        EncodeLoginNackPacket(&Buffer);
                        m_Socket.Send(Buffer, Ip);
                    }
                    else
                    {
                        tracker.m_nStrike++;
                        int ignoreMins;
                        if ( tracker.m_nStrike <= 1 )
                            ignoreMins = 5;
                        else if ( tracker.m_nStrike <= 2 )
                            ignoreMins = 30;
                        else
                            ignoreMins = 60;

                        tracker.m_dIgnoreDuration = ignoreMins * 60.0;
                        tracker.m_IgnoreStart.Now();
                        tracker.m_nCount = 0;

                        std::cout << "DPlus persistent invalid login from " << Ip
                                  << " - ignoring for " << ignoreMins
                                  << " min (strike " << tracker.m_nStrike << ")" << std::endl;
                    }
                }
                } // end of reject tracker cap else
            }
            else
            {
                std::cout << "DPlus unknown packet (" << Buffer.size() << " bytes) from " << Ip << " [";
                int dumpLen = MIN((int)Buffer.size(), 16);
                for ( int i = 0; i < dumpLen; i++ )
                {
                    if ( i > 0 ) std::cout << " ";
                    std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)(unsigned char)Buffer.data()[i];
                }
                if ( (int)Buffer.size() > 16 ) std::cout << " ...";
                std::cout << std::dec << "]" << std::endl;
            }
        }
    }
    
    // periodic cleanup of stale reject trackers
    if ( m_LastCleanupCheck.DurationSinceNow() > CLEANUP_CHECK_INTERVAL )
    {
        for ( auto it = m_RejectTrackers.begin(); it != m_RejectTrackers.end(); )
        {
            CRejectTracker &t = it->second;
            if ( t.m_nStrike > 0 && t.m_IgnoreStart.DurationSinceNow() >= t.m_dIgnoreDuration + 600.0 )
                it = m_RejectTrackers.erase(it);
            else if ( t.m_nStrike == 0 && t.m_IgnoreStart.DurationSinceNow() > 600.0 )
                it = m_RejectTrackers.erase(it);
            else
                ++it;
        }
        m_LastCleanupCheck.Now();
    }

    // handle end of streaming timeout
    CheckStreamsTimeout();

}

////////////////////////////////////////////////////////////////////////////////////////
// TX task — runs on separate thread, never blocked by CloseStream

void CDplusProtocol::TxTask(void)
{
    // wait for queue notification or 20ms timeout
    {
        std::unique_lock<std::mutex> lk(m_QueueCondMutex);
        m_QueueCondVar.wait_for(lk, std::chrono::milliseconds(20));
    }

    // handle queue from reflector
    HandleQueue();

    // handle pending connection acks
    HandlePendingAcks();

    // keep client alive
    if ( m_LastKeepaliveTime.DurationSinceNow() > DPLUS_KEEPALIVE_PERIOD )
    {
        HandleKeepalives();
        m_LastKeepaliveTime.Now();
    }

    // handle REF peer connection states (handshake progress)
    HandleDplusPeerConnectionStates();

    // handle REF peer connections
    if ( m_LastDplusPeerLinkTime.DurationSinceNow() > DPLUS_PEER_RECONNECT_PERIOD )
    {
        HandleDplusPeerLinks();
        m_LastDplusPeerLinkTime.Now();
    }

    // handle REF peer keepalives
    if ( m_LastDplusPeerKeepaliveTime.DurationSinceNow() > DPLUS_PEER_KEEPALIVE_PERIOD )
    {
        HandleDplusPeerKeepalives();
        m_LastDplusPeerKeepaliveTime.Now();
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// streams helpers

bool CDplusProtocol::OnDvHeaderPacketIn(CDvHeaderPacket *Header, const CIp &Ip)
{
    bool newstream = false;

    // set default suffix if not already set
    if ( !Header->HasMySuffix() )
    {
        Header->SetMySuffix("DPLS");
    }

    // find the stream (match on both StreamId and IP to avoid false matches)
    CPacketStream *stream = GetStream(Header->GetStreamId(), &Ip);
    if ( stream == NULL )
    {
        // no stream open yet, open a new one
        CCallsign via(Header->GetRpt1Callsign());
        CCallsign myCallsign;
        CCallsign rpt2Callsign;

        // first, check module is valid
        if ( g_Reflector.IsValidModule(Header->GetRpt1Module()) )
        {
            // find this client
            CClients *clients = g_Reflector.GetClients();
            CClient *client = clients->FindClient(Ip, PROTOCOL_DPLUS);

            // If not found by exact IP match, check if this is from a DPlus peer
            // (peer clients may have been created with a different port)
            if ( client == NULL )
            {
                // Look through all DPlus protocol clients for a matching IP address (ignoring port)
                int index = -1;
                CClient *c = NULL;
                while ( (c = clients->FindNextClient(PROTOCOL_DPLUS, &index)) != NULL )
                {
                    if ( c->IsPeer() && (c->GetIp() == Ip) )
                    {
                        client = c;
                        break;
                    }
                }
            }

            if ( client != NULL )
            {
                // now we know if it's a dextra dongle or a genuine dplus node
                if ( Header->GetRpt2Callsign().HasSameCallsignWithWildcard(CCallsign("XRF*"))  )
                {
                    client->SetDextraDongle();
                }
                // now we know its module, let's update it
                if ( !client->HasModule() )
                {
                    client->SetModule(Header->GetRpt1Module());
                }
                // For peer clients, update RPT2 module to our local module
                // This ensures the stream opens on the correct local module
                // regardless of what module the remote reflector sent in the header
                if ( client->IsPeer() )
                {
                    Header->SetRpt2Module(client->GetReflectorModule());
                }
                // get client callsign
                via = client->GetCallsign();
                // save header callsigns before OpenStream — OpenStream transfers
                // Header into the router queue, where the router thread may delete it
                myCallsign = Header->GetMyCallsign();
                rpt2Callsign = Header->GetRpt2Callsign();
                // and try to open the stream
                if ( (stream = g_Reflector.OpenStream(Header, client)) != NULL )
                {
                    // keep the handle — Header ownership transferred to stream
                    m_Streams.push_back(stream);
                    newstream = true;
                    Header = NULL;
                }
                else if ( g_Reflector.TryLateEntry(Header, client) )
                {
                    Header = NULL;  // ownership transferred
                }
            }
            // release
            g_Reflector.ReleaseClients();

            // update last heard
            if ( newstream )
            {
                g_Reflector.GetUsers()->Hearing(myCallsign, via, rpt2Callsign);
                g_Reflector.ReleaseUsers();
            }

            // delete header if needed
            if ( Header != NULL )
            {
                delete Header;
            }
        }
        else
        {
            std::cout << "DPlus node " << via << " link attempt on non-existing module" << std::endl;
            delete Header;
        }
    }
    else
    {
        // stream already open
        // skip packet, but tickle the stream
        stream->Tickle();
        // and delete packet
        delete Header;
    }
    
    // done
    return newstream;
}

////////////////////////////////////////////////////////////////////////////////////////
// pending ack helper

void CDplusProtocol::HandlePendingAcks(void)
{
    // collect ready acks under lock, then process without lock
    std::vector<CDplusPendingAck> readyAcks;
    {
        std::lock_guard<std::mutex> lock(m_PendingAcksMutex);
        for ( auto it = m_PendingAcks.begin(); it != m_PendingAcks.end(); )
        {
            if ( it->m_CreatedTime.DurationSinceNow() >= it->m_DelaySeconds )
            {
                readyAcks.push_back(*it);
                it = m_PendingAcks.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    // process ready acks without holding lock
    for ( const CDplusPendingAck &ack : readyAcks )
    {
        CBuffer buffer;
        if ( ack.m_bAccepted )
        {
            // acknowledge the request
            EncodeLoginAckPacket(&buffer);
            m_Socket.Send(buffer, ack.m_Ip);

            // create the client
            CDplusClient *client = new CDplusClient(ack.m_Callsign, ack.m_Ip);

            // and append
            g_Reflector.GetClients()->AddClient(client);
            g_Reflector.ReleaseClients();
        }
        else
        {
            // deny the request
            EncodeLoginNackPacket(&buffer);
            m_Socket.Send(buffer, ack.m_Ip);

            std::cout << "DPlus login nack sent to " << ack.m_Callsign << " at " << ack.m_Ip << std::endl;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// queue helper

void CDplusProtocol::RefreshClientCache(int iModId)
{
    // bounds check
    if ( iModId < 0 || iModId >= NB_OF_MODULES )
        return;

    // Check freshness under cache lock, release before acquiring Clients lock
    // to prevent cache→Clients lock inversion
    {
        std::lock_guard<std::mutex> lock(m_ClientCache[iModId].m_Mutex);
        if ( m_ClientCache[iModId].m_bInitialized &&
             m_ClientCache[iModId].m_LastRefresh.DurationSinceNow() < DPLUS_CLIENT_CACHE_REFRESH_INTERVAL )
        {
            return;  // Cache is still fresh
        }
    }

    // Scan clients WITHOUT holding cache lock
    char moduleChar = (char)('A' + iModId);
    std::vector<CDplusCachedClient> freshClients;

    CClients *clients = g_Reflector.GetClients();
    int index = -1;
    CClient *client = NULL;
    while ( (client = clients->FindNextClient(PROTOCOL_DPLUS, &index)) != NULL )
    {
        // cache non-master, non-peer clients on this module
        if ( !client->IsAMaster() && !client->IsPeer() && (client->GetReflectorModule() == moduleChar) )
        {
            CDplusClient *dplusClient = static_cast<CDplusClient *>(client);
            CDplusCachedClient cached;
            cached.m_Ip = client->GetIp();
            cached.m_bIsDextraDongle = dplusClient->IsDextraDongle();
            cached.m_bHasModule = client->HasModule();
            freshClients.push_back(cached);
        }
    }
    g_Reflector.ReleaseClients();

    // Write results under cache lock
    std::lock_guard<std::mutex> lock(m_ClientCache[iModId].m_Mutex);
    m_ClientCache[iModId].m_Clients.swap(freshClients);
    m_ClientCache[iModId].m_LastRefresh.Now();
    m_ClientCache[iModId].m_bInitialized = true;
}

void CDplusProtocol::SendDvHeaderCached(CDvHeaderPacket *packet, const CDplusCachedClient &client)
{
    // encode it
    CBuffer buffer;
    if ( EncodeDvPacket(*packet, &buffer) )
    {
        if ( client.m_bIsDextraDongle || !client.m_bHasModule )
        {
            // clone the packet and patch it
            CDvHeaderPacket packet2(*packet);
            CCallsign rpt2 = packet2.GetRpt2Callsign();
            rpt2.PatchCallsign(0, (const uint8 *)"XRF", 3);
            packet2.SetRpt2Callsign(rpt2);

            // encode it
            CBuffer buffer2;
            if ( EncodeDvPacket(packet2, &buffer2) )
            {
                // and send it (voice packet - use DSCP marking)
                m_Socket.SendVoice(buffer2, client.m_Ip);
            }

            // client type known ?
            if ( !client.m_bHasModule )
            {
                // no, send also the genuine packet (voice packet - use DSCP marking)
                m_Socket.SendVoice(buffer, client.m_Ip);
            }
        }
        else
        {
            // otherwise, send the original packet (voice packet - use DSCP marking)
            m_Socket.SendVoice(buffer, client.m_Ip);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// echo detection helper

bool CDplusProtocol::IsEchoOfOutboundStream(uint16 streamId, const CIp &ip)
{
    for ( int i = 0; i < NB_OF_MODULES; i++ )
    {
        std::lock_guard<std::mutex> lock(m_StreamsCache[i].m_Mutex);
        if ( m_StreamsCache[i].m_bHasOwner &&
             m_StreamsCache[i].m_uiOutboundStreamId != 0 &&
             m_StreamsCache[i].m_uiOutboundStreamId == streamId &&
             !(m_StreamsCache[i].m_OwnerIp == ip) )
        {
            return true;
        }
    }
    return false;
}

void CDplusProtocol::HandleQueue(void)
{
    // drain queue quickly while holding lock
    std::vector<CPacket *> packets;
    packets.reserve(100);

    m_Queue.Lock();
    while ( !m_Queue.empty() )
    {
        packets.push_back(m_Queue.front());
        m_Queue.pop();
    }
    m_Queue.Unlock();

    // nothing to do?
    if ( packets.empty() )
    {
        return;
    }

    // process packets without holding queue lock
    for ( size_t i = 0; i < packets.size(); i++ )
    {
        CPacket *packet = packets[i];
        packets[i] = NULL;  // clear to prevent double-delete on exception

        // get our sender's id
        int iModId = g_Reflector.GetModuleIndex(packet->GetModuleId());

        // bounds check
        if ( iModId < 0 || iModId >= NB_OF_MODULES )
        {
            delete packet;
            continue;
        }

        // update cache and snapshot owner under slot lock
        bool hasOwner;
        CIp ownerIp;
        CDvHeaderPacket cachedHeader;
        uint8 seqCounter;
        {
            std::lock_guard<std::mutex> lock(m_StreamsCache[iModId].m_Mutex);

            if ( packet->IsDvHeader() )
            {
                m_StreamsCache[iModId].m_dvHeader = CDvHeaderPacket((const CDvHeaderPacket &)*packet);
                m_StreamsCache[iModId].m_iSeqCounter = 0;
                m_StreamsCache[iModId].m_uiOutboundStreamId = packet->GetStreamId();
                m_StreamsCache[iModId].m_bHasOwner = false;
                CPacketStream *ownerStream = g_Reflector.GetStream(packet->GetModuleId());
                if ( ownerStream != NULL )
                {
                    ownerStream->Lock();
                    if ( ownerStream->IsOpen() && ownerStream->GetStreamId() == packet->GetStreamId() )
                    {
                        const CIp *oip = ownerStream->GetOwnerIp();
                        if ( oip != NULL )
                        {
                            m_StreamsCache[iModId].m_OwnerIp = *oip;
                            m_StreamsCache[iModId].m_bHasOwner = true;
                        }
                    }
                    ownerStream->Unlock();
                }
            }

            // snapshot for send loops
            hasOwner = m_StreamsCache[iModId].m_bHasOwner;
            ownerIp = m_StreamsCache[iModId].m_OwnerIp;
            cachedHeader = m_StreamsCache[iModId].m_dvHeader;
            seqCounter = m_StreamsCache[iModId].m_iSeqCounter;

            // increment seq counter for frames
            if ( !packet->IsDvHeader() && packet->IsDvFrame() )
            {
                m_StreamsCache[iModId].m_iSeqCounter++;
            }
        } // slot lock released before network I/O

        // refresh per-module client cache
        RefreshClientCache(iModId);

        // send to clients and peers (cache mutex is TX-thread-only, safe to hold during sends)
        std::lock_guard<std::mutex> cacheLock(m_ClientCache[iModId].m_Mutex);
        if ( packet->IsDvHeader() )
        {
            for ( const CDplusCachedClient &client : m_ClientCache[iModId].m_Clients )
            {
                if ( hasOwner && ownerIp == client.m_Ip )
                    continue;
                SendDvHeaderCached((CDvHeaderPacket *)packet, client);
            }

            CPeers *peers = g_Reflector.GetPeers();
            int peerIndex = -1;
            CPeer *peer = NULL;
            while ( (peer = peers->FindNextPeer(PROTOCOL_DPLUS, &peerIndex)) != NULL )
            {
                CDplusPeer *dplusPeer = dynamic_cast<CDplusPeer *>(peer);
                if ( dplusPeer == NULL || !dplusPeer->IsConnected() )
                    continue;
                if ( packet->GetModuleId() != dplusPeer->GetLocalModule() )
                    continue;
                CIp peerIp = dplusPeer->GetIp();
                peerIp.SetPort(htons(dplusPeer->GetPort()));
                if ( hasOwner && ownerIp == peerIp )
                    continue;

                CDvHeaderPacket peerHeader((const CDvHeaderPacket &)*packet);
                peerHeader.SetRpt2Module(dplusPeer->GetRemoteModule());
                CBuffer peerBuffer;
                if ( EncodeDvPacket(peerHeader, &peerBuffer) )
                {
                    m_Socket.SendVoice(peerBuffer, dplusPeer->GetIp(), dplusPeer->GetPort());
                }
            }
            g_Reflector.ReleasePeers();
        }
        else
        {
            CBuffer buffer;
            if ( EncodeDvPacket(*packet, &buffer) )
            {
                for ( const CDplusCachedClient &client : m_ClientCache[iModId].m_Clients )
                {
                    if ( hasOwner && ownerIp == client.m_Ip )
                        continue;

                    m_Socket.SendVoice(buffer, client.m_Ip);

                    if ( packet->IsDvFrame() && (seqCounter % 21) == 20 )
                    {
                        CDvHeaderPacket packet2(cachedHeader);
                        SendDvHeaderCached(&packet2, client);
                    }
                }

                CPeers *peers = g_Reflector.GetPeers();
                int peerIndex = -1;
                CPeer *peer = NULL;
                while ( (peer = peers->FindNextPeer(PROTOCOL_DPLUS, &peerIndex)) != NULL )
                {
                    CDplusPeer *dplusPeer = dynamic_cast<CDplusPeer *>(peer);
                    if ( dplusPeer == NULL || !dplusPeer->IsConnected() )
                        continue;
                    if ( packet->GetModuleId() != dplusPeer->GetLocalModule() )
                        continue;
                    CIp peerIp2 = dplusPeer->GetIp();
                    peerIp2.SetPort(htons(dplusPeer->GetPort()));
                    if ( hasOwner && ownerIp == peerIp2 )
                        continue;

                    m_Socket.SendVoice(buffer, dplusPeer->GetIp(), dplusPeer->GetPort());

                    if ( packet->IsDvFrame() && (seqCounter % 21) == 20 )
                    {
                        CDvHeaderPacket packet2(cachedHeader);
                        packet2.SetRpt2Module(dplusPeer->GetRemoteModule());
                        CBuffer headerBuffer;
                        if ( EncodeDvPacket(packet2, &headerBuffer) )
                        {
                            m_Socket.SendVoice(headerBuffer, dplusPeer->GetIp(), dplusPeer->GetPort());
                        }
                    }
                }
                g_Reflector.ReleasePeers();
            }
        }

        // done
        delete packet;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// keepalive helpers

void CDplusProtocol::HandleKeepalives(void)
{
    // send keepalives using per-client timing for staggering
    CBuffer keepalive;
    EncodeKeepAlivePacket(&keepalive);

    // iterate on clients
    CClients *clients = g_Reflector.GetClients();
    int index = -1;
    CClient *client = NULL;
    std::vector<CClient *> toRemove;
    while ( (client = clients->FindNextClient(PROTOCOL_DPLUS, &index)) != NULL )
    {
        // check if it's time to send keepalive to this client
        if ( client->IsKeepaliveDue() )
        {
            // send keepalive
            m_Socket.Send(keepalive, client->GetIp());

            // reset timer and schedule next keepalive at regular interval
            client->ResetKeepaliveTimer();
            client->ScheduleNextKeepalive(DPLUS_KEEPALIVE_PERIOD);

            // is this client busy ?
            if ( client->IsAMaster() )
            {
                // yes, just tickle it
                client->Alive();
            }
            // check it's still with us
            else if ( !client->IsAlive() )
            {
                // no, disconnect
                CBuffer disconnect;
                EncodeDisconnectPacket(&disconnect);
                m_Socket.Send(disconnect, client->GetIp());

                // collect for removal after loop
                std::cout << "DPlus client " << client->GetCallsign() << " keepalive timeout" << std::endl;
                toRemove.push_back(client);
            }
        }
    }
    for ( size_t i = 0; i < toRemove.size(); i++ )
    {
        clients->RemoveClient(toRemove[i]);
    }
    g_Reflector.ReleaseClients();
}

////////////////////////////////////////////////////////////////////////////////////////
// packet decoding helpers

bool CDplusProtocol::IsValidConnectPacket(const CBuffer &Buffer)
{
    uint8 tag[] = { 0x05,0x00,0x18,0x00,0x01 };
    return (Buffer == CBuffer(tag, sizeof(tag)));
}

bool CDplusProtocol::IsValidLoginPacket(const CBuffer &Buffer, CCallsign *Callsign)
{
    uint8 Tag[] = { 0x1C,0xC0,0x04,0x00 };
    bool valid = false;
    
    if ( (Buffer.size() == 28) &&(::memcmp(Buffer.data(), Tag, sizeof(Tag)) == 0) )
    {
        Callsign->SetCallsign(&(Buffer.data()[4]), 8);
        valid = Callsign->IsValid();
    }
    return valid;
}

bool CDplusProtocol::IsValidDisconnectPacket(const CBuffer &Buffer)
{
    uint8 tag[] = { 0x05,0x00,0x18,0x00,0x00 };
    return (Buffer == CBuffer(tag, sizeof(tag)));
}

bool CDplusProtocol::IsValidKeepAlivePacket(const CBuffer &Buffer)
{
    uint8 tag[] = { 0x03,0x60,0x00 };
    return (Buffer == CBuffer(tag, sizeof(tag)));
}

CDvHeaderPacket *CDplusProtocol::IsValidDvHeaderPacket(const CBuffer &Buffer)
{
    CDvHeaderPacket *header = NULL;
    
    if ( (Buffer.size() == 58) &&
         (Buffer.data()[0] == 0x3A) && (Buffer.data()[1] == 0x80) &&
         (Buffer.Compare((uint8 *)"DSVT", 2, 4) == 0) &&
         (Buffer.data()[6] == 0x10) && (Buffer.data()[10] == 0x20) )
    {
        // create packet
        header = new CDvHeaderPacket((struct dstar_header *)&(Buffer.data()[17]),
                                     *((uint16 *)&(Buffer.data()[14])), 0x80);
        // check validity of packet
        if ( !header->IsValid() )
        {
            delete header;
            header = NULL;
        }
    }
    return header;
}

CDvFramePacket *CDplusProtocol::IsValidDvFramePacket(const CBuffer &Buffer)
{
    CDvFramePacket *dvframe = NULL;
    
    if ( (Buffer.size() == 29) &&
         (Buffer.data()[0] == 0x1D) && (Buffer.data()[1] == 0x80) &&
         (Buffer.Compare((uint8 *)"DSVT", 2, 4) == 0) &&
         (Buffer.data()[6] == 0x20) && (Buffer.data()[10] == 0x20) )
    {
        // create packet
        dvframe = new CDvFramePacket((struct dstar_dvframe *)&(Buffer.data()[17]),
                                     *((uint16 *)&(Buffer.data()[14])), Buffer.data()[16]);
        // check validity of packet
        if ( !dvframe->IsValid() )
        {
            delete dvframe;
            dvframe = NULL;
        }
    }
    return dvframe;
}

CDvLastFramePacket *CDplusProtocol::IsValidDvLastFramePacket(const CBuffer &Buffer)
{
    CDvLastFramePacket *dvframe = NULL;
    
    if ( (Buffer.size() == 32) &&
         (Buffer.Compare((uint8 *)"DSVT", 2, 4) == 0) &&
         (Buffer.data()[0] == 0x20) && (Buffer.data()[1] == 0x80) &&
         (Buffer.data()[6] == 0x20) && (Buffer.data()[10] == 0x20) )
    {
        // create packet
        dvframe = new CDvLastFramePacket((struct dstar_dvframe *)&(Buffer.data()[17]),
                                         *((uint16 *)&(Buffer.data()[14])), Buffer.data()[16] & 0x1F);
        // check validity of packet
        if ( !dvframe->IsValid() )
        {
            delete dvframe;
            dvframe = NULL;
        }
    }
    return dvframe;
}


////////////////////////////////////////////////////////////////////////////////////////
// packet encoding helpers

void CDplusProtocol::EncodeKeepAlivePacket(CBuffer *Buffer)
{
    uint8 tag[] = { 0x03,0x60,0x00 };
    Buffer->Set(tag, sizeof(tag));
}

void CDplusProtocol::EncodeLoginAckPacket(CBuffer *Buffer)
{
    uint8 tag[] = { 0x08,0xC0,0x04,0x00,'O','K','R','W' };
    Buffer->Set(tag, sizeof(tag));
}

void CDplusProtocol::EncodeLoginNackPacket(CBuffer *Buffer)
{
    uint8 tag[] = { 0x08,0xC0,0x04,0x00,'B','U','S','Y' };
    Buffer->Set(tag, sizeof(tag));
}

void CDplusProtocol::EncodeDisconnectPacket(CBuffer *Buffer)
{
    uint8 tag[] = { 0x05,0x00,0x18,0x00,0x00 };
    Buffer->Set(tag, sizeof(tag));
}

void CDplusProtocol::SendPeerLinkAnnouncement(CDplusPeer *peer)
{
    // Send a short silent transmission to establish the link on the remote module
    // This makes REF behave like DCS/XRF where the link appears immediately

    // D-Star AMBE silence pattern and sync
    struct dstar_dvframe silentFrame;
    static const uint8 AMBE_SILENCE[AMBE_SIZE] = { 0x9E, 0x8D, 0x32, 0x88, 0x26, 0x1A, 0x3F, 0x61, 0xE8 };
    static const uint8 DVDATA_SYNC[DVDATA_SIZE] = { 0x55, 0x2D, 0x16 };
    ::memcpy(silentFrame.AMBE, AMBE_SILENCE, AMBE_SIZE);
    ::memcpy(silentFrame.DVDATA, DVDATA_SYNC, DVDATA_SIZE);

    // Generate a unique stream ID
    uint16 streamId = (uint16)::rand();

    // Build header packet
    // RPT1 = our callsign with local module
    // RPT2 = remote reflector with remote module
    // MY = our callsign
    CCallsign rpt1 = GetReflectorCallsign();
    rpt1.SetModule(peer->GetLocalModule());
    CCallsign rpt2 = peer->GetCallsign();
    rpt2.SetModule(peer->GetRemoteModule());
    CCallsign my = GetReflectorCallsign();
    my.SetModule(peer->GetLocalModule());

    CDvHeaderPacket header(my, CCallsign("CQCQCQ"), rpt1, rpt2, streamId, 0);

    CBuffer buffer;
    if ( EncodeDvHeaderPacket(header, &buffer) )
    {
        m_Socket.Send(buffer, peer->GetIp(), peer->GetPort());
    }

    // Send 3 silent voice frames (minimum for valid transmission)
    for ( uint8 i = 0; i < 3; i++ )
    {
        CDvFramePacket frame(&silentFrame, streamId, i);
        buffer.clear();
        EncodeDvFramePacket(frame, &buffer);
        m_Socket.Send(buffer, peer->GetIp(), peer->GetPort());
    }

    // Send last frame
    CDvLastFramePacket lastFrame(&silentFrame, streamId, 3);
    buffer.clear();
    EncodeDvLastFramePacket(lastFrame, &buffer);
    m_Socket.Send(buffer, peer->GetIp(), peer->GetPort());

    std::cout << "DPlus sent link announcement to " << peer->GetCallsign() << std::endl;
}

bool CDplusProtocol::EncodeDvHeaderPacket(const CDvHeaderPacket &Packet, CBuffer *Buffer) const
{
    uint8 tag[]	= { 0x3A,0x80,0x44,0x53,0x56,0x54,0x10,0x00,0x00,0x00,0x20,0x00,0x01,0x02 };
    struct dstar_header DstarHeader;
    
    Packet.ConvertToDstarStruct(&DstarHeader);
   
    Buffer->Set(tag, sizeof(tag));
    Buffer->Append(Packet.GetStreamId());
    Buffer->Append((uint8)0x80);
    Buffer->Append((uint8 *)&DstarHeader, sizeof(struct dstar_header));

    return true;
}

bool CDplusProtocol::EncodeDvFramePacket(const CDvFramePacket &Packet, CBuffer *Buffer) const
{
    uint8 tag[] = { 0x1D,0x80,0x44,0x53,0x56,0x54,0x20,0x00,0x00,0x00,0x20,0x00,0x01,0x02 };

    Buffer->Set(tag, sizeof(tag));
    Buffer->Append(Packet.GetStreamId());
    Buffer->Append((uint8)(Packet.GetPacketId() % 21));
    Buffer->Append((uint8 *)Packet.GetAmbe(), AMBE_SIZE);
    Buffer->Append((uint8 *)Packet.GetDvData(), DVDATA_SIZE);

    return true;
    
}

bool CDplusProtocol::EncodeDvLastFramePacket(const CDvLastFramePacket &Packet, CBuffer *Buffer) const
{
    uint8 tag1[] = { 0x20,0x80,0x44,0x53,0x56,0x54,0x20,0x00,0x81,0x00,0x20,0x00,0x01,0x02 };
    uint8 tag2[] = { 0x55,0xC8,0x7A,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x25,0x1A,0xC6 };

    Buffer->Set(tag1, sizeof(tag1));
    Buffer->Append(Packet.GetStreamId());
    Buffer->Append((uint8)((Packet.GetPacketId() % 21) | 0x40));
    Buffer->Append(tag2, sizeof(tag2));

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
// REF peer connection packet encoding

void CDplusProtocol::EncodeConnectPacket(CBuffer *Buffer)
{
    uint8 tag[] = { 0x05,0x00,0x18,0x00,0x01 };
    Buffer->Set(tag, sizeof(tag));
}

void CDplusProtocol::EncodeLoginPacket(CBuffer *Buffer, const CCallsign &Callsign, char localModule, char remoteModule)
{
    // Login packet: 0x1C,0xC0,0x04,0x00 + callsign (8 bytes) + remote module + padding (15 bytes)
    // Format: [4-byte header][8-byte callsign with local module in byte 7][remote module][15 bytes padding]
    // - Local module goes in callsign position 7 (byte 11 of packet)
    // - Remote/target module goes at byte 12
    uint8 tag[] = { 0x1C,0xC0,0x04,0x00 };
    uint8 callsign[8];
    uint8 padding[15];

    // Get callsign as 8 bytes, space-padded
    Callsign.GetCallsign(callsign);

    // Set local module in position 7 of callsign (byte 11 of packet)
    if ( localModule != ' ' && localModule != '\0' )
    {
        callsign[7] = (uint8)localModule;
    }

    // Zero the padding
    ::memset(padding, 0, sizeof(padding));

    Buffer->Set(tag, sizeof(tag));
    Buffer->Append(callsign, sizeof(callsign));
    // Append remote/target module at byte 12
    Buffer->Append((uint8)remoteModule);
    Buffer->Append(padding, sizeof(padding));
}

void CDplusProtocol::EncodeLinkPacket(CBuffer *Buffer, char module)
{
    // Link packet to request a specific module after authentication
    // Format: 05 00 04 00 [module ASCII]
    uint8 tag[] = { 0x05, 0x00, 0x04, 0x00 };
    Buffer->Set(tag, sizeof(tag));
    Buffer->Append((uint8)module);
}

bool CDplusProtocol::IsValidLoginAckPacket(const CBuffer &Buffer)
{
    uint8 tag[] = { 0x08,0xC0,0x04,0x00,'O','K','R','W' };
    return (Buffer == CBuffer(tag, sizeof(tag)));
}

bool CDplusProtocol::IsValidLoginNackPacket(const CBuffer &Buffer)
{
    uint8 tag[] = { 0x08,0xC0,0x04,0x00,'B','U','S','Y' };
    return (Buffer == CBuffer(tag, sizeof(tag)));
}

////////////////////////////////////////////////////////////////////////////////////////
// REF peer helpers

bool CDplusProtocol::IsRefPeerCallsign(const CCallsign &callsign) const
{
    // REF peer callsigns start with "REF" followed by a digit
    char cs[CALLSIGN_LEN + 1];
    callsign.GetCallsign((uint8 *)cs);
    cs[CALLSIGN_LEN] = '\0';

    return (::strncmp(cs, "REF", 3) == 0) && (cs[3] >= '0') && (cs[3] <= '9');
}

CCallsignListItem *CDplusProtocol::FindRefPeerByIp(CPeerCallsignList *list, const CIp &ip)
{
    CCallsignListItem *item = NULL;

    for ( int i = 0; i < list->size(); i++ )
    {
        CCallsignListItem *listItem = &(list->data()[i]);
        if ( IsRefPeerCallsign(listItem->GetCallsign()) &&
             (listItem->GetIp().GetAddr() == ip.GetAddr()) )
        {
            item = listItem;
            break;
        }
    }
    return item;
}

////////////////////////////////////////////////////////////////////////////////////////
// REF peer connection state handling

void CDplusProtocol::HandleDplusPeerConnectionStates(void)
{
    // Check for connection timeouts
    CPeers *peers = g_Reflector.GetPeers();
    int index = -1;
    CPeer *peer = NULL;
    std::vector<CPeer *> peersToRemove;

    while ( (peer = peers->FindNextPeer(PROTOCOL_DPLUS, &index)) != NULL )
    {
        CDplusPeer *dplusPeer = dynamic_cast<CDplusPeer *>(peer);
        if ( dplusPeer == NULL )
        {
            continue;
        }

        // Only check peers that are in the process of connecting
        if ( dplusPeer->IsConnecting() )
        {
            if ( dplusPeer->GetConnectDuration() > DPLUS_PEER_CONNECT_TIMEOUT )
            {
                std::cout << "DPlus peer " << dplusPeer->GetCallsign()
                          << " connection timeout (state "
                          << dplusPeer->GetConnectionState() << ")" << std::endl;
                peersToRemove.push_back(peer);
            }
        }
    }
    for ( size_t i = 0; i < peersToRemove.size(); i++ )
    {
        peers->RemovePeer(peersToRemove[i]);
    }

    g_Reflector.ReleasePeers();
}

////////////////////////////////////////////////////////////////////////////////////////
// REF peer link handling

void CDplusProtocol::HandleDplusPeerLinks(void)
{
    CBuffer buffer;

    // Snapshot the peer list under the GK lock, then release BEFORE acquiring
    // Peers or Clients locks. Prevents ABBA: RX holds Clients → GK (via MayLink).
    std::vector<CCallsignListItem> peerListSnapshot;
    {
        CPeerCallsignList *list = g_GateKeeper.GetPeerList();
        peerListSnapshot.assign(list->begin(), list->end());
        g_GateKeeper.ReleasePeerList();
    }

    CPeers *peers = g_Reflector.GetPeers();

    // check if all our connected DPlus peers are still listed by gatekeeper
    // if not, send disconnect and clean up
    int index = -1;
    CPeer *peer = NULL;
    std::vector<CPeer *> peersToRemove;
    while ( (peer = peers->FindNextPeer(PROTOCOL_DPLUS, &index)) != NULL )
    {
        bool foundInList = false;
        for ( const auto &item : peerListSnapshot )
        {
            if ( item.GetCallsign().HasSameCallsign(peer->GetCallsign()) )
            {
                foundInList = true;
                break;
            }
        }
        if ( !foundInList )
        {
            CDplusPeer *dplusPeer = dynamic_cast<CDplusPeer *>(peer);
            std::cout << "DPlus peer " << peer->GetCallsign() << " removed - no longer in peer list, sending disconnect" << std::endl;

            // Send disconnect packet
            if ( dplusPeer != NULL && dplusPeer->IsConnected() )
            {
                CBuffer disconnect;
                EncodeDisconnectPacket(&disconnect);
                m_Socket.Send(disconnect, dplusPeer->GetIp(), dplusPeer->GetPort());
            }

            // Find and remove the associated peer client
            CClients *clients = g_Reflector.GetClients();
            int clientIndex = -1;
            CClient *client = NULL;
            while ( (client = clients->FindNextClient(PROTOCOL_DPLUS, &clientIndex)) != NULL )
            {
                if ( client->IsPeer() && (client->GetIp().GetAddr() == peer->GetIp().GetAddr()) )
                {
                    g_Reflector.ReleaseStreamOwner(client);
                    clients->RemoveClient(client);
                    break;
                }
            }
            g_Reflector.ReleaseClients();

            peersToRemove.push_back(peer);
        }
    }
    for ( size_t i = 0; i < peersToRemove.size(); i++ )
    {
        peers->RemovePeer(peersToRemove[i]);
    }

    // check if all REF peers listed by gatekeeper are connected
    // if not, initiate connection
    for ( size_t i = 0; i < peerListSnapshot.size(); i++ )
    {
        CCallsignListItem *item = &peerListSnapshot[i];

        // Only process REF peers
        if ( !IsRefPeerCallsign(item->GetCallsign()) )
        {
            continue;
        }

        if ( peers->FindPeer(item->GetCallsign(), PROTOCOL_DPLUS) == NULL )
        {
            // resolve again peer's IP in case it's a dynamic IP
            item->ResolveIp();

            // get the port (custom or default)
            uint16 port = item->GetPort();
            if ( port == 0 )
            {
                port = DPLUS_PORT;
            }

            // Create peer object in CONNECTING state
            CVersion version(1, 0, 0);
            char modules[3];
            modules[0] = item->GetLocalModule();
            modules[1] = item->GetRemoteModule();
            modules[2] = '\0';

            CDplusPeer *newPeer = new CDplusPeer(item->GetCallsign(), item->GetIp(), modules, version);
            newPeer->SetPort(port);
            newPeer->SetConnectionState(DPLUS_PEER_STATE_CONNECTING);
            newPeer->ResetConnectTimer();

            // Add to peers list
            peers->AddPeer(newPeer);

            // send connect packet to initiate handshake
            std::cout << "DPlus connecting to peer " << item->GetCallsign()
                      << " @ " << item->GetIp() << ":" << port
                      << " (local " << modules[0] << " -> remote " << modules[1] << ")" << std::endl;
            EncodeConnectPacket(&buffer);
            m_Socket.Send(buffer, item->GetIp(), port);
        }
    }

    // done
    g_Reflector.ReleasePeers();
}

////////////////////////////////////////////////////////////////////////////////////////
// REF peer keepalive handling

void CDplusProtocol::HandleDplusPeerKeepalives(void)
{
    CBuffer buffer;

    // get the list of peers
    CPeers *peers = g_Reflector.GetPeers();

    // send keepalive to all connected DPlus peers and check for timeouts
    int index = -1;
    CPeer *peer = NULL;
    std::vector<CPeer *> peersToRemove;
    while ( (peer = peers->FindNextPeer(PROTOCOL_DPLUS, &index)) != NULL )
    {
        CDplusPeer *dplusPeer = dynamic_cast<CDplusPeer *>(peer);
        if ( dplusPeer == NULL )
        {
            continue;
        }

        // Only process fully connected peers
        if ( !dplusPeer->IsConnected() )
        {
            continue;
        }

        // check if still alive
        if ( !dplusPeer->IsAlive() )
        {
            std::cout << "DPlus peer " << peer->GetCallsign() << " keepalive timeout, disconnecting..." << std::endl;

            // Lock order: Peers → Clients (consistent across all peer management paths)
            // Remove peer client from clients list
            CClients *clients = g_Reflector.GetClients();
            int clientIndex = -1;
            CClient *client = NULL;
            while ( (client = clients->FindNextClient(PROTOCOL_DPLUS, &clientIndex)) != NULL )
            {
                if ( client->IsPeer() && (client->GetIp().GetAddr() == dplusPeer->GetIp().GetAddr()) )
                {
                    g_Reflector.ReleaseStreamOwner(client);
                    clients->RemoveClient(client);
                    break;
                }
            }
            g_Reflector.ReleaseClients();

            // Collect for removal after loop
            peersToRemove.push_back(peer);
        }
        else
        {
            // send keepalive
            EncodeKeepAlivePacket(&buffer);
            m_Socket.Send(buffer, peer->GetIp(), dplusPeer->GetPort());
        }
    }
    for ( size_t i = 0; i < peersToRemove.size(); i++ )
    {
        peers->RemovePeer(peersToRemove[i]);
    }

    g_Reflector.ReleasePeers();
}
