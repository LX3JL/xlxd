//
//  cdcsprotocol.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 07/11/2015.
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
#include "cdcsclient.h"
#include "cdcsprotocol.h"
#include "cdcspeer.h"
#include "cdcspeerclient.h"
#include "cdstarslowdata.h"
#include "creflector.h"
#include "cgatekeeper.h"
#include "cpeers.h"

// periodic cleanup interval for stale reject trackers
#define CLEANUP_CHECK_INTERVAL  5.0     // seconds between cleanup sweeps

////////////////////////////////////////////////////////////////////////////////////////
// operation

bool CDcsProtocol::Init(void)
{
    bool ok;
    
    // base class
    ok = CProtocol::Init();
    
    // update the reflector callsign
    m_ReflectorCallsign.PatchCallsign(0, (const uint8 *)"DCS", 3);
    
    // create our socket
    ok &= m_Socket.Open(DCS_PORT);
    if ( !ok )
    {
        std::cout << "Error opening socket on port UDP" << DCS_PORT << " on ip " << g_Reflector.GetListenIp() << std::endl;
    }
    
    // update time
    m_LastKeepaliveTime.Now();
    m_LastDcsPeerLinkTime.Now();
    m_LastDcsPeerKeepaliveTime.Now();

    // done
    return ok;
}



////////////////////////////////////////////////////////////////////////////////////////
// task

void CDcsProtocol::RxTask(void)
{
    CBuffer             Buffer;
    CIp                 Ip;
    CCallsign           Callsign;
    char                ToLinkModule;
    CDvHeaderPacket     *Header;
    CDvFramePacket      *Frame;
    
    // handle incoming packets
    if ( m_Socket.Receive(&Buffer, &Ip, 20) != -1 )
    {
        // crack the packet
        if ( IsValidDvPacket(Buffer, &Header, &Frame) )
        {
            //std::cout << "DCS DV packet" << std::endl;

            // check if this is an echo of our own outbound stream
            // (e.g., a linked reflector sending our audio back with the same stream ID)
            uint16 incomingSid = Header->GetStreamId();
            bool echoDetected = false;
            for ( int i = 0; i < NB_OF_MODULES && !echoDetected; i++ )
            {
                std::lock_guard<std::mutex> lock(m_StreamsCache[i].m_Mutex);
                if ( m_StreamsCache[i].m_bHasOwner &&
                     m_StreamsCache[i].m_uiOutboundStreamId != 0 &&
                     m_StreamsCache[i].m_uiOutboundStreamId == incomingSid &&
                     !(m_StreamsCache[i].m_OwnerIp == Ip) )
                {
                    echoDetected = true;
                }
            }

            if ( echoDetected )
            {
                delete Header;
                delete Frame;
            }
            else
            {
                // Skip gatekeeper for peer connections (we initiated these)
                bool isPeer = false;
                {
                    CPeers *peers = g_Reflector.GetPeers();
                    isPeer = (peers->FindPeer(Ip, PROTOCOL_DCS) != NULL);
                    g_Reflector.ReleasePeers();
                }

                // Peer traffic bypasses MayTransmit by design, but is
                // still subject to the per-callsign loop block — see
                // cysfprotocol.cpp for the rationale. Drop both Header
                // and Frame atomically if the gate rejects.
                bool dropPacket = false;
                if ( isPeer )
                {
                    if ( g_GateKeeper.IsCallsignLoopBlocked(
                            Header->GetMyCallsign(), "DCS peer") )
                    {
                        dropPacket = true;
                    }
                }
                else if ( !g_GateKeeper.MayTransmit(
                            Header->GetMyCallsign(), Ip,
                            PROTOCOL_DCS, Header->GetRpt2Module()) )
                {
                    dropPacket = true;
                }

                if ( !dropPacket )
                {
                    // capture DCS text field (bytes 62-99) for outgoing packet reconstruction
                    if ( Buffer.size() >= 100 )
                    {
                        CClients *clients = g_Reflector.GetClients();
                        CClient *client = clients->FindClient(Ip, PROTOCOL_DCS);
                        if ( client != NULL )
                        {
                            int iModId = g_Reflector.GetModuleIndex(client->GetReflectorModule());
                            if ( iModId >= 0 && iModId < NB_OF_MODULES )
                            {
                                std::lock_guard<std::mutex> lock(m_StreamsCache[iModId].m_Mutex);
                                ::memcpy(m_StreamsCache[iModId].m_uiDcsTail, Buffer.data() + 58, 42);
                                m_StreamsCache[iModId].m_bHasDcsTail = true;
                            }
                        }
                        g_Reflector.ReleaseClients();
                    }

                    OnDvHeaderPacketIn(Header, Ip);

                    if ( !Frame->IsLastPacket() )
                    {
                        OnDvFramePacketIn(Frame, &Ip);
                    }
                    else
                    {
                        OnDvLastFramePacketIn((CDvLastFramePacket *)Frame, &Ip);
                    }
                }
                else
                {
                    delete Header;
                    delete Frame;
                }
            }
        }
        else if ( IsValidConnectPacket(Buffer, &Callsign, &ToLinkModule) )
        {
            // Already connected on same module? Just ACK immediately (retry from client)
            bool alreadyConnected = false;
            {
                CClients *clients = g_Reflector.GetClients();
                CClient *existing = clients->FindClient(Ip, PROTOCOL_DCS, ToLinkModule);
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
                EncodeConnectAckPacket(Callsign, ToLinkModule, &ackBuf);
                m_Socket.Send(ackBuf, Ip);
            }
            else
            {
                std::cout << "DCS connect packet for module " << ToLinkModule << " from " << Callsign << " at " << Ip << std::endl;

                // Queue pending ack with random delay to stagger connection timing
                CDcsPendingAck pendingAck;
                pendingAck.m_Ip = Ip;
                pendingAck.m_Callsign = Callsign;
                pendingAck.m_ToLinkModule = ToLinkModule;
                pendingAck.m_CreatedTime.Now();
                pendingAck.m_DelaySeconds = CClient::GetRandomJitter(CONNECT_ACK_JITTER_MAX);
                pendingAck.m_bAccepted = g_GateKeeper.MayLink(Callsign, Ip, PROTOCOL_DCS) && g_Reflector.IsValidModule(ToLinkModule);
                {
                    std::lock_guard<std::mutex> lock(m_PendingAcksMutex);
                    // Skip if already pending for this IP
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
        else if ( IsValidDisconnectPacket(Buffer, &Callsign) )
        {
            std::cout << "DCS disconnect packet from " << Callsign << " at " << Ip << std::endl;
            
            // find client
            CClients *clients = g_Reflector.GetClients();
            CClient *client = clients->FindClient(Ip, PROTOCOL_DCS);
            if ( client != NULL )
            {
                // remove it
                clients->RemoveClient(client);
                // and acknowledge the disconnect
                EncodeConnectNackPacket(Callsign, ' ', &Buffer);
                m_Socket.Send(Buffer, Ip);
            }
            g_Reflector.ReleaseClients();
        }
        else if ( IsValidKeepAlivePacket(Buffer, &Callsign) )
        {
            //std::cout << "DCS keepalive packet from " << Callsign << " at " << Ip << std::endl;

            // Check if this is from a DCS peer
            CPeers *peers = g_Reflector.GetPeers();
            CPeer *existingPeer = peers->FindPeer(Ip, PROTOCOL_DCS);
            if ( existingPeer != NULL )
            {
                existingPeer->Alive();
            }
            g_Reflector.ReleasePeers();

            // find all clients with that callsign & ip and keep them alive
            CClients *clients = g_Reflector.GetClients();
            int index = -1;
            CClient *client = NULL;
            while ( (client = clients->FindNextClient(Callsign, Ip, PROTOCOL_DCS, &index)) != NULL )
            {
                client->Alive();
            }
            // Also keep peer clients alive by IP address (ignoring port)
            // since peer clients may have been created with different port
            index = -1;
            while ( (client = clients->FindNextClient(PROTOCOL_DCS, &index)) != NULL )
            {
                if ( client->IsPeer() && (client->GetIp().GetAddr() == Ip.GetAddr()) )
                {
                    client->Alive();
                }
            }
            g_Reflector.ReleaseClients();
        }
        else if ( IsIgnorePacket(Buffer) )
        {
            // valid but ignore packet
            //std::cout << "DCS ignored packet from " << Ip << std::endl;
        }
        else
        {
            // Check if this is a connect ACK from a DCS peer we're connecting to
            bool handled = false;
            CPeers *peers = g_Reflector.GetPeers();
            int peerIndex = -1;
            CPeer *peer = NULL;
            while ( (peer = peers->FindNextPeer(PROTOCOL_DCS, &peerIndex)) != NULL )
            {
                CDcsPeer *dcsPeer = dynamic_cast<CDcsPeer *>(peer);
                if ( dcsPeer == NULL )
                {
                    continue;
                }
                if ( dcsPeer->IsConnecting() && (dcsPeer->GetIp().GetAddr() == Ip.GetAddr()) )
                {
                    // Check if this is a valid ACK for our connect request
                    if ( IsValidPeerConnectAckPacket(Buffer) )
                    {
                        std::cout << "DCS peer " << dcsPeer->GetCallsign() << " connected" << std::endl;
                        dcsPeer->SetConnectionState(DCS_PEER_STATE_CONNECTED);
                        dcsPeer->Alive();

                        // Add peer client to global clients list for voice routing
                        CClients *clients = g_Reflector.GetClients();
                        CIp peerClientIp = dcsPeer->GetIp();
                        peerClientIp.SetPort(htons(dcsPeer->GetPort()));
                        CDcsPeerClient *peerClient = new CDcsPeerClient(dcsPeer->GetCallsign(), peerClientIp, dcsPeer->GetLocalModule());
                        peerClient->SetPeer(true);
                        clients->AddClient(peerClient);
                        g_Reflector.ReleaseClients();

                        handled = true;
                        break;
                    }
                }
                // Also check for keepalive responses from connected peers
                else if ( dcsPeer->IsConnected() && (dcsPeer->GetIp().GetAddr() == Ip.GetAddr()) )
                {
                    // Keepalive response from peer - keep it alive
                    dcsPeer->Alive();
                    handled = true;
                    break;
                }
            }
            g_Reflector.ReleasePeers();

            if ( !handled )
            {
                // invalid 519-byte connect attempt — progressive ignore
                if ( Buffer.size() == 519 )
                {
                    uint32 addr = Ip.GetAddr();
                    // cap map size to prevent memory exhaustion from spoofed IPs
                    if ( m_RejectTrackers.find(addr) == m_RejectTrackers.end() &&
                         m_RejectTrackers.size() >= MAX_REJECT_TRACKERS )
                    {
                        // map full — silently drop this IP
                    }
                    else
                    {
                    CRejectTracker &tracker = m_RejectTrackers[addr];

                    // if currently in ignore period, silently drop
                    if ( tracker.m_nStrike > 0 &&
                         tracker.m_IgnoreStart.DurationSinceNow() < tracker.m_dIgnoreDuration )
                    {
                        // still ignoring — do nothing
                    }
                    else
                    {
                        tracker.m_nCount++;

                        if ( tracker.m_nCount <= 3 )
                        {
                            // first 3 attempts: log and send NACK
                            char clientModule = Buffer.data()[8];
                            char requestedModule = Buffer.data()[9];

                            CCallsign callsign;
                            callsign.SetCallsign(Buffer.data(), 8);
                            callsign.SetModule(clientModule);

                            // sanitize for log output — prevent terminal injection
                            char safeCs[9];
                            for ( int j = 0; j < 8; j++ )
                                safeCs[j] = (Buffer.data()[j] >= 0x20 && Buffer.data()[j] < 0x7F) ? (char)Buffer.data()[j] : '.';
                            safeCs[8] = 0;

                            std::cout << "DCS connect rejected from " << Ip
                                      << " - callsign: '" << safeCs << "'"
                                      << " module: " << (IsLetter(clientModule) ? clientModule : '?')
                                      << " requesting: " << (IsLetter(requestedModule) ? requestedModule : '?')
                                      << " (0x" << std::hex << (int)(unsigned char)requestedModule << std::dec << ")";
                            if ( !callsign.IsValid() )
                                std::cout << " [invalid callsign]";
                            if ( !IsLetter(requestedModule) )
                                std::cout << " [invalid module]";
                            std::cout << " (" << tracker.m_nCount << "/3)" << std::endl;

                            CBuffer nackBuffer;
                            EncodeConnectNackPacket(callsign, requestedModule, &nackBuffer);
                            m_Socket.Send(nackBuffer, Ip);
                        }
                        else
                        {
                            // escalate: progressive ignore
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

                            std::cout << "DCS persistent invalid connect from " << Ip
                                      << " - ignoring for " << ignoreMins
                                      << " min (strike " << tracker.m_nStrike << ")" << std::endl;
                        }
                    }
                    } // end of reject tracker cap else
                }
                else
                {
                    std::cout << "DCS unknown packet (" << Buffer.size() << " bytes) from " << Ip << " [";
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
    }
    
    // periodic cleanup of stale reject trackers
    if ( m_LastCleanupCheck.DurationSinceNow() > CLEANUP_CHECK_INTERVAL )
    {
        for ( auto it = m_RejectTrackers.begin(); it != m_RejectTrackers.end(); )
        {
            CRejectTracker &t = it->second;
            if ( t.m_nStrike > 0 && t.m_IgnoreStart.DurationSinceNow() >= t.m_dIgnoreDuration + 600.0 )
            {
                // ignore period expired 10+ minutes ago — remove
                it = m_RejectTrackers.erase(it);
            }
            else if ( t.m_nStrike == 0 && t.m_IgnoreStart.DurationSinceNow() > 600.0 )
            {
                // never escalated and no activity for 10 minutes — remove
                it = m_RejectTrackers.erase(it);
            }
            else
            {
                ++it;
            }
        }
        m_LastCleanupCheck.Now();
    }

    // handle end of streaming timeout
    CheckStreamsTimeout();

}

////////////////////////////////////////////////////////////////////////////////////////
// TX task — runs on separate thread, never blocked by CloseStream

void CDcsProtocol::TxTask(void)
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
    if ( m_LastKeepaliveTime.DurationSinceNow() > DCS_KEEPALIVE_PERIOD )
    {
        HandleKeepalives();
        m_LastKeepaliveTime.Now();
    }

    // handle DCS peer links
    if ( m_LastDcsPeerLinkTime.DurationSinceNow() > DCS_PEER_RECONNECT_PERIOD )
    {
        HandleDcsPeerLinks();
        m_LastDcsPeerLinkTime.Now();
    }

    // handle DCS peer keepalives
    if ( m_LastDcsPeerKeepaliveTime.DurationSinceNow() > DCS_PEER_KEEPALIVE_PERIOD )
    {
        HandleDcsPeerKeepalives();
        HandleDcsPeerConnectionStates();
        m_LastDcsPeerKeepaliveTime.Now();
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// streams helpers

bool CDcsProtocol::OnDvHeaderPacketIn(CDvHeaderPacket *Header, const CIp &Ip)
{
    bool newstream = false;

    // set default suffix if not already set
    if ( !Header->HasMySuffix() )
    {
        Header->SetMySuffix("DCS");
    }

    // find the stream (match on both StreamId and IP to avoid false matches)
    CPacketStream *stream = GetStream(Header->GetStreamId(), &Ip);
    if ( stream == NULL )
    {
        // no stream open yet, open a new one
        CCallsign via(Header->GetRpt1Callsign());
        CCallsign myCallsign;
        CCallsign rpt2Callsign;

        // find this client
        CClients *clients = g_Reflector.GetClients();
        CClient *client = clients->FindClient(Ip, PROTOCOL_DCS);

        // If not found, check if this is from a connected DCS peer
        if ( client == NULL )
        {
            // Look for a peer client with matching IP:port
            // Peers always send from their well-known server port
            int index = -1;
            CClient *testClient = NULL;
            while ( (testClient = clients->FindNextClient(PROTOCOL_DCS, &index)) != NULL )
            {
                if ( testClient->IsPeer() && (testClient->GetIp() == Ip) )
                {
                    client = testClient;
                    break;
                }
            }
        }

        if ( client != NULL )
        {
            // Always use the client's connected module for stream routing
            // The packet header may contain the remote side's module info,
            // but we route based on which module the client connected to
            Header->SetRpt2Module(client->GetReflectorModule());

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

void CDcsProtocol::HandlePendingAcks(void)
{
    // collect ready acks under lock, then process without lock
    std::vector<CDcsPendingAck> readyAcks;
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
    for ( const CDcsPendingAck &ack : readyAcks )
    {
        CBuffer buffer;
        if ( ack.m_bAccepted )
        {
            // acknowledge the request
            EncodeConnectAckPacket(ack.m_Callsign, ack.m_ToLinkModule, &buffer);
            m_Socket.Send(buffer, ack.m_Ip);

            // create the client
            CDcsClient *client = new CDcsClient(ack.m_Callsign, ack.m_Ip, ack.m_ToLinkModule);

            // and append
            g_Reflector.GetClients()->AddClient(client);
            g_Reflector.ReleaseClients();
        }
        else
        {
            // deny the request
            EncodeConnectNackPacket(ack.m_Callsign, ack.m_ToLinkModule, &buffer);
            m_Socket.Send(buffer, ack.m_Ip);

            std::cout << "DCS connect nack sent to " << ack.m_Callsign << " at " << ack.m_Ip << std::endl;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// client cache helpers

void CDcsProtocol::RefreshClientCache(int iModId)
{
    // Check freshness under cache lock, release before acquiring Clients lock
    // to prevent cache→Clients lock inversion
    {
        std::lock_guard<std::mutex> lock(m_ClientCache[iModId].m_Mutex);
        if ( m_ClientCache[iModId].m_bInitialized &&
             m_ClientCache[iModId].m_LastRefresh.DurationSinceNow() < DCS_CLIENT_CACHE_REFRESH_INTERVAL )
        {
            return;  // Cache is still fresh
        }
    }

    // Scan clients WITHOUT holding cache lock
    char moduleId = 'A' + iModId;
    std::vector<CIp> freshIps;

    CClients *clients = g_Reflector.GetClients();
    int index = -1;
    CClient *client = NULL;
    while ( (client = clients->FindNextClient(PROTOCOL_DCS, &index)) != NULL )
    {
        // Only cache non-master, non-peer clients on this module
        if ( !client->IsAMaster() && !client->IsPeer() && (client->GetReflectorModule() == moduleId) )
        {
            freshIps.push_back(client->GetIp());
        }
    }
    g_Reflector.ReleaseClients();

    // Write results under cache lock
    std::lock_guard<std::mutex> lock(m_ClientCache[iModId].m_Mutex);
    m_ClientCache[iModId].m_ClientIps.swap(freshIps);
    m_ClientCache[iModId].m_LastRefresh.Now();
    m_ClientCache[iModId].m_bInitialized = true;
}

void CDcsProtocol::SendToModuleClients(int iModId, const CBuffer &buffer, bool hasOwner, const CIp &ownerIp)
{
    // Refresh cache if needed
    RefreshClientCache(iModId);

    // Send to cached clients (cache mutex is TX-thread-only, safe to hold during sends)
    std::lock_guard<std::mutex> lock(m_ClientCache[iModId].m_Mutex);
    for ( const CIp &ip : m_ClientCache[iModId].m_ClientIps )
    {
        if ( hasOwner && ownerIp == ip )
            continue;
        m_Socket.SendVoice(buffer, ip);
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// queue helper

void CDcsProtocol::HandleQueue(void)
{
    // Phase 1A: Dequeue all packets with lock held, then release lock immediately
    std::vector<CPacket *> packets;
    packets.reserve(100);  // Pre-allocate for typical queue sizes

    m_Queue.Lock();
    while ( !m_Queue.empty() )
    {
        packets.push_back(m_Queue.front());
        m_Queue.pop();
    }
    m_Queue.Unlock();

    // Process packets without holding queue lock
    for ( size_t i = 0; i < packets.size(); i++ )
    {
        CPacket *packet = packets[i];

        // get our sender's id and validate bounds
        int iModId = g_Reflector.GetModuleIndex(packet->GetModuleId());
        if ( iModId < 0 || iModId >= NB_OF_MODULES )
        {
            // Invalid module - skip this packet
            delete packet;
            continue;
        }

        // check if it's header and update cache
        if ( packet->IsDvHeader() )
        {
            // lock order: slot mutex → stream lock (never reverse)
            std::lock_guard<std::mutex> lock(m_StreamsCache[iModId].m_Mutex);
            m_StreamsCache[iModId].m_dvHeader = CDvHeaderPacket((const CDvHeaderPacket &)*packet);
            m_StreamsCache[iModId].m_iSeqCounter = 0;
            m_StreamsCache[iModId].m_uiOutboundStreamId = packet->GetStreamId();
            // capture stream owner IP for send-time exclusion
            m_StreamsCache[iModId].m_bHasOwner = false;
            m_StreamsCache[iModId].m_bHasDcsTail = false;
            CPacketStream *stream = g_Reflector.GetStream(packet->GetModuleId());
            if ( stream != NULL )
            {
                stream->Lock();
                if ( stream->IsOpen() && stream->GetStreamId() == packet->GetStreamId() )
                {
                    const CIp *ownerIp = stream->GetOwnerIp();
                    if ( ownerIp != NULL )
                    {
                        m_StreamsCache[iModId].m_OwnerIp = *ownerIp;
                        m_StreamsCache[iModId].m_bHasOwner = true;
                    }
                }
                stream->Unlock();
            }
        }
        else
        {
            // encode under slot lock, snapshot owner exclusion, send without lock
            CBuffer buffer;
            bool hasOwner;
            CIp ownerIp;
            uint16 outboundSid;
            {
                std::lock_guard<std::mutex> lock(m_StreamsCache[iModId].m_Mutex);
                // snapshot owner exclusion fields for send loop
                hasOwner = m_StreamsCache[iModId].m_bHasOwner;
                ownerIp = m_StreamsCache[iModId].m_OwnerIp;
                outboundSid = m_StreamsCache[iModId].m_uiOutboundStreamId;

                if ( packet->IsLastPacket() )
                {
                    EncodeDvLastPacket(
                                       m_StreamsCache[iModId].m_dvHeader,
                                       (const CDvFramePacket &)*packet,
                                       m_StreamsCache[iModId].m_iSeqCounter++,
                                       &buffer);
                }
                else if ( packet->IsDvFrame() )
                {
                    EncodeDvPacket(
                                   m_StreamsCache[iModId].m_dvHeader,
                                   (const CDvFramePacket &)*packet,
                                   m_StreamsCache[iModId].m_iSeqCounter++,
                                   &buffer);
                }

                // restore DCS tail from cached source data (DCS source only —
                // cross-mode sources use EncodeDvPacket's incrementing seq counter)
                if ( buffer.size() >= 100 && m_StreamsCache[iModId].m_bHasDcsTail )
                {
                    ::memcpy(buffer.data() + 58, m_StreamsCache[iModId].m_uiDcsTail, 42);
                }
            } // slot lock released before network I/O

            // send it (no slot lock held)
            if ( buffer.size() > 0 )
            {
                SendToModuleClients(iModId, buffer, hasOwner, ownerIp);

                // Also send to connected DCS peers if this is their linked module
                // But skip the peer that originated this stream to prevent loops
                // Use snapshotted owner data (captured under slot lock above)
                CPeers *peers = g_Reflector.GetPeers();
                int peerIndex = -1;
                CPeer *peer = NULL;
                while ( (peer = peers->FindNextPeer(PROTOCOL_DCS, &peerIndex)) != NULL )
                {
                    CDcsPeer *dcsPeer = dynamic_cast<CDcsPeer *>(peer);
                    if ( dcsPeer == NULL || !dcsPeer->IsConnected() )
                        continue;

                    // Check if this packet's module matches the peer's local module
                    if ( packet->GetModuleId() != dcsPeer->GetLocalModule() )
                        continue;

                    // Skip if this stream originated from this peer (prevent mirror)
                    // Full IP:port match — peers always send from their well-known port
                    CIp peerIp = dcsPeer->GetIp();
                    peerIp.SetPort(htons(dcsPeer->GetPort()));
                    if ( hasOwner && ownerIp == peerIp )
                        continue;
                    m_Socket.SendVoice(buffer, peerIp);
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

void CDcsProtocol::HandleKeepalives(void)
{
    // DCS protocol sends and monitors keepalives packets using per-client timing
    // even if the client is currently streaming

    // build generic reflector keepalive once (9 bytes: reflector callsign + null)
    CBuffer keepalive1;
    EncodeKeepAlivePacket(&keepalive1);

    // iterate on clients
    CClients *clients = g_Reflector.GetClients();
    int index = -1;
    CClient *client = NULL;
    std::vector<CClient *> toRemove;
    while ( (client = clients->FindNextClient(PROTOCOL_DCS, &index)) != NULL )
    {
        // check if it's time to send keepalive to this client
        if ( client->IsKeepaliveDue() )
        {
            // encode client's specific keepalive packet (22 bytes)
            CBuffer keepalive2;
            EncodeKeepAlivePacket(&keepalive2, client);

            // send both keepalives
            m_Socket.Send(keepalive1, client->GetIp());
            m_Socket.Send(keepalive2, client->GetIp());

            // reset timer and schedule next keepalive at regular interval
            client->ResetKeepaliveTimer();
            client->ScheduleNextKeepalive(DCS_KEEPALIVE_PERIOD);

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
                EncodeDisconnectPacket(&disconnect, client);
                m_Socket.Send(disconnect, client->GetIp());

                // collect for removal after loop
                std::cout << "DCS client " << client->GetCallsign() << " keepalive timeout" << std::endl;
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

bool CDcsProtocol::IsValidConnectPacket(const CBuffer &Buffer, CCallsign *callsign, char *reflectormodule)
{
    bool valid = false;
    if ( Buffer.size() == 519 )
    {
        callsign->SetCallsign(Buffer.data(), 8);
        callsign->SetModule(Buffer.data()[8]);
        *reflectormodule = Buffer.data()[9];
        valid = (callsign->IsValid() && IsLetter(*reflectormodule));
    }
    return valid;
}

bool CDcsProtocol::IsValidDisconnectPacket(const CBuffer &Buffer, CCallsign *callsign)
{
    bool valid = false;
    if ((Buffer.size() == 11) && (Buffer.data()[9] == ' '))
    {
        callsign->SetCallsign(Buffer.data(), 8);
        callsign->SetModule(Buffer.data()[8]);
        valid = callsign->IsValid();
    }
    else if ((Buffer.size() == 19) && (Buffer.data()[9] == ' ') && (Buffer.data()[10] == 0x00))
    {
        callsign->SetCallsign(Buffer.data(), 8);
        callsign->SetModule(Buffer.data()[8]);
        valid = callsign->IsValid();
    }
   return valid;
}

bool CDcsProtocol::IsValidKeepAlivePacket(const CBuffer &Buffer, CCallsign *callsign)
{
    bool valid = false;
    if ( (Buffer.size() == 9) || (Buffer.size() == 17) || (Buffer.size() == 15) || (Buffer.size() == 19) || (Buffer.size() == 22) )
    {
        callsign->SetCallsign(Buffer.data(), 8);
        valid = callsign->IsValid();
    }
    return valid;
}

bool CDcsProtocol::IsValidDvPacket(const CBuffer &Buffer, CDvHeaderPacket **header, CDvFramePacket **frame)
{
    uint8 tag[] = { '0','0','0','1' };
    
    bool valid = false;
    *header = NULL;
    *frame = NULL;
    
    if ( (Buffer.size() >= 100) && (Buffer.Compare(tag, sizeof(tag)) == 0) )
    {
        // get the header
        *header = new CDvHeaderPacket((struct dstar_header *)&(Buffer.data()[4]),
                                     *((uint16 *)&(Buffer.data()[43])), 0x80);
        
        // get the frame
        if ( ((Buffer.data()[45]) & 0x40) != 0 )
        {
            // it's the last frame
            *frame = new CDvLastFramePacket((struct dstar_dvframe *)&(Buffer.data()[46]),
                                             *((uint16 *)&(Buffer.data()[43])), Buffer.data()[45] & 0x1F);
        }
        else
        {
            // it's a regular DV frame
            *frame = new CDvFramePacket((struct dstar_dvframe *)&(Buffer.data()[46]),
                                         *((uint16 *)&(Buffer.data()[43])), Buffer.data()[45]);
        }
        
        // check validity of packets
        if ( !((*header)->IsValid() && (*frame)->IsValid()) )
        {
            delete *header;
            delete *frame;
            *header = NULL;
            *frame = NULL;
        }
        else
        {
            valid = true;
        }
    }
    // done
    return valid;
}

bool CDcsProtocol::IsIgnorePacket(const CBuffer &Buffer)
{
    bool valid = false;
    uint8 tag[] = { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, };
    
    if ( Buffer.size() == 15 )
    {
        valid = (Buffer.Compare(tag, sizeof(tag)) == 0);
    }
    return valid;
}


////////////////////////////////////////////////////////////////////////////////////////
// packet encoding helpers

void CDcsProtocol::EncodeKeepAlivePacket(CBuffer *Buffer)
{
    Buffer->Set(GetReflectorCallsign());
}

void CDcsProtocol::EncodePeerKeepAlivePacket(CBuffer *Buffer, CDcsPeer *Peer)
{
    // DCS peer keepalive format (17 bytes):
    // Our callsign (8 bytes) + remote callsign (8 bytes) + null (1 byte)
    uint8 cs[CALLSIGN_LEN];

    GetReflectorCallsign().GetCallsign(cs);
    cs[CALLSIGN_LEN-1] = Peer->GetLocalModule();  // Our module
    Buffer->Set(cs, CALLSIGN_LEN);

    Peer->GetCallsign().GetCallsign(cs);
    cs[CALLSIGN_LEN-1] = Peer->GetRemoteModule();  // Their module
    Buffer->Append(cs, CALLSIGN_LEN);

    Buffer->Append((uint8)0x00);
}

void CDcsProtocol::EncodeKeepAlivePacket(CBuffer *Buffer, CClient *Client)
{
    uint8 tag[] = { 0x0A,0x00,0x20,0x20 };
    
    Buffer->Set((uint8 *)(const char *)GetReflectorCallsign(), CALLSIGN_LEN-1);
    Buffer->Append((uint8)Client->GetReflectorModule());
    Buffer->Append((uint8)' ');
    Buffer->Append((uint8 *)(const char *)Client->GetCallsign(), CALLSIGN_LEN-1);
    Buffer->Append((uint8)Client->GetModule());
    Buffer->Append((uint8)Client->GetModule());
    Buffer->Append(tag, sizeof(tag));
}

void CDcsProtocol::EncodeConnectAckPacket(const CCallsign &Callsign, char ReflectorModule, CBuffer *Buffer)
{
    uint8 tag[] = { 'A','C','K',0x00 };
    uint8 cs[CALLSIGN_LEN];
    
    Callsign.GetCallsign(cs);
    Buffer->Set(cs, CALLSIGN_LEN-1);
    Buffer->Append((uint8)' ');
    Buffer->Append((uint8)Callsign.GetModule());
    Buffer->Append((uint8)ReflectorModule);
    Buffer->Append(tag, sizeof(tag));
}

void CDcsProtocol::EncodeConnectNackPacket(const CCallsign &Callsign, char ReflectorModule, CBuffer *Buffer)
{
    uint8 tag[] = { 'N','A','K',0x00 };
    uint8 cs[CALLSIGN_LEN];
    
    Callsign.GetCallsign(cs);
    Buffer->Set(cs, CALLSIGN_LEN-1);
    Buffer->Append((uint8)' ');
    Buffer->Append((uint8)Callsign.GetModule());
    Buffer->Append((uint8)ReflectorModule);
    Buffer->Append(tag, sizeof(tag));
}

void CDcsProtocol::EncodeDisconnectPacket(CBuffer *Buffer, CClient *Client)
{
    Buffer->Set((uint8 *)(const char *)Client->GetCallsign(), CALLSIGN_LEN-1);
    Buffer->Append((uint8)' ');
    Buffer->Append((uint8)Client->GetModule());
    Buffer->Append((uint8)0x00);
    Buffer->Append((uint8 *)(const char *)GetReflectorCallsign(), CALLSIGN_LEN-1);
    Buffer->Append((uint8)' ');
    Buffer->Append((uint8)0x00);
}

void CDcsProtocol::EncodeDvPacket(const CDvHeaderPacket &Header, const CDvFramePacket &DvFrame, uint32 iSeq, CBuffer *Buffer) const
{
    uint8 tag[] = { '0','0','0','1' };
    struct dstar_header DstarHeader;

    Header.ConvertToDstarStruct(&DstarHeader);

    Buffer->Set(tag, sizeof(tag));
    Buffer->Append((uint8 *)&DstarHeader, sizeof(struct dstar_header) - sizeof(uint16));
    Buffer->Append(DvFrame.GetStreamId());
    Buffer->Append((uint8)(DvFrame.GetPacketId() % 21));
    Buffer->Append((uint8 *)DvFrame.GetAmbe(), AMBE_SIZE);
    Buffer->Append((uint8 *)DvFrame.GetDvData(), DVDATA_SIZE);
    Buffer->Append((uint8)((iSeq >> 0) & 0xFF));
    Buffer->Append((uint8)((iSeq >> 8) & 0xFF));
    Buffer->Append((uint8)((iSeq >> 16) & 0xFF));
    Buffer->Append((uint8)0x01);           // [61] DCS format marker
    Buffer->Append((uint8)0x00, 38);       // [62-99] text field default fill

    // DCS format compliance: byte 63 MUST be 0x21 (text field separator)
    // and bytes 64-83 carry the 20-char text message. Native DCS voice
    // frames captured on the wire always have these set; xlxd's cross-
    // mode egress used to leave bytes 62-99 zeroed, and at least one
    // strict downstream (Icom g2_link → RP2C) rejects such frames as
    // malformed DCS — native plays, cross-mode silent.
    //
    // Written unconditionally so every DCS voice frame we emit is well-
    // formed regardless of source protocol. On the DCS-source path,
    // HandleQueue's m_bHasDcsTail overwrite later replaces bytes 58-99
    // verbatim with the cached source tail — that tail has 0x21 at the
    // matching offset (CDcsStreamCacheItem constructor), so format
    // compliance is preserved; text content gets replaced with the
    // source radio's actual slow-data text.
    //
    // ReplaceAt is the idiomatic codebase method for in-place byte
    // patching — it bounds-checks and avoids raw data() manipulation.
    char text[DSTAR_SLOW_DATA_TEXT_LEN];
    CDStarSlowData::ComposeText(Header, text);
    Buffer->ReplaceAt(63, (uint8)0x21);
    Buffer->ReplaceAt(64, (const uint8 *)text, DSTAR_SLOW_DATA_TEXT_LEN);
}

void CDcsProtocol::EncodeDvLastPacket(const CDvHeaderPacket &Header, const CDvFramePacket &DvFrame, uint32 iSeq, CBuffer *Buffer) const
{
    EncodeDvPacket(Header, DvFrame, iSeq, Buffer);
    (Buffer->data())[45] |= 0x40;

    // Spec-compliant D-Star EoT pattern in the AMBE (bytes 46-54) and
    // DVDATA (bytes 55-57) fields. DExtra and DPlus achieve this with a
    // static tag2[] array on egress (cdextraprotocol.cpp:1083,
    // cdplusprotocol.cpp:1189) — DCS historically only set the 0x40 EoT
    // bit and left the AMBE/DVDATA fields carrying whatever bytes the
    // upstream pipeline produced.
    //
    // For native D-Star → DCS this was already correct (the source
    // radio's spec-compliant EoT bytes flowed through unchanged), so the
    // override below is a no-op rewrite of identical bytes. For cross-
    // mode → DCS the AMBE field had non-deterministic transcoder output
    // (AMBEd's response to feeding the source's EoT pattern through DVSI
    // as if it were audio) and the DVDATA field carried mid-cycle slow-
    // data content (header-sync element fragment or filler), neither
    // matching the spec EoT marker. The override replaces both with the
    // standard pattern.
    //
    // Bytes:
    //   46-54  AMBE     = 55 C8 7A 00 00 00 00 00 00  (D-Star RF burst
    //                    termination M-sequence + 6 silence bytes)
    //   55-57  DVDATA   = 25 1A C6  (wire bytes; plaintext 55 55 55,
    //                    the slow-data idle filler / EoT pattern)
    //
    // The 0x40 EoT bit on byte 45 (set above) remains the protocol-level
    // signal DCS clients gate on; this override brings the AMBE/DVDATA
    // fields into JARL spec compliance for strict-decoder downstreams.
    static const uint8 eotAmbe[]   = { 0x55, 0xC8, 0x7A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    static const uint8 eotDvData[] = { 0x25, 0x1A, 0xC6 };
    Buffer->ReplaceAt(46, eotAmbe,   (int)sizeof(eotAmbe));
    Buffer->ReplaceAt(55, eotDvData, (int)sizeof(eotDvData));
}

////////////////////////////////////////////////////////////////////////////////////////
// DCS peer helpers

void CDcsProtocol::HandleDcsPeerLinks(void)
{
    // Snapshot the peer list under the GK lock, then release BEFORE acquiring
    // Peers or Clients locks. This prevents ABBA deadlock: the RX thread can
    // hold Clients → GK (via MayLink), so we must not hold GK while acquiring
    // Clients or Peers here.
    std::vector<CCallsignListItem> peerListSnapshot;
    {
        CPeerCallsignList *peerList = g_GateKeeper.GetPeerList();
        peerListSnapshot.assign(peerList->begin(), peerList->end());
        g_GateKeeper.ReleasePeerList();
    }

    CPeers *peers = g_Reflector.GetPeers();

    // Check if all our connected DCS peers are still listed by gatekeeper
    // If not, send disconnect and clean up
    int peerIndex = -1;
    CPeer *peer = NULL;
    std::vector<CPeer *> peersToRemove;
    while ( (peer = peers->FindNextPeer(PROTOCOL_DCS, &peerIndex)) != NULL )
    {
        // Check if peer is still in the snapshot
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
            CDcsPeer *dcsPeer = dynamic_cast<CDcsPeer *>(peer);
            std::cout << "DCS peer " << peer->GetCallsign() << " removed - no longer in peer list, sending disconnect" << std::endl;

            // Find and remove the associated peer client, and send disconnect
            CClients *clients = g_Reflector.GetClients();
            int clientIndex = -1;
            CClient *client = NULL;
            while ( (client = clients->FindNextClient(PROTOCOL_DCS, &clientIndex)) != NULL )
            {
                if ( client->IsPeer() && (client->GetIp().GetAddr() == peer->GetIp().GetAddr()) )
                {
                    // Send disconnect packet
                    CBuffer disconnect;
                    EncodeDisconnectPacket(&disconnect, client);
                    if ( dcsPeer != NULL )
                    {
                        CIp peerIp = dcsPeer->GetIp();
                        peerIp.SetPort(htons(dcsPeer->GetPort()));
                        m_Socket.Send(disconnect, peerIp);
                    }
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

    // Iterate through peer list snapshot and create/connect DCS peers
    for ( size_t i = 0; i < peerListSnapshot.size(); i++ )
    {
        CCallsignListItem &item = peerListSnapshot[i];

        // Only process DCS peers
        if ( !IsDcsPeerCallsign(item.GetCallsign()) )
            continue;

        // Check if peer already exists
        CDcsPeer *existingPeer = NULL;
        peerIndex = -1;
        while ( (peer = peers->FindNextPeer(PROTOCOL_DCS, &peerIndex)) != NULL )
        {
            if ( peer->GetCallsign().HasSameCallsign(item.GetCallsign()) )
            {
                existingPeer = dynamic_cast<CDcsPeer *>(peer);
                break;
            }
        }

        if ( existingPeer == NULL )
        {
            // Create new peer
            CIp peerIp = item.GetIp();
            if ( peerIp.GetAddr() == 0 )
            {
                // IP not resolved yet, skip
                continue;
            }

            // Get modules and port from list item
            char modules[3];
            ::strncpy(modules, item.GetModules(), 2);
            modules[2] = '\0';

            CVersion version(1, 0, 0);
            CDcsPeer *newPeer = new CDcsPeer(item.GetCallsign(), peerIp, modules, version);
            newPeer->SetPort(item.GetPort() != 0 ? item.GetPort() : DCS_PORT);

            // Add to peers list
            peers->AddPeer(newPeer);

            std::cout << "DCS peer " << item.GetCallsign() << " created, connecting to module "
                      << newPeer->GetRemoteModule() << " on " << peerIp << ":" << newPeer->GetPort() << std::endl;
        }
    }

    g_Reflector.ReleasePeers();
}

void CDcsProtocol::HandleDcsPeerKeepalives(void)
{
    // Send keepalives to all connected DCS peers
    CPeers *peers = g_Reflector.GetPeers();
    int peerIndex = -1;
    CPeer *peer = NULL;
    while ( (peer = peers->FindNextPeer(PROTOCOL_DCS, &peerIndex)) != NULL )
    {
        CDcsPeer *dcsPeer = dynamic_cast<CDcsPeer *>(peer);
        if ( dcsPeer == NULL )
        {
            continue;
        }

        // Connected peers are kept alive via the normal client keepalive path
        // in HandleKeepalives(). Only act on disconnected peers here.
        if ( dcsPeer->GetConnectionState() == DCS_PEER_STATE_DISCONNECTED )
        {
            // Not connected - send connect packet
            CBuffer connect;
            EncodePeerConnectPacket(&connect, GetReflectorCallsign(), dcsPeer->GetLocalModule(), dcsPeer->GetRemoteModule());

            CIp peerIp = dcsPeer->GetIp();
            peerIp.SetPort(htons(dcsPeer->GetPort()));
            m_Socket.Send(connect, peerIp);

            // Set state to connecting
            dcsPeer->SetConnectionState(DCS_PEER_STATE_CONNECTING);
            dcsPeer->ResetConnectTimer();

            std::cout << "DCS peer " << dcsPeer->GetCallsign() << " connecting to " << peerIp
                      << ":" << ntohs(peerIp.GetPort())
                      << " (local " << dcsPeer->GetLocalModule() << " -> remote " << dcsPeer->GetRemoteModule() << ")" << std::endl;
        }
    }
    g_Reflector.ReleasePeers();
}

void CDcsProtocol::HandleDcsPeerConnectionStates(void)
{
    // Check for connection timeouts and dead peers
    CPeers *peers = g_Reflector.GetPeers();
    int peerIndex = -1;
    CPeer *peer = NULL;
    std::vector<CPeer *> peersToRemove;
    while ( (peer = peers->FindNextPeer(PROTOCOL_DCS, &peerIndex)) != NULL )
    {
        CDcsPeer *dcsPeer = dynamic_cast<CDcsPeer *>(peer);
        if ( dcsPeer == NULL )
        {
            continue;
        }

        if ( dcsPeer->IsConnecting() )
        {
            // Check for connect timeout
            if ( dcsPeer->GetConnectDuration() > DCS_PEER_CONNECT_TIMEOUT )
            {
                std::cout << "DCS peer " << dcsPeer->GetCallsign() << " connect timeout, retrying..." << std::endl;
                dcsPeer->SetConnectionState(DCS_PEER_STATE_DISCONNECTED);
            }
        }
        else if ( dcsPeer->IsConnected() )
        {
            // Check if peer is still alive
            if ( !dcsPeer->IsAlive() )
            {
                std::cout << "DCS peer " << dcsPeer->GetCallsign() << " keepalive timeout, disconnecting..." << std::endl;

                // Lock order: Peers → Clients (consistent across all peer management paths)
                // Remove peer client from clients list
                CClients *clients = g_Reflector.GetClients();
                int clientIndex = -1;
                CClient *client = NULL;
                while ( (client = clients->FindNextClient(PROTOCOL_DCS, &clientIndex)) != NULL )
                {
                    if ( client->IsPeer() && (client->GetIp().GetAddr() == dcsPeer->GetIp().GetAddr()) )
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
        }
    }
    for ( size_t i = 0; i < peersToRemove.size(); i++ )
    {
        peers->RemovePeer(peersToRemove[i]);
    }
    g_Reflector.ReleasePeers();
}

bool CDcsProtocol::IsDcsPeerCallsign(const CCallsign &callsign) const
{
    // DCS peer callsigns start with "DCS" followed by a digit
    const char *cs = (const char *)callsign;
    return (::strncmp(cs, "DCS", 3) == 0) && (cs[3] >= '0') && (cs[3] <= '9');
}

void CDcsProtocol::EncodePeerConnectPacket(CBuffer *Buffer, const CCallsign &Callsign, char LocalModule, char RemoteModule)
{
    // DCS connect packet is 519 bytes
    // Format: callsign(8) + clientModule(1) + reflectorModule(1) + zeros(509)

    uint8 cs[CALLSIGN_LEN];
    Callsign.GetCallsign(cs);

    Buffer->Set(cs, CALLSIGN_LEN);  // 8 bytes callsign
    Buffer->Append((uint8)LocalModule);  // Client module (our local module)
    Buffer->Append((uint8)RemoteModule); // Reflector module we want to connect to

    // Pad with zeros to 519 bytes
    for ( int i = 10; i < 519; i++ )
    {
        Buffer->Append((uint8)0x00);
    }
}

bool CDcsProtocol::IsValidPeerConnectAckPacket(const CBuffer &Buffer)
{
    // DCS ACK format: callsign(7) + ' ' + clientModule + reflectorModule + "ACK\0"
    // Total: 14 bytes

    if ( Buffer.size() != 14 )
        return false;

    // Check for ACK tag
    if ( ::memcmp(&Buffer.data()[10], "ACK", 3) != 0 )
        return false;

    // Check null terminator
    if ( Buffer.data()[13] != 0x00 )
        return false;

    return true;
}
