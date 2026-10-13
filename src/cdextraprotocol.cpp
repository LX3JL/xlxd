//
//  cdextraprotocol.cpp
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
#include "cdextraclient.h"
#include "cdextraprotocol.h"
#include "creflector.h"
#include "cgatekeeper.h"

// periodic cleanup interval for stale reject trackers
#define CLEANUP_CHECK_INTERVAL  5.0     // seconds between cleanup sweeps

////////////////////////////////////////////////////////////////////////////////////////
// operation

bool CDextraProtocol::Init(void)
{
    bool ok;

    // base class
    ok = CProtocol::Init();

    // update the reflector callsign
    m_ReflectorCallsign.PatchCallsign(0, (const uint8 *)"XRF", 3);

    // create our socket
    ok &= m_Socket.Open(DEXTRA_PORT);
    if ( !ok )
    {
        std::cout << "Error opening socket on port UDP" << DEXTRA_PORT << " on ip " << g_Reflector.GetListenIp() << std::endl;
    }

    // update time
    m_LastKeepaliveTime.Now();
    m_LastDextraPeerLinkTime.Now();
    m_LastDextraPeerKeepaliveTime.Now();

    // done
    return ok;
}

////////////////////////////////////////////////////////////////////////////////////////
// task

void CDextraProtocol::RxTask(void)
{
    CBuffer             Buffer;
    CIp                 Ip;
    CCallsign           Callsign;
    char                ToLinkModule;
    int                 ProtRev;
    CDvHeaderPacket     *Header;
    CDvFramePacket      *Frame;
    CDvLastFramePacket  *LastFrame;
    
    // any incoming packet ?
    if ( m_Socket.Receive(&Buffer, &Ip, 20) != -1 )
    {
        // crack the packet
        if ( (Frame = IsValidDvFramePacket(Buffer)) != NULL )
        {
            // check if this is an echo of our own outbound stream
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
                    isPeer = (peers->FindPeer(Ip, PROTOCOL_DEXTRA) != NULL);
                    g_Reflector.ReleasePeers();
                }

                // Peer traffic bypasses MayTransmit by design, but is
                // still subject to the per-callsign loop block — see
                // cysfprotocol.cpp for the rationale.
                if ( isPeer )
                {
                    if ( g_GateKeeper.IsCallsignLoopBlocked(
                            Header->GetMyCallsign(), "DExtra peer") )
                    {
                        delete Header;
                        Header = NULL;
                    }
                }
                else if ( !g_GateKeeper.MayTransmit(
                            Header->GetMyCallsign(), Ip,
                            PROTOCOL_DEXTRA, Header->GetRpt2Module()) )
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
        else if ( IsValidConnectPacket(Buffer, &Callsign, &ToLinkModule, &ProtRev) )
        {
            // Check if this is an ack from an XRF peer we're connecting to
            CPeers *peers = g_Reflector.GetPeers();
            CPeer *existingPeer = peers->FindPeer(Ip, PROTOCOL_DEXTRA);

            if ( existingPeer != NULL )
            {
                CDextraPeer *dextraPeer = dynamic_cast<CDextraPeer *>(existingPeer);
                if ( dextraPeer != NULL && dextraPeer->GetConnectionState() == DEXTRA_PEER_STATE_CONNECTING )
                {
                    // This is the connect ack - verify it's for us
                    if ( IsValidConnectAckPacket(Buffer) )
                    {
                        std::cout << "DExtra connect ack from peer " << dextraPeer->GetCallsign()
                                  << " - connected (local " << dextraPeer->GetLocalModule()
                                  << " -> remote " << dextraPeer->GetRemoteModule() << ")" << std::endl;
                        dextraPeer->SetConnectionState(DEXTRA_PEER_STATE_CONNECTED);
                        dextraPeer->Alive();

                        // Add peer client to global clients list for voice routing
                        CClients *clients = g_Reflector.GetClients();
                        CIp peerClientIp = dextraPeer->GetIp();
                        peerClientIp.SetPort(htons(dextraPeer->GetPort()));
                        CDextraPeerClient *peerClient = new CDextraPeerClient(dextraPeer->GetCallsign(), peerClientIp, dextraPeer->GetLocalModule());
                        peerClient->SetPeer(true);
                        clients->AddClient(peerClient);
                        g_Reflector.ReleaseClients();
                    }
                }
                g_Reflector.ReleasePeers();
            }
            else
            {
                g_Reflector.ReleasePeers();

                // Already connected on same module? Just ACK immediately (retry from client)
                bool alreadyConnected = false;
                {
                    CClients *clients = g_Reflector.GetClients();
                    CClient *existing = clients->FindClient(Ip, PROTOCOL_DEXTRA, ToLinkModule);
                    if ( existing != NULL )
                    {
                        existing->Alive();
                        alreadyConnected = true;
                    }
                    g_Reflector.ReleaseClients();
                }

                if ( alreadyConnected )
                {
                    // Build and send ack immediately
                    CBuffer ackBuf;
                    ackBuf.Set((uint8 *)(const char *)Callsign, CALLSIGN_LEN);
                    ackBuf.Append((uint8)Callsign.GetModule());
                    ackBuf.Append((uint8)ToLinkModule);
                    ackBuf.Append((uint8)0);
                    EncodeConnectAckPacket(&ackBuf, ProtRev);
                    m_Socket.Send(ackBuf, Ip);
                }
                else
                {
                    std::cout << "DExtra connect packet for module " << ToLinkModule << " from " << Callsign << " at " << Ip << " rev " << ProtRev << std::endl;

                    // Queue pending ack with random delay to stagger connection timing
                    CDextraPendingAck pendingAck;
                    pendingAck.m_Ip = Ip;
                    pendingAck.m_Callsign = Callsign;
                    pendingAck.m_ToLinkModule = ToLinkModule;
                    pendingAck.m_ProtRev = ProtRev;
                    pendingAck.m_CreatedTime.Now();
                    pendingAck.m_DelaySeconds = CClient::GetRandomJitter(CONNECT_ACK_JITTER_MAX);
                    pendingAck.m_bAccepted = g_GateKeeper.MayLink(Callsign, Ip, PROTOCOL_DEXTRA) && g_Reflector.IsValidModule(ToLinkModule);
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
        }
        else if ( IsValidDisconnectPacket(Buffer, &Callsign) )
        {
            std::cout << "DExtra disconnect packet from " << Callsign << " at " << Ip << std::endl;
            
            // find client & remove it
            CClients *clients = g_Reflector.GetClients();
            CClient *client = clients->FindClient(Ip, PROTOCOL_DEXTRA);
            if ( client != NULL )
            {
                // ack disconnect packet
                if ( client->GetProtocolRevision() == 1 )
                {
                    EncodeDisconnectedPacket(&Buffer);
                    m_Socket.Send(Buffer, Ip);
                }
                else if ( client->GetProtocolRevision() == 2 )
                {
                    m_Socket.Send(Buffer, Ip);
                }
               // and remove it
                clients->RemoveClient(client);
            }
            g_Reflector.ReleaseClients();
        }
        else if ( IsValidKeepAlivePacket(Buffer, &Callsign) )
        {
            //std::cout << "DExtra keepalive packet from " << Callsign << " at " << Ip << std::endl;

            // Check if this is from an XRF peer
            CPeers *peers = g_Reflector.GetPeers();
            CPeer *existingPeer = peers->FindPeer(Ip, PROTOCOL_DEXTRA);
            if ( existingPeer != NULL )
            {
                existingPeer->Alive();
            }
            g_Reflector.ReleasePeers();

            // find all clients with that callsign & ip and keep them alive
            CClients *clients = g_Reflector.GetClients();
            int index = -1;
            CClient *client = NULL;
            while ( (client = clients->FindNextClient(Callsign, Ip, PROTOCOL_DEXTRA, &index)) != NULL )
            {
               client->Alive();
            }
            // Also keep peer clients alive by IP address (ignoring port)
            // since peer clients may have been created with different port
            index = -1;
            while ( (client = clients->FindNextClient(PROTOCOL_DEXTRA, &index)) != NULL )
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
            // Check for malformed 11-byte connect packets that failed validation
            if ( Buffer.size() == 11 && Buffer.data()[9] != ' ' )
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
                            cs[j] = (Buffer.data()[j] >= 0x20 && Buffer.data()[j] < 0x7F) ? (char)Buffer.data()[j] : '.';
                        cs[8] = 0;
                        char clientModule = Buffer.data()[8];
                        char requestedModule = Buffer.data()[9];

                        CCallsign callsign;
                        callsign.SetCallsign(Buffer.data(), 8);

                        std::cout << "DExtra connect rejected from " << Ip
                                  << " - callsign: '" << cs << "'"
                                  << " module: " << (IsLetter(clientModule) ? clientModule : '?')
                                  << " requesting: " << (IsLetter(requestedModule) ? requestedModule : '?')
                                  << " (0x" << std::hex << (int)(unsigned char)requestedModule << std::dec << ")";
                        if ( !callsign.IsValid() )
                            std::cout << " [invalid callsign]";
                        if ( !IsLetter(requestedModule) )
                            std::cout << " [invalid module]";
                        std::cout << " (" << tracker.m_nCount << "/3)" << std::endl;

                        EncodeConnectNackPacket(&Buffer);
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

                        std::cout << "DExtra persistent invalid connect from " << Ip
                                  << " - ignoring for " << ignoreMins
                                  << " min (strike " << tracker.m_nStrike << ")" << std::endl;
                    }
                }
                } // end of reject tracker cap else
            }
            else
            {
                std::cout << "DExtra unknown packet (" << Buffer.size() << " bytes) from " << Ip << " [";
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

void CDextraProtocol::TxTask(void)
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
    if ( m_LastKeepaliveTime.DurationSinceNow() > DEXTRA_KEEPALIVE_PERIOD )
    {
        HandleKeepalives();
        m_LastKeepaliveTime.Now();
    }

    // handle XRF peer links
    if ( m_LastDextraPeerLinkTime.DurationSinceNow() > DEXTRA_PEER_RECONNECT_PERIOD )
    {
        HandleDextraPeerLinks();
        m_LastDextraPeerLinkTime.Now();
    }

    // handle XRF peer keepalives
    if ( m_LastDextraPeerKeepaliveTime.DurationSinceNow() > DEXTRA_PEER_KEEPALIVE_PERIOD )
    {
        HandleDextraPeerKeepalives();
        m_LastDextraPeerKeepaliveTime.Now();
    }

    // handle XRF peer connection timeouts
    HandleDextraPeerConnectionStates();
}

////////////////////////////////////////////////////////////////////////////////////////
// pending ack helper

void CDextraProtocol::HandlePendingAcks(void)
{
    // collect ready acks under lock, then process without lock
    std::vector<CDextraPendingAck> readyAcks;
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
    for ( const CDextraPendingAck &ack : readyAcks )
    {
        CBuffer buffer;
        if ( ack.m_bAccepted )
        {
            // build ack packet (needs original buffer format for EncodeConnectAckPacket)
            buffer.Set((uint8 *)(const char *)ack.m_Callsign, CALLSIGN_LEN);
            buffer.Append((uint8)ack.m_Callsign.GetModule());
            buffer.Append((uint8)ack.m_ToLinkModule);
            buffer.Append((uint8)0);

            // acknowledge the request
            EncodeConnectAckPacket(&buffer, ack.m_ProtRev);
            m_Socket.Send(buffer, ack.m_Ip);

            // check if any client currently streaming on the module
            int moduleIdx = g_Reflector.GetModuleIndex(ack.m_ToLinkModule);
            if ( moduleIdx >= 0 && moduleIdx < NB_OF_MODULES )
            {
                CClients *clients = g_Reflector.GetClients();
                for ( int i = 0; i < clients->GetSize(); i++ )
                {
                    if ( clients->GetClient(i)->IsAMaster() && (clients->GetClient(i)->GetReflectorModule() == ack.m_ToLinkModule) )
                    {
                        // snapshot cache under slot lock
                        uint16 cachedSid;
                        CDvHeaderPacket cachedHdr;
                        {
                            std::lock_guard<std::mutex> slotLock(m_StreamsCache[moduleIdx].m_Mutex);
                            cachedSid = m_StreamsCache[moduleIdx].m_uiOutboundStreamId;
                            cachedHdr = m_StreamsCache[moduleIdx].m_dvHeader;
                        }
                        CBuffer buffer2;
                        if ( cachedSid != 0 && EncodeDvPacket(cachedHdr, &buffer2) )
                        {
                            // and send it to the connecting client, so it can listen the already streaming client
                            for ( int j = 0; j < 5; j++ )
                            {
                                m_Socket.Send(buffer2, ack.m_Ip);
                            }
                        }
                        break; // done, stop here
                    }
                }
                g_Reflector.ReleaseClients();
            }

            // create the client
            CDextraClient *client = new CDextraClient(ack.m_Callsign, ack.m_Ip, ack.m_ToLinkModule, ack.m_ProtRev);

            // and append
            g_Reflector.GetClients()->AddClient(client);
            g_Reflector.ReleaseClients();
        }
        else
        {
            // build nack packet
            buffer.Set((uint8 *)(const char *)ack.m_Callsign, CALLSIGN_LEN);
            buffer.Append((uint8)ack.m_Callsign.GetModule());
            buffer.Append((uint8)ack.m_ToLinkModule);
            buffer.Append((uint8)0);

            // deny the request
            EncodeConnectNackPacket(&buffer);
            m_Socket.Send(buffer, ack.m_Ip);

            std::cout << "DExtra connect nack sent to " << ack.m_Callsign << " at " << ack.m_Ip << std::endl;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// queue helper

void CDextraProtocol::RefreshClientCache(int iModId)
{
    // bounds check
    if ( iModId < 0 || iModId >= NB_OF_MODULES )
    {
        return;
    }

    // Check freshness under cache lock, release before acquiring Clients lock
    // to prevent cache→Clients lock inversion
    {
        std::lock_guard<std::mutex> lock(m_ClientCache[iModId].m_Mutex);
        if ( m_ClientCache[iModId].m_bInitialized &&
             m_ClientCache[iModId].m_LastRefresh.DurationSinceNow() < DEXTRA_CLIENT_CACHE_REFRESH_INTERVAL )
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
    while ( (client = clients->FindNextClient(PROTOCOL_DEXTRA, &index)) != NULL )
    {
        // cache non-master, non-peer clients linked to this module
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

void CDextraProtocol::SendToModuleClients(int iModId, const CBuffer &buffer, int repeatCount, bool hasOwner, const CIp &ownerIp)
{
    // bounds check
    if ( iModId < 0 || iModId >= NB_OF_MODULES )
    {
        return;
    }

    // refresh cache if needed
    RefreshClientCache(iModId);

    // send to cached clients (cache mutex is TX-thread-only, safe to hold during sends)
    std::lock_guard<std::mutex> lock(m_ClientCache[iModId].m_Mutex);
    for ( const CIp &ip : m_ClientCache[iModId].m_ClientIps )
    {
        // skip the stream owner to prevent audio echo back to transmitter
        if ( hasOwner && ownerIp == ip )
        {
            continue;
        }
        for ( int i = 0; i < repeatCount; i++ )
        {
            m_Socket.SendVoice(buffer, ip);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// echo detection helper

bool CDextraProtocol::IsEchoOfOutboundStream(uint16 streamId, const CIp &ip)
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

void CDextraProtocol::HandleQueue(void)
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

        // encode under slot lock, send without it
        CBuffer buffer;
        bool hasOwner;
        CIp ownerIp;
        int repeatCount = 1;
        {
            std::lock_guard<std::mutex> lock(m_StreamsCache[iModId].m_Mutex);

            // check if it's header and update cache
            if ( packet->IsDvHeader() )
            {
                m_StreamsCache[iModId].m_dvHeader = CDvHeaderPacket((const CDvHeaderPacket &)*packet);
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
                repeatCount = 5;
            }

            // snapshot owner exclusion fields
            hasOwner = m_StreamsCache[iModId].m_bHasOwner;
            ownerIp = m_StreamsCache[iModId].m_OwnerIp;

            // encode
            EncodeDvPacket(*packet, &buffer);
        } // slot lock released before network I/O

        if ( buffer.size() > 0 )
        {
            SendToModuleClients(iModId, buffer, repeatCount, hasOwner, ownerIp);

            // Also send to connected DExtra peers
            CPeers *peers = g_Reflector.GetPeers();
            int peerIndex = -1;
            CPeer *peer = NULL;
            while ( (peer = peers->FindNextPeer(PROTOCOL_DEXTRA, &peerIndex)) != NULL )
            {
                CDextraPeer *dextraPeer = dynamic_cast<CDextraPeer *>(peer);

                if ( dextraPeer == NULL || !dextraPeer->IsConnected() )
                    continue;

                if ( packet->GetModuleId() != dextraPeer->GetLocalModule() )
                    continue;

                // Skip if this stream originated from this peer (prevent mirror)
                CIp peerIp = dextraPeer->GetIp();
                peerIp.SetPort(htons(dextraPeer->GetPort()));
                if ( hasOwner && ownerIp == peerIp )
                    continue;

                // Send the packet to the peer
                // For headers, rewrite RPT2 module to the peer's target module
                // so it looks like a regular user linked to that module
                if ( packet->IsDvHeader() )
                {
                    CDvHeaderPacket peerHeader((const CDvHeaderPacket &)*packet);
                    peerHeader.SetRpt2Module(dextraPeer->GetRemoteModule());
                    CBuffer peerBuffer;
                    if ( EncodeDvPacket(peerHeader, &peerBuffer) )
                    {
                        for ( int j = 0; j < repeatCount; j++ )
                        {
                            m_Socket.SendVoice(peerBuffer, dextraPeer->GetIp(), dextraPeer->GetPort());
                        }
                    }
                }
                else
                {
                    m_Socket.SendVoice(buffer, dextraPeer->GetIp(), dextraPeer->GetPort());
                }
            }
            g_Reflector.ReleasePeers();
        }

        // done
        delete packet;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// keepalive helpers

void CDextraProtocol::HandleKeepalives(void)
{
    // DExtra protocol sends and monitors keepalives packets using per-client timing
    // even if the client is currently streaming
    CBuffer keepalive;
    EncodeKeepAlivePacket(&keepalive);

    // iterate on clients
    CClients *clients = g_Reflector.GetClients();
    int index = -1;
    CClient *client = NULL;
    std::vector<CClient *> toRemove;
    while ( (client = clients->FindNextClient(PROTOCOL_DEXTRA, &index)) != NULL )
    {
        // check if it's time to send keepalive to this client
        if ( client->IsKeepaliveDue() )
        {
            // send keepalive
            m_Socket.Send(keepalive, client->GetIp());

            // reset timer and schedule next keepalive at regular interval
            client->ResetKeepaliveTimer();
            client->ScheduleNextKeepalive(DEXTRA_KEEPALIVE_PERIOD);

            // client busy ?
            if ( client->IsAMaster() )
            {
                // yes, just tickle it
                client->Alive();
            }
            // otherwise check if still with us
            else if ( !client->IsAlive() )
            {
                // no, disconnect
                CBuffer disconnect;
                EncodeDisconnectPacket(&disconnect);
                m_Socket.Send(disconnect, client->GetIp());

                // collect for removal after loop
                std::cout << "DExtra client " << client->GetCallsign() << " keepalive timeout" << std::endl;
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
// streams helpers

bool CDextraProtocol::OnDvHeaderPacketIn(CDvHeaderPacket *Header, const CIp &Ip)
{
    bool newstream = false;

    // set default suffix if not already set
    if ( !Header->HasMySuffix() )
    {
        Header->SetMySuffix("DXTR");
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
        CClient *client = clients->FindClient(Ip, PROTOCOL_DEXTRA);

        // If not found by exact IP match, check if this is from a DExtra peer
        // (peer clients may have been created with a different port)
        if ( client == NULL )
        {
            // Look for a peer client with matching IP:port
            // Peers always send from their well-known server port
            int index = -1;
            CClient *c = NULL;
            while ( (c = clients->FindNextClient(PROTOCOL_DEXTRA, &index)) != NULL )
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
            // get client callsign
            via = client->GetCallsign();
            // For peer clients or protocol revision 2, update RPT2 module
            // to our local module. This ensures the stream opens on the
            // correct local module regardless of what the remote sent.
            if ( client->IsPeer() || client->GetProtocolRevision() == 2 )
            {
                Header->SetRpt2Module(client->GetReflectorModule());
            }
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

        // update last heard (only if stream actually opened)
        if ( newstream )
        {
            g_Reflector.GetUsers()->Hearing(myCallsign, via, rpt2Callsign);
            g_Reflector.ReleaseUsers();
        }
        else if ( Header != NULL )
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
// packet decoding helpers

bool CDextraProtocol::IsValidConnectPacket(const CBuffer &Buffer, CCallsign *callsign, char *reflectormodule, int *revision)
{
    bool valid = false;
    if ((Buffer.size() == 11) && (Buffer.data()[9] != ' '))
    {
        callsign->SetCallsign(Buffer.data(), 8);
        callsign->SetModule(Buffer.data()[8]);
        *reflectormodule = Buffer.data()[9];
        valid = (callsign->IsValid() && IsLetter(*reflectormodule));
        // detect revision
        if ( (Buffer.data()[10] == 11) )
        {
            *revision = 1;
        }
        else if ( callsign->HasSameCallsignWithWildcard(CCallsign("XRF*")) )
        {
            *revision = 2;
        }
        else
        {
            *revision = 0;
        }
    }
    return valid;
}

bool CDextraProtocol::IsValidDisconnectPacket(const CBuffer &Buffer, CCallsign *callsign)
{
    bool valid = false;
    if ((Buffer.size() == 11) && (Buffer.data()[9] == ' '))
    {
        callsign->SetCallsign(Buffer.data(), 8);
        callsign->SetModule(Buffer.data()[8]);
       valid = callsign->IsValid();
    }
    return valid;
}

bool CDextraProtocol::IsValidKeepAlivePacket(const CBuffer &Buffer, CCallsign *callsign)
{
    bool valid = false;
    if (Buffer.size() == 9)
    {
        callsign->SetCallsign(Buffer.data(), 8);
        valid = callsign->IsValid();
    }
    return valid;
}

CDvHeaderPacket *CDextraProtocol::IsValidDvHeaderPacket(const CBuffer &Buffer)
{
    CDvHeaderPacket *header = NULL;
    
    if ( (Buffer.size() == 56) && (Buffer.Compare((uint8 *)"DSVT", 4) == 0) &&
         (Buffer.data()[4] == 0x10) && (Buffer.data()[8] == 0x20) )
    {
        // create packet
        header = new CDvHeaderPacket((struct dstar_header *)&(Buffer.data()[15]),
                                *((uint16 *)&(Buffer.data()[12])), 0x80);
        // check validity of packet
        if ( !header->IsValid() )
        {
            delete header;
            header = NULL;
        }
    }
    return header;
}

CDvFramePacket *CDextraProtocol::IsValidDvFramePacket(const CBuffer &Buffer)
{
    CDvFramePacket *dvframe = NULL;
    
    if ( (Buffer.size() == 27) && (Buffer.Compare((uint8 *)"DSVT", 4) == 0) &&
         (Buffer.data()[4] == 0x20) && (Buffer.data()[8] == 0x20) &&
         ((Buffer.data()[14] & 0x40) == 0) )
    {
        // create packet
        dvframe = new CDvFramePacket((struct dstar_dvframe *)&(Buffer.data()[15]),
                                     *((uint16 *)&(Buffer.data()[12])), Buffer.data()[14]);
        // check validity of packet
        if ( !dvframe->IsValid() )
        {
            delete dvframe;
            dvframe = NULL;
        }
    }
    return dvframe;
}

CDvLastFramePacket *CDextraProtocol::IsValidDvLastFramePacket(const CBuffer &Buffer)
{
    CDvLastFramePacket *dvframe = NULL;
    
    if ( (Buffer.size() == 27) && (Buffer.Compare((uint8 *)"DSVT", 4) == 0) &&
         (Buffer.data()[4] == 0x20) && (Buffer.data()[8] == 0x20) &&
         ((Buffer.data()[14] & 0x40) != 0) )
    {
        // create packet
        dvframe = new CDvLastFramePacket((struct dstar_dvframe *)&(Buffer.data()[15]),
                                         *((uint16 *)&(Buffer.data()[12])), Buffer.data()[14] & 0x1F);
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

void CDextraProtocol::EncodeKeepAlivePacket(CBuffer *Buffer)
{
   Buffer->Set(GetReflectorCallsign());
}

void CDextraProtocol::EncodeConnectAckPacket(CBuffer *Buffer, int ProtRev)
{
   // is it for a XRF or repeater
    if ( ProtRev == 2 )
    {
        // XRFxxx
        uint8 rm = (Buffer->data())[8];
        uint8 lm = (Buffer->data())[9];
        Buffer->clear();
        Buffer->Set((uint8 *)(const char *)GetReflectorCallsign(), CALLSIGN_LEN);
        Buffer->Append(lm);
        Buffer->Append(rm);
        Buffer->Append((uint8)0);
    }
    else
    {
        // regular repeater
        uint8 tag[] = { 'A','C','K',0 };
        Buffer->resize(Buffer->size()-1);
        Buffer->Append(tag, sizeof(tag));
    }
}

void CDextraProtocol::EncodeConnectNackPacket(CBuffer *Buffer)
{
    uint8 tag[] = { 'N','A','K',0 };
    Buffer->resize(Buffer->size()-1);
    Buffer->Append(tag, sizeof(tag));
}

void CDextraProtocol::EncodeDisconnectPacket(CBuffer *Buffer)
{
    uint8 tag[] = { ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',0 };
    Buffer->Set(tag, sizeof(tag));
}

void CDextraProtocol::EncodeDisconnectedPacket(CBuffer *Buffer)
{
    uint8 tag[] = { 'D','I','S','C','O','N','N','E','C','T','E','D' };
    Buffer->Set(tag, sizeof(tag));
}

bool CDextraProtocol::EncodeDvHeaderPacket(const CDvHeaderPacket &Packet, CBuffer *Buffer) const
{
    uint8 tag[]	= { 'D','S','V','T',0x10,0x00,0x00,0x00,0x20,0x00,0x01,0x02 };
    struct dstar_header DstarHeader;
    
    Packet.ConvertToDstarStruct(&DstarHeader);
    
    Buffer->Set(tag, sizeof(tag));
    Buffer->Append(Packet.GetStreamId());
    Buffer->Append((uint8)0x80);
    Buffer->Append((uint8 *)&DstarHeader, sizeof(struct dstar_header));
    
    return true;
}

bool CDextraProtocol::EncodeDvFramePacket(const CDvFramePacket &Packet, CBuffer *Buffer) const
{
    uint8 tag[] = { 'D','S','V','T',0x20,0x00,0x00,0x00,0x20,0x00,0x01,0x02 };
    
    Buffer->Set(tag, sizeof(tag));
    Buffer->Append(Packet.GetStreamId());
    Buffer->Append((uint8)(Packet.GetPacketId() % 21));
    Buffer->Append((uint8 *)Packet.GetAmbe(), AMBE_SIZE);
    Buffer->Append((uint8 *)Packet.GetDvData(), DVDATA_SIZE);

    return true;
    
}

bool CDextraProtocol::EncodeDvLastFramePacket(const CDvLastFramePacket &Packet, CBuffer *Buffer) const
{
    uint8 tag1[] = { 'D','S','V','T',0x20,0x00,0x00,0x00,0x20,0x00,0x01,0x02 };
    uint8 tag2[] = { 0x55,0xC8,0x7A,0x00,0x00,0x00,0x00,0x00,0x00,0x25,0x1A,0xC6 };

    Buffer->Set(tag1, sizeof(tag1));
    Buffer->Append(Packet.GetStreamId());
    Buffer->Append((uint8)((Packet.GetPacketId() % 21) | 0x40));
    Buffer->Append(tag2, sizeof(tag2));

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
// XRF peer helpers

void CDextraProtocol::HandleDextraPeerLinks(void)
{
    // Snapshot the peer list under the GK lock, then release BEFORE acquiring
    // Peers or Clients locks. Prevents ABBA: RX holds Clients → GK (via MayLink).
    std::vector<CCallsignListItem> peerListSnapshot;
    {
        CPeerCallsignList *peerList = g_GateKeeper.GetPeerList();
        peerListSnapshot.assign(peerList->begin(), peerList->end());
        g_GateKeeper.ReleasePeerList();
    }

    CPeers *peers = g_Reflector.GetPeers();

    // Check if all our connected DExtra peers are still listed by gatekeeper
    // If not, send disconnect and clean up
    int peerIndex = -1;
    CPeer *peer = NULL;
    std::vector<CPeer *> peersToRemove;
    while ( (peer = peers->FindNextPeer(PROTOCOL_DEXTRA, &peerIndex)) != NULL )
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
            CDextraPeer *dextraPeer = dynamic_cast<CDextraPeer *>(peer);
            std::cout << "DExtra peer " << peer->GetCallsign() << " removed - no longer in peer list, sending disconnect" << std::endl;

            // Send disconnect packet
            if ( dextraPeer != NULL && dextraPeer->IsConnected() )
            {
                CBuffer disconnect;
                EncodeDisconnectPacket(&disconnect);
                m_Socket.Send(disconnect, dextraPeer->GetIp(), dextraPeer->GetPort());
            }

            // Find and remove the associated peer client
            CClients *clients = g_Reflector.GetClients();
            int clientIndex = -1;
            CClient *client = NULL;
            while ( (client = clients->FindNextClient(PROTOCOL_DEXTRA, &clientIndex)) != NULL )
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

    // Iterate through the peer list snapshot looking for XRF peers
    for ( size_t i = 0; i < peerListSnapshot.size(); i++ )
    {
        CCallsignListItem &item = peerListSnapshot[i];

        // Only process XRF peers (callsign starts with "XRF")
        if ( !IsXrfPeerCallsign(item.GetCallsign()) )
        {
            continue;
        }

        // Resolve IP if needed
        if ( item.GetIp().GetAddr() == 0 )
        {
            item.ResolveIp();
            if ( item.GetIp().GetAddr() == 0 )
            {
                std::cout << "DExtra: cannot resolve hostname for peer " << item.GetCallsign() << std::endl;
                continue;
            }
        }

        // Check if we already have this peer
        CPeer *existingPeer = peers->FindPeer(item.GetCallsign(), PROTOCOL_DEXTRA);

        if ( existingPeer == NULL )
        {
            // Create new peer and initiate connection
            char modules[3];
            modules[0] = item.GetLocalModule();
            modules[1] = item.GetRemoteModule();
            modules[2] = '\0';

            CDextraPeer *newPeer = new CDextraPeer(item.GetCallsign(), item.GetIp(), modules, CVersion());
            uint16 port = item.GetPort();
            if ( port == 0 )
            {
                port = DEXTRA_PORT;
            }
            newPeer->SetPort(port);
            newPeer->SetConnectionState(DEXTRA_PEER_STATE_CONNECTING);
            newPeer->ResetConnectTimer();

            // Add peer to the reflector
            peers->AddPeer(newPeer);

            std::cout << "DExtra connecting to peer " << item.GetCallsign()
                      << " at " << item.GetIp() << ":" << port
                      << " (local " << item.GetLocalModule() << " -> remote " << item.GetRemoteModule() << ")"
                      << std::endl;

            // Send connect packet
            CBuffer buffer;
            EncodeConnectPacket(&buffer, g_Reflector.GetCallsign(), item.GetLocalModule(), item.GetRemoteModule());
            m_Socket.Send(buffer, item.GetIp(), port);
        }
        else
        {
            // Peer exists but disconnected - try to reconnect
            CDextraPeer *dextraPeer = dynamic_cast<CDextraPeer *>(existingPeer);
            if ( dextraPeer != NULL && !dextraPeer->IsConnected() && !dextraPeer->IsConnecting() )
            {
                dextraPeer->SetConnectionState(DEXTRA_PEER_STATE_CONNECTING);
                dextraPeer->ResetConnectTimer();

                std::cout << "DExtra reconnecting to peer " << item.GetCallsign() << std::endl;

                // Send connect packet
                CBuffer buffer;
                EncodeConnectPacket(&buffer, g_Reflector.GetCallsign(), dextraPeer->GetLocalModule(), dextraPeer->GetRemoteModule());
                m_Socket.Send(buffer, dextraPeer->GetIp(), dextraPeer->GetPort());
            }
        }
    }

    g_Reflector.ReleasePeers();
}

void CDextraProtocol::HandleDextraPeerKeepalives(void)
{
    CBuffer keepalive;
    EncodeKeepAlivePacket(&keepalive);

    // Send keepalives to all connected DExtra peers
    CPeers *peers = g_Reflector.GetPeers();
    int index = -1;
    CPeer *peer = NULL;
    std::vector<CPeer *> peersToRemove;
    while ( (peer = peers->FindNextPeer(PROTOCOL_DEXTRA, &index)) != NULL )
    {
        CDextraPeer *dextraPeer = dynamic_cast<CDextraPeer *>(peer);
        if ( dextraPeer == NULL )
        {
            continue;
        }

        // Only send keepalives to connected peers
        if ( !dextraPeer->IsConnected() )
        {
            continue;
        }

        // Send keepalive
        m_Socket.Send(keepalive, dextraPeer->GetIp(), dextraPeer->GetPort());

        // Check for timeout
        if ( !dextraPeer->IsAlive() )
        {
            std::cout << "DExtra peer " << dextraPeer->GetCallsign() << " keepalive timeout, disconnecting..." << std::endl;

            // Lock order: Peers → Clients (consistent across all peer management paths)
            // Remove peer client from clients list
            CClients *clients = g_Reflector.GetClients();
            int clientIndex = -1;
            CClient *client = NULL;
            while ( (client = clients->FindNextClient(PROTOCOL_DEXTRA, &clientIndex)) != NULL )
            {
                if ( client->IsPeer() && (client->GetIp().GetAddr() == dextraPeer->GetIp().GetAddr()) )
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
    for ( size_t i = 0; i < peersToRemove.size(); i++ )
    {
        peers->RemovePeer(peersToRemove[i]);
    }
    g_Reflector.ReleasePeers();
}

void CDextraProtocol::HandleDextraPeerConnectionStates(void)
{
    // Check for connection timeouts
    CPeers *peers = g_Reflector.GetPeers();
    int index = -1;
    CPeer *peer = NULL;
    while ( (peer = peers->FindNextPeer(PROTOCOL_DEXTRA, &index)) != NULL )
    {
        CDextraPeer *dextraPeer = dynamic_cast<CDextraPeer *>(peer);
        if ( dextraPeer == NULL )
        {
            continue;
        }

        if ( dextraPeer->IsConnecting() )
        {
            // Check for timeout waiting for connect ack
            if ( dextraPeer->GetConnectDuration() > DEXTRA_PEER_CONNECT_TIMEOUT )
            {
                std::cout << "DExtra peer " << dextraPeer->GetCallsign() << " connect timeout" << std::endl;
                dextraPeer->SetConnectionState(DEXTRA_PEER_STATE_DISCONNECTED);
            }
        }
    }
    g_Reflector.ReleasePeers();
}

bool CDextraProtocol::IsXrfPeerCallsign(const CCallsign &callsign) const
{
    // Check if callsign starts with "XRF" followed by a digit
    const char *cs = (const char *)callsign;
    return (::strncmp(cs, "XRF", 3) == 0) && (cs[3] >= '0') && (cs[3] <= '9');
}

void CDextraProtocol::EncodeConnectPacket(CBuffer *Buffer, const CCallsign &callsign, char localModule, char remoteModule)
{
    // DExtra connect packet format:
    // 11 bytes: callsign(8) + clientModule(1) + reflectorModule(1) + revision(1)
    // For XRF peer connections, we use revision 2 format

    // Use XRF prefix for our callsign when connecting as a peer
    CCallsign xrfCallsign(callsign);
    xrfCallsign.PatchCallsign(0, (const uint8 *)"XRF", 3);

    Buffer->Set((uint8 *)(const char *)xrfCallsign, CALLSIGN_LEN);
    Buffer->Append((uint8)localModule);     // Our module (client side) - byte[8]
    Buffer->Append((uint8)remoteModule);    // Remote module we want to link to - byte[9]
    Buffer->Append((uint8)0);               // Revision 0 (standard), will be detected as rev 2 due to XRF prefix
}

bool CDextraProtocol::IsValidConnectAckPacket(const CBuffer &Buffer)
{
    // Connect ack for XRF (revision 2) format:
    // 11 bytes: reflectorCallsign(8) + remoteModule(1) + localModule(1) + 0

    if ( Buffer.size() != 11 )
    {
        return false;
    }

    // The ack should contain the remote reflector's callsign
    CCallsign ackCallsign;
    ackCallsign.SetCallsign(Buffer.data(), 8);

    // Check that the modules are valid letters
    char remoteModule = Buffer.data()[8];
    char localModule = Buffer.data()[9];

    return ackCallsign.IsValid() && IsLetter(remoteModule) && IsLetter(localModule);
}

