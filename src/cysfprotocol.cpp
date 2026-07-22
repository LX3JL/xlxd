//
//  cysfprotocol.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 20/05/2018.
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
#include "ccrc.h"
#include "cysfpayload.h"
#include "cysfclient.h"
#include "cysfutils.h"
#include "cysfprotocol.h"
#include "creflector.h"
#include "cgatekeeper.h"
#include "cysfpeer.h"
#include "cysfpeerclient.h"
#include "cpeercallsignlist.h"

////////////////////////////////////////////////////////////////////////////////////////
// constructor
CYsfProtocol::CYsfProtocol()
{
    m_seqNo = 0;
}

////////////////////////////////////////////////////////////////////////////////////////
// operation

bool CYsfProtocol::Init(void)
{
    bool ok;
    
    // base class
    ok = CProtocol::Init();
    
    // update the reflector callsign
    m_ReflectorCallsign.PatchCallsign(0, (const uint8 *)"YSF", 3);
    
    // create our socket
    ok &= m_Socket.Open(YSF_PORT);
    if ( !ok )
    {
        std::cout << "Error opening socket on port UDP" << YSF_PORT << " on ip " << g_Reflector.GetListenIp() << std::endl;
    }
    
    // init the wiresx cmd handler
    ok &= m_WiresxCmdHandler.Init();

    // update time
    m_LastKeepaliveTime.Now();
    m_LastYsfPeerLinkTime.Now();
    m_LastYsfPeerKeepaliveTime.Now();

    // done
    return ok;
}

void CYsfProtocol::Close(void)
{
    // base class
    CProtocol::Close();
    
    // and close wiresx handler
    m_WiresxCmdHandler.Close();
}

////////////////////////////////////////////////////////////////////////////////////////
// task

void CYsfProtocol::RxTask(void)
{
    CBuffer             Buffer;
    CIp                 Ip;
    CCallsign           Callsign;
    CYSFFICH            Fich;
    CDvHeaderPacket     *Header;
    CDvFramePacket      *Frames[5];
    CWiresxCmd          WiresxCmd;
    
    int                 iWiresxCmd;
    int                 iWiresxArg;

    // handle outgoing packets
    {
        // any packet to go ?
        CWiresxPacketQueue *queue = m_WiresxCmdHandler.GetPacketQueue();
        while ( !queue->empty() )
        {
            CWiresxPacket packet = queue->front();
            queue->pop();
            m_Socket.Send(packet.GetBuffer(), packet.GetIp());
        }
        m_WiresxCmdHandler.ReleasePacketQueue();
    }
    
    // handle incoming packets
    if ( m_Socket.Receive(&Buffer, &Ip, 20) != -1 )
    {
        // crack the packet
        if ( IsValidDvPacket(Buffer, &Fich) )
        {
            if ( IsValidDvFramePacket(Ip, Fich, Buffer, Frames) )
            {
                // handle it
                OnDvFramePacketIn(Frames[0], &Ip);
                OnDvFramePacketIn(Frames[1], &Ip);
                OnDvFramePacketIn(Frames[2], &Ip);
                OnDvFramePacketIn(Frames[3], &Ip);
                OnDvFramePacketIn(Frames[4], &Ip);
            }
            else if ( IsValidDvHeaderPacket(Ip, Fich, Buffer, &Header, Frames) )
            {
                // IsValidDvHeaderPacket allocates dummy silence frames alongside the header
                // We only need the header — delete the frames to avoid a leak
                delete Frames[0];
                delete Frames[1];
                Frames[0] = NULL;
                Frames[1] = NULL;

                // Skip gatekeeper check for peer connections (we initiated these)
                bool isPeer = false;
                char clientModule = ' ';
                {
                    CPeers *peers = g_Reflector.GetPeers();
                    isPeer = (peers->FindPeerByIpPort(Ip, PROTOCOL_YSF) != NULL);
                    g_Reflector.ReleasePeers();
                }

                // For YSF clients, look up their assigned module for gatekeeper check (not RPT2)
                if ( !isPeer )
                {
                    CClients *clients = g_Reflector.GetClients();
                    CClient *client = clients->FindClient(Ip, PROTOCOL_YSF);
                    if ( client != NULL )
                    {
                        clientModule = client->GetReflectorModule();
                    }
                    g_Reflector.ReleaseClients();
                }

                // Peer traffic bypasses MayTransmit (peers are trusted
                // interlinks, not subject to whitelist/blacklist/
                // callsign-validity checks) but is still subject to
                // the per-callsign loop block — without that, a
                // peer-sourced echo loop on a single user callsign
                // would never be stopped (MW0MWZ/xlxd production
                // observation, May 2026). Non-peer traffic continues
                // through the regular MayTransmit gate, which itself
                // consults the loop block via IsLoopSuppressed.
                if ( isPeer )
                {
                    if ( g_GateKeeper.IsCallsignLoopBlocked(
                            Header->GetMyCallsign(), "YSF peer") )
                    {
                        delete Header;
                        Header = NULL;
                    }
                }
                else if ( !g_GateKeeper.MayTransmit(
                            Header->GetMyCallsign(), Ip,
                            PROTOCOL_YSF, clientModule) )
                {
                    delete Header;
                    Header = NULL;
                }

                if ( Header != NULL )
                {
                    OnDvHeaderPacketIn(Header, Ip);
                }
            }
            else if ( IsValidDvLastFramePacket(Ip, Fich, Buffer, Frames) )
            {
                // handle it
                OnDvFramePacketIn(Frames[0], &Ip);
                OnDvLastFramePacketIn((CDvLastFramePacket *)Frames[1], &Ip);
            }
        }
        else if ( IsValidConnectPacket(Buffer, &Callsign) )
        {
            // Check if this is a poll response from a YSF reflector peer we connected to
            CPeers *peers = g_Reflector.GetPeers();
            CPeer *existingPeer = peers->FindPeerByIpPort(Ip, PROTOCOL_YSF);

            if ( existingPeer != NULL )
            {
                // This is a poll response from a YSF peer - mark it alive
                existingPeer->Alive();
                g_Reflector.ReleasePeers();

                // Also keep the corresponding client alive
                CClients *clients = g_Reflector.GetClients();
                int index = -1;
                CClient *client = NULL;
                while ( (client = clients->FindNextClient(PROTOCOL_YSF, &index)) != NULL )
                {
                    if ( client->IsPeer() && (client->GetIp() == Ip) )
                    {
                        client->Alive();
                        break;
                    }
                }
                g_Reflector.ReleaseClients();
            }
            else
            {
                // Release Peers before acquiring GK to prevent lock-order inversion
                g_Reflector.ReleasePeers();

                // Snapshot GK data for this IP
                CCallsign peerCallsign;
                char peerModule = ' ';
                uint32_t ysfId = 0;
                bool foundInGk = false;
                {
                    CPeerCallsignList *list = g_GateKeeper.GetPeerList();
                    CCallsignListItem *item = FindYsfPeerByIp(list, Ip);
                    if ( item != NULL )
                    {
                        peerCallsign = item->GetCallsign();
                        peerModule = item->GetModules()[0];
                        char cs[CALLSIGN_LEN + 1];
                        item->GetCallsign().GetCallsign((uint8 *)cs);
                        cs[CALLSIGN_LEN] = '\0';
                        ysfId = (uint32_t)::atoi(cs + 3);
                        foundInGk = true;
                    }
                    g_GateKeeper.ReleasePeerList();
                }

                if ( foundInGk )
                {
                    // Create the YSF peer object with its client attached
                    CVersion version(1, 0, 0);
                    char modules[2] = { peerModule, '\0' };

                    CYsfPeer *newPeer = new CYsfPeer(peerCallsign, Ip, modules, version);
                    newPeer->Alive();
                    newPeer->SetYsfId(ysfId);

                    std::cout << "YSF peer " << peerCallsign << " connected on module " << peerModule << std::endl;

                    // AddPeer registers peer and its clients in the reflector
                    CPeers *peers2 = g_Reflector.GetPeers();
                    peers2->AddPeer(newPeer);
                    g_Reflector.ReleasePeers();
                }
                else
                {

                    // Not a YSF peer, handle as regular client
                    // callsign authorized as a regular client?
                    if ( g_GateKeeper.MayLink(Callsign, Ip, PROTOCOL_YSF) )
                    {
                        // acknowledge the request
                        EncodeConnectAckPacket(&Buffer);
                        m_Socket.Send(Buffer, Ip);

                        // add client if needed
                        CClients *clients = g_Reflector.GetClients();
                        CClient *client = clients->FindClient(Callsign, Ip, PROTOCOL_YSF);
                        // client already connected ?
                        if ( client == NULL )
                        {
                            std::cout << "YSF connect packet from " << Callsign << " at " << Ip << std::endl;

                            // create the client
                            CYsfClient *newclient = new CYsfClient(Callsign, Ip);

                            // autolink, if enabled
                            #if YSF_AUTOLINK_ENABLE
                                newclient->SetReflectorModule(YSF_AUTOLINK_MODULE);
                            #endif

                            // and append
                            clients->AddClient(newclient);
                        }
                        else
                        {
                            client->Alive();
                        }
                        // and done
                        g_Reflector.ReleaseClients();
                    }
                }
            }
        }
        else if ( IsValidwirexPacket(Buffer, Ip, &Fich, &Callsign, &iWiresxCmd, &iWiresxArg) )
        {
            // Ignore Wires-X commands from YSF peers - only process from direct clients
            CPeers *peers = g_Reflector.GetPeers();
            CPeer *peer = peers->FindPeerByIpPort(Ip, PROTOCOL_YSF);
            g_Reflector.ReleasePeers();

            if ( peer == NULL )
            {
                // Not from a YSF peer, process the command
                WiresxCmd = CWiresxCmd(Ip, Callsign, iWiresxCmd, iWiresxArg);
                m_WiresxCmdHandler.GetCmdQueue()->push(WiresxCmd);
                m_WiresxCmdHandler.ReleaseCmdQueue();
            }
            // else: silently ignore Wires-X commands from YSF peers
        }
        else if ( IsValidServerStatusPacket(Buffer) )
        {
            std::cout << "YSF server status enquiry from " << Ip   << std::endl;
            // reply
            EncodeServerStatusPacket(&Buffer);
            m_Socket.Send(Buffer, Ip);
        }
        else
        {
            // invalid packet
            //std::cout << "YSF packet (" << Buffer.size() << ") from " << Callsign << " at " << Ip << std::endl;
            //Buffer.DebugDump(g_Reflector.m_DebugFile);
        }
    }
    
    // handle end of streaming timeout
    CheckStreamsTimeout();
    
}

////////////////////////////////////////////////////////////////////////////////////////
// TX task — runs on separate thread, never blocked by CloseStream

void CYsfProtocol::TxTask(void)
{
    {
        std::unique_lock<std::mutex> lk(m_QueueCondMutex);
        m_QueueCondVar.wait_for(lk, std::chrono::milliseconds(20));
    }

    HandleQueue();

    if ( m_LastKeepaliveTime.DurationSinceNow() > YSF_KEEPALIVE_PERIOD )
    {
        HandleKeepalives();
        m_LastKeepaliveTime.Now();
    }

    if ( m_LastYsfPeerLinkTime.DurationSinceNow() > YSF_PEER_RECONNECT_PERIOD )
    {
        HandleYsfPeerLinks();
        m_LastYsfPeerLinkTime.Now();
    }

    if ( m_LastYsfPeerKeepaliveTime.DurationSinceNow() > YSF_PEER_KEEPALIVE_PERIOD )
    {
        HandleYsfPeerKeepalives();
        m_LastYsfPeerKeepaliveTime.Now();
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// streams helpers

bool CYsfProtocol::OnDvHeaderPacketIn(CDvHeaderPacket *Header, const CIp &Ip)
{
    bool newstream = false;

    // set default suffix if not already set
    if ( !Header->HasMySuffix() )
    {
        Header->SetMySuffix("YSF");
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
        CClient *client = clients->FindClient(Ip, PROTOCOL_YSF);

        // If not found by exact IP match, check if this is from a YSF peer
        // (peer clients may have been created with a different port)
        if ( client == NULL )
        {
            // Look through all YSF protocol clients for a matching IP address (ignoring port)
            int index = -1;
            CClient *c = NULL;
            while ( (c = clients->FindNextClient(PROTOCOL_YSF, &index)) != NULL )
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

            // For YSF, always use client's assigned module (RPT2 is a D-Star concept)
            Header->SetRpt2Module(client->GetReflectorModule());

            // save callsigns before OpenStream — OpenStream transfers Header
            // to the router queue where the router thread may delete it
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
// client cache helpers

void CYsfProtocol::RefreshClientCache(int iModId)
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
             m_ClientCache[iModId].m_LastRefresh.DurationSinceNow() < YSF_CLIENT_CACHE_REFRESH_INTERVAL )
        {
            return;  // Cache is still fresh
        }
    }

    // Scan clients WITHOUT holding cache lock
    char moduleId = 'A' + iModId;
    std::vector<CIp> freshIps;
    std::vector<CIp> freshPeerIps;

    CClients *clients = g_Reflector.GetClients();
    int index = -1;
    CClient *client = NULL;
    while ( (client = clients->FindNextClient(PROTOCOL_YSF, &index)) != NULL )
    {
        if ( client->GetReflectorModule() == moduleId )
        {
            if ( client->IsPeer() && !client->IsAMaster() )
            {
                freshPeerIps.push_back(client->GetIp());
            }
            else if ( !client->IsAMaster() && !client->IsPeer() )
            {
                freshIps.push_back(client->GetIp());
            }
        }
    }
    g_Reflector.ReleaseClients();

    // Write results under cache lock
    std::lock_guard<std::mutex> lock(m_ClientCache[iModId].m_Mutex);
    m_ClientCache[iModId].m_ClientIps.swap(freshIps);
    m_ClientCache[iModId].m_PeerClientIps.swap(freshPeerIps);
    m_ClientCache[iModId].m_LastRefresh.Now();
    m_ClientCache[iModId].m_bInitialized = true;
}

void CYsfProtocol::SendToModuleClients(int iModId, const CBuffer &buffer, uint16 streamId)
{
    // bounds check
    if ( iModId < 0 || iModId >= NB_OF_MODULES )
    {
        return;
    }

    // snapshot owner fields under slot lock
    bool hasOwner = false;
    uint16 cachedStreamId = 0;
    CIp ownerIpCopy;
    {
        std::lock_guard<std::mutex> slotLock(m_StreamsCache[iModId].m_Mutex);
        hasOwner = m_StreamsCache[iModId].m_bHasOwner;
        cachedStreamId = m_StreamsCache[iModId].m_uiOutboundStreamId;
        ownerIpCopy = m_StreamsCache[iModId].m_OwnerIp;
    }

    // refresh cache if needed
    RefreshClientCache(iModId);

    // send to cached clients (cache mutex is TX-thread-only, safe to hold during sends)
    std::lock_guard<std::mutex> lock(m_ClientCache[iModId].m_Mutex);
    for ( const CIp &ip : m_ClientCache[iModId].m_ClientIps )
    {
        // skip the stream owner to prevent audio echo back to transmitter
        if ( hasOwner && cachedStreamId == streamId && ownerIpCopy == ip )
        {
            continue;
        }
        m_Socket.SendVoice(buffer, ip);
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// queue helper

void CYsfProtocol::HandleQueue(void)
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
    CBuffer buffer;
    buffer.reserve(256);

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

        // encode
        buffer.clear();

        // snapshot owner fields under slot lock for send loops
        CIp ownerIpCopy;
        bool hasOwner = false;
        uint16 outboundStreamId = 0;

        // check if it's header
        if ( packet->IsDvHeader() )
        {
            // update local stream cache and capture owner under slot lock
            std::lock_guard<std::mutex> slotLock(m_StreamsCache[iModId].m_Mutex);

            m_StreamsCache[iModId].m_dvHeader = CDvHeaderPacket((const CDvHeaderPacket &)*packet);
            m_StreamsCache[iModId].m_uiOutboundStreamId = packet->GetStreamId();
            m_StreamsCache[iModId].m_uiLastSubid = 0;
            // clear frame cache to prevent stale data from previous stream
            for ( int j = 0; j < 5; j++ )
                m_StreamsCache[iModId].m_dvFrames[j] = CDvFramePacket();

            // capture owner IP from stream (once at header time)
            m_StreamsCache[iModId].m_bHasOwner = false;
            CPacketStream *ownerStream = g_Reflector.GetStream(packet->GetModuleId());
            if ( ownerStream != NULL )
            {
                ownerStream->Lock();
                if ( ownerStream->IsOpen() && ownerStream->GetStreamId() == packet->GetStreamId() )
                {
                    const CIp *ownerIp = ownerStream->GetOwnerIp();
                    if ( ownerIp != NULL )
                    {
                        m_StreamsCache[iModId].m_OwnerIp = *ownerIp;
                        m_StreamsCache[iModId].m_bHasOwner = true;
                    }
                }
                ownerStream->Unlock();
            }

            // snapshot for send loops
            hasOwner = m_StreamsCache[iModId].m_bHasOwner;
            ownerIpCopy = m_StreamsCache[iModId].m_OwnerIp;
            outboundStreamId = m_StreamsCache[iModId].m_uiOutboundStreamId;

            // encode it
            EncodeDvHeaderPacket((const CDvHeaderPacket &)*packet, &buffer);
        }
        // check if it's a last frame
        else if ( packet->IsLastPacket() )
        {
            // snapshot owner and cache fields under slot lock
            {
                std::lock_guard<std::mutex> slotLock(m_StreamsCache[iModId].m_Mutex);
                hasOwner = m_StreamsCache[iModId].m_bHasOwner;
                ownerIpCopy = m_StreamsCache[iModId].m_OwnerIp;
                outboundStreamId = m_StreamsCache[iModId].m_uiOutboundStreamId;
            }

            // flush any incomplete quintuplet before sending terminator
            // pad missing frame slots with zeros (0x00)
            if ( m_StreamsCache[iModId].m_uiLastSubid > 0 )
            {
                uint8 zeroAmbe[9];
                ::memset(zeroAmbe, 0, sizeof(zeroAmbe));
                for ( int j = m_StreamsCache[iModId].m_uiLastSubid; j <= 4; j++ )
                {
                    CDvFramePacket zeroFrame(zeroAmbe, (uint8 *)"\x00\x00\x00\x00\x00\x00\x00",
                                             outboundStreamId, 0, j);
                    m_StreamsCache[iModId].m_dvFrames[j] = zeroFrame;
                }
                CBuffer flushBuf;
                EncodeDvPacket(m_StreamsCache[iModId].m_dvHeader, m_StreamsCache[iModId].m_dvFrames, &flushBuf);
                if ( flushBuf.size() > 0 )
                {
                    SendToModuleClients(iModId, flushBuf, packet->GetStreamId());

                    // also send to peer clients via cache (no GetClients lock needed)
                    {
                        std::lock_guard<std::mutex> cacheLock(m_ClientCache[iModId].m_Mutex);
                        for ( const CIp &peerIp : m_ClientCache[iModId].m_PeerClientIps )
                        {
                            if ( hasOwner && ownerIpCopy == peerIp )
                                continue;
                            m_Socket.SendVoice(flushBuf, peerIp);
                        }
                    }
                }
                m_StreamsCache[iModId].m_uiLastSubid = 0;
            }

            // now send the terminator
            EncodeDvLastPacket(m_StreamsCache[iModId].m_dvHeader, &buffer);
        }
        // otherwise, just a regular DV frame
        else
        {
            // snapshot owner under slot lock
            {
                std::lock_guard<std::mutex> slotLock(m_StreamsCache[iModId].m_Mutex);
                hasOwner = m_StreamsCache[iModId].m_bHasOwner;
                ownerIpCopy = m_StreamsCache[iModId].m_OwnerIp;
                outboundStreamId = m_StreamsCache[iModId].m_uiOutboundStreamId;
            }

            // update local stream cache or send quintuplet when needed
            uint8 sid = packet->GetYsfPacketSubId();

            if ( (sid >= 0) && (sid <= 4) )
            {
                m_StreamsCache[iModId].m_dvFrames[sid] = CDvFramePacket((const CDvFramePacket &)*packet);
                m_StreamsCache[iModId].m_uiLastSubid = sid + 1;
                if ( sid == 4 )
                {
                    EncodeDvPacket(m_StreamsCache[iModId].m_dvHeader, m_StreamsCache[iModId].m_dvFrames, &buffer);
                    m_StreamsCache[iModId].m_uiLastSubid = 0;
                }
            }
        }

        // send it
        if ( buffer.size() > 0 )
        {
            // send to non-peer clients via cache
            SendToModuleClients(iModId, buffer, packet->GetStreamId());

            // send to peer clients via cache (no GetClients lock needed)
            {
                std::lock_guard<std::mutex> cacheLock(m_ClientCache[iModId].m_Mutex);
                for ( const CIp &peerIp : m_ClientCache[iModId].m_PeerClientIps )
                {
                    // Skip if this stream originated from this peer (prevent loop)
                    // Full IP:port match — remote hosts may run multiple modes on different ports
                    if ( hasOwner && ownerIpCopy == peerIp )
                    {
                        continue;
                    }
                    m_Socket.SendVoice(buffer, peerIp);
                }
            }
        }

        // done
        delete packet;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// keepalive helpers

void CYsfProtocol::HandleKeepalives(void)
{
    // YSF protocol keepalive request is client tasks
    // here, just check that all clients are still alive
    // and disconnect them if not
    
    // iterate on clients, collect stale ones for removal
    // (RemoveClient erases from vector, invalidating FindNextClient iterator)
    std::vector<CClient *> toRemoveClients;
    CClients *clients = g_Reflector.GetClients();
    int index = -1;
    CClient *client = NULL;
    while ( (client = clients->FindNextClient(PROTOCOL_YSF, &index)) != NULL )
    {
        // peer clients are managed by HandleYsfPeerKeepalives, skip them here
        if ( client->IsPeer() )
        {
            continue;
        }
        // is this client busy ?
        if ( client->IsAMaster() )
        {
            // yes, just tickle it
            client->Alive();
        }
        // check it's still with us
        else if ( !client->IsAlive() )
        {
            // no, collect for removal
            toRemoveClients.push_back(client);
        }
    }
    for ( size_t i = 0; i < toRemoveClients.size(); i++ )
    {
        std::cout << "YSF client " << toRemoveClients[i]->GetCallsign() << " keepalive timeout" << std::endl;
        clients->RemoveClient(toRemoveClients[i]);
    }
    g_Reflector.ReleaseClients();
}

////////////////////////////////////////////////////////////////////////////////////////
// DV packet decoding helpers

bool CYsfProtocol::IsValidConnectPacket(const CBuffer &Buffer, CCallsign *callsign)
{
    uint8 tag[] = { 'Y','S','F','P' };

    bool valid = false;
    if ( (Buffer.size() == 14) && (Buffer.Compare(tag, sizeof(tag)) == 0) )
    {
        callsign->SetCallsign(Buffer.data()+4, 8);
        callsign->SetModule(YSF_MODULE_ID);
        valid = (callsign->IsValid());
    }
    return valid;
}

bool CYsfProtocol::IsValidDvPacket(const CBuffer &Buffer, CYSFFICH *Fich)
{
    uint8 tag[] = { 'Y','S','F','D' };

    bool valid = false;

    if ( (Buffer.size() == 155) && (Buffer.Compare(tag, sizeof(tag)) == 0) )
    {
        // decode YSF fich
        if ( Fich->decode(&(Buffer.data()[40])) )
        {
            valid = (Fich->getDT() == YSF_DT_VD_MODE2);
        }
    }
    return valid;
}


bool CYsfProtocol::IsValidDvHeaderPacket(const CIp &Ip, const CYSFFICH &Fich, const CBuffer &Buffer, CDvHeaderPacket **header, CDvFramePacket **frames)
{
    bool valid = false;
    *header = NULL;
    frames[0] = NULL;
    frames[1] = NULL;

    // DV header ?
    if ( Fich.getFI() == YSF_FI_HEADER )
    {
        // Allocate a fresh stream-id for this new transmission. Releases
        // any stale mapping for this source from a prior over that may
        // still be draining; see AllocateNewStreamIdForSource().
        uint32 uiStreamId = AllocateNewStreamIdForSource(Ip);

        // get header data
        CYSFPayload ysfPayload;
        if ( ysfPayload.processHeaderData((unsigned char *)&(Buffer.data()[35])) )
        {
            // build DVHeader
             char sz[YSF_CALLSIGN_LENGTH+1];
            ::memcpy(sz, &(Buffer.data()[14]), YSF_CALLSIGN_LENGTH);
            sz[YSF_CALLSIGN_LENGTH] = 0;
            CCallsign csMY = CCallsign();
            csMY.SetYsfCallsign(sz);
            ::memcpy(sz, &(Buffer.data()[4]), YSF_CALLSIGN_LENGTH);
            sz[YSF_CALLSIGN_LENGTH] = 0;
            // Get the module this peer is linked to
            CPeers *peers = g_Reflector.GetPeers();
            CPeer *peer = peers->FindPeerByIpPort(Ip, PROTOCOL_YSF);
            char ysfModule = ' ';
            if ( peer != NULL && peer->GetNbClients() > 0 )
            {
                ysfModule = peer->GetClient(0)->GetReflectorModule();
            }
            g_Reflector.ReleasePeers();

            // RPT1 uses caller's callsign + 'B' — D-Star hotspot convention.
            // See cnxdnprotocol.cpp for the rationale.
            CCallsign rpt1 = csMY;
            rpt1.SetModule('B');
            CCallsign rpt2 = m_ReflectorCallsign;
            rpt2.SetModule('G');
            
            // and packet
            *header = new CDvHeaderPacket(csMY, CCallsign("CQCQCQ"), rpt1, rpt2, uiStreamId, Fich.getFN());
        }
        // and 2 DV Frames
        {
            uint8  uiAmbe[AMBE_SIZE];
            ::memset(uiAmbe, 0x00, sizeof(uiAmbe));
            frames[0] = new CDvFramePacket(uiAmbe, uiStreamId, Fich.getFN(), 0, (uint8)0);
            frames[1] = new CDvFramePacket(uiAmbe, uiStreamId, Fich.getFN(), 1, (uint8)0);
        }
        
        // check validity of packets
        if ( ((*header) == NULL) || !(*header)->IsValid() ||
             (frames[0] == NULL) || !(frames[0]->IsValid()) ||
             (frames[1] == NULL) || !(frames[1]->IsValid()) )

        {
            delete *header;
            *header = NULL;
            delete frames[0];
            delete frames[1];
            frames[0] = NULL;
            frames[1] = NULL;
        }
        else
        {
            valid = true;
         }

    }
    
    // done
    return valid;
}

bool CYsfProtocol::IsValidDvFramePacket(const CIp &Ip, const CYSFFICH &Fich, const CBuffer &Buffer, CDvFramePacket **frames)
{
    bool valid = false;
    frames[0] = NULL;
    frames[1] = NULL;
    frames[2] = NULL;
    frames[3] = NULL;
    frames[4] = NULL;
    
    // is it DV frame ?
    if ( Fich.getFI() == YSF_FI_COMMUNICATIONS )
    {
        // Use the active per-source mapping (set by the most recent
        // header from this Ip). Falls back to the deterministic hash
        // for orphan mid-stream packets, preserving late-entry buffering.
        uint32 uiStreamId = LookupStreamIdForSource(Ip);

        // get DV frames
        uint8   ambe0[AMBEPLUS_SIZE];
        uint8   ambe1[AMBEPLUS_SIZE];
        uint8   ambe2[AMBEPLUS_SIZE];
        uint8   ambe3[AMBEPLUS_SIZE];
        uint8   ambe4[AMBEPLUS_SIZE];
        uint8 *ambes[5] = { ambe0, ambe1, ambe2, ambe3, ambe4 };
        CYsfUtils::DecodeVD2Vchs((unsigned char *)&(Buffer.data()[35]), ambes);

        // Adjust gain for YSF→DMR direction
        for (unsigned int i = 0; i < 5; i++) {
            CYsfUtils::AdjustAmbeGain(ambes[i], GAIN_YSF_TO_DMR / 10.0f);
        }

        // get DV frames
        uint8 fid = Buffer.data()[34];
        frames[0] = new CDvFramePacket(ambe0, uiStreamId, Fich.getFN(), 0, fid);
        frames[1] = new CDvFramePacket(ambe1, uiStreamId, Fich.getFN(), 1, fid);
        frames[2] = new CDvFramePacket(ambe2, uiStreamId, Fich.getFN(), 2, fid);
        frames[3] = new CDvFramePacket(ambe3, uiStreamId, Fich.getFN(), 3, fid);
        frames[4] = new CDvFramePacket(ambe4, uiStreamId, Fich.getFN(), 4, fid);
        
        // check validity of packets
        if ( (frames[0] == NULL) || !(frames[0]->IsValid()) ||
            (frames[1] == NULL) || !(frames[1]->IsValid()) ||
            (frames[2] == NULL) || !(frames[2]->IsValid()) ||
            (frames[3] == NULL) || !(frames[3]->IsValid()) ||
            (frames[4] == NULL) || !(frames[4]->IsValid()) )
        {
            delete frames[0];
            delete frames[1];
            delete frames[2];
            delete frames[3];
            delete frames[4];
            frames[0] = NULL;
            frames[1] = NULL;
            frames[2] = NULL;
            frames[3] = NULL;
            frames[4] = NULL;
        }
        else
        {
            valid = true;
        }
    }

    // done
    return valid;
}

bool CYsfProtocol::IsValidDvLastFramePacket(const CIp &Ip, const CYSFFICH &Fich, const CBuffer &Buffer, CDvFramePacket **frames)
{
    bool valid = false;
    frames[0] = NULL;
    frames[1] = NULL;
    
    // DV header ?
    if ( Fich.getFI() == YSF_FI_TERMINATOR )
    {
        // Use the active per-source mapping. Drop the mapping after we
        // build the LastFramePacket so the next header arrival from this
        // source allocates a fresh sid. NB: real YSF radios commonly
        // emit 1-3 terminator FICH frames at end-of-transmission for
        // redundancy. The first call here releases the mapping; the
        // second/third terminators fall back to the deterministic hash
        // (which won't match any open stream because OnDvLastFramePacketIn
        // already closed it), so they route into the late-entry / cancel
        // path in CProtocol::OnDvLastFramePacketIn and get dropped — no
        // duplicate EoT can reach DCS egress.
        uint32 uiStreamId = LookupStreamIdForSource(Ip);

        // get DV frames
        {
            uint8  uiAmbe[AMBE_SIZE];
            ::memset(uiAmbe, 0x00, sizeof(uiAmbe));
            frames[0] = new CDvFramePacket(uiAmbe, uiStreamId, Fich.getFN(), 0, (uint8)0);
            frames[1] = new CDvLastFramePacket(uiAmbe, uiStreamId, Fich.getFN(), 1, (uint8)0);
        }
        
        // check validity of packets
        if ( (frames[0] == NULL) || !(frames[0]->IsValid()) ||
             (frames[1] == NULL) || !(frames[1]->IsValid()) )

        {
            delete frames[0];
            delete frames[1];
            frames[0] = NULL;
            frames[1] = NULL;
        }
        else
        {
            valid = true;
        }

        // The terminator frame consumed this source's sid mapping —
        // drop it so the next header allocates fresh. Done unconditionally
        // (regardless of frame validity) because a malformed terminator
        // FICH still marks the end of this transmission from the source.
        ReleaseStreamIdForSource(Ip);
    }

    // done
    return valid;
}

////////////////////////////////////////////////////////////////////////////////////////
// DV packet encoding helpers

void CYsfProtocol::EncodeConnectAckPacket(CBuffer *Buffer) const
{
    uint8 tag[] = { 'Y','S','F','P','R','E','F','L','E','C','T','O','R',0x20 };

    Buffer->Set(tag, sizeof(tag));
}

bool CYsfProtocol::EncodeDvHeaderPacket(const CDvHeaderPacket &Header, CBuffer *Buffer) const
{
    uint8 tag[]  = { 'Y','S','F','D' };
    uint8 dest[] = { 'A','L','L',' ',' ',' ',' ',' ',' ',' ' };
    char  sz[YSF_CALLSIGN_LENGTH];
    uint8 fichd[YSF_FICH_LENGTH_BYTES];

    // tag
    Buffer->Set(tag, sizeof(tag));
    // rpt1
    ::memset(sz, ' ', sizeof(sz));
    Header.GetRpt1Callsign().GetCallsignString(sz);
    sz[::strlen(sz)] = ' ';
    Buffer->Append((uint8 *)sz, YSF_CALLSIGN_LENGTH);
    // my
    ::memset(sz, ' ', sizeof(sz));
    Header.GetMyCallsign().GetCallsignString(sz);
    sz[::strlen(sz)] = ' ';
    Buffer->Append((uint8 *)sz, YSF_CALLSIGN_LENGTH);
    // dest
    Buffer->Append(dest, 10);
    // net frame counter
    Buffer->Append((uint8)0x00);
    // FS
    Buffer->Append((uint8 *)YSF_SYNC_BYTES, YSF_SYNC_LENGTH_BYTES);
    // FICH
    CYSFFICH fich;
    fich.setFI(YSF_FI_HEADER);
    fich.setCS(2U);
    //fich.setFN(Header.GetYsfPacketId());
    fich.setFN(0U);
    fich.setFT(7U);
    fich.setDev(0U);
    fich.setMR(YSF_MR_BUSY);
    fich.setDT(YSF_DT_VD_MODE2);
    fich.setSQL(0U);
    fich.setSQ(0U);
    fich.encode(fichd);
    Buffer->Append(fichd, YSF_FICH_LENGTH_BYTES);
    // payload
    unsigned char csd1[20U], csd2[20U];
    ::memset(csd1, '*', YSF_CALLSIGN_LENGTH);
    ::memset(csd1 + YSF_CALLSIGN_LENGTH, ' ', YSF_CALLSIGN_LENGTH);
    Header.GetMyCallsign().GetCallsignString(sz);
    ::memcpy(csd1 + YSF_CALLSIGN_LENGTH, sz, ::strlen(sz));
    ::memset(csd2, ' ', YSF_CALLSIGN_LENGTH + YSF_CALLSIGN_LENGTH);
    CYSFPayload payload;
    uint8 temp[120];
    payload.writeHeader(temp, csd1, csd2);
    Buffer->Append(temp+30, 120-30);
     
    // done
    return true;
}

bool CYsfProtocol::EncodeDvPacket(const CDvHeaderPacket &Header, const CDvFramePacket *DvFrames, CBuffer *Buffer) const
{
    uint8 tag[]  = { 'Y','S','F','D' };
    uint8 dest[] = { 'A','L','L',' ',' ',' ',' ',' ',' ',' ' };
    uint8 gps[]  = { 0x52,0x22,0x61,0x5F,0x27,0x03,0x5E,0x20,0x20,0x20 };
    char  sz[YSF_CALLSIGN_LENGTH];
    uint8 fichd[YSF_FICH_LENGTH_BYTES];
    
     // tag
    Buffer->Set(tag, sizeof(tag));
    // rpt1
    ::memset(sz, ' ', sizeof(sz));
    Header.GetRpt1Callsign().GetCallsignString(sz);
    sz[::strlen(sz)] = ' ';
    Buffer->Append((uint8 *)sz, YSF_CALLSIGN_LENGTH);
    // my
    ::memset(sz, ' ', sizeof(sz));
    Header.GetMyCallsign().GetCallsignString(sz);
    sz[::strlen(sz)] = ' ';
    Buffer->Append((uint8 *)sz, YSF_CALLSIGN_LENGTH);
    // dest
    Buffer->Append(dest, 10);
    // net frame counter
    Buffer->Append(DvFrames[0].GetYsfPacketFrameId());
     // FS
    Buffer->Append((uint8 *)YSF_SYNC_BYTES, YSF_SYNC_LENGTH_BYTES);
    // FICH
    CYSFFICH fich;
    fich.setFI(YSF_FI_COMMUNICATIONS);
    fich.setCS(2U);
    fich.setFN(DvFrames[0].GetYsfPacketId());
    fich.setFT(6U);
    fich.setDev(0U);
    fich.setMR(YSF_MR_BUSY);
    fich.setDT(YSF_DT_VD_MODE2);
    fich.setSQL(0U);
    fich.setSQ(0U);
    fich.encode(fichd);
    Buffer->Append(fichd, YSF_FICH_LENGTH_BYTES);
     // payload
    CYSFPayload payload;
    uint8 temp[120];
    ::memset(temp, 0x00, sizeof(temp));
    // DV - with gain adjustment for DMR→YSF direction
    for (unsigned int i = 0; i < 5; i++)
    {
        uint8 ambe_copy[AMBEPLUS_SIZE];
        ::memcpy(ambe_copy, DvFrames[i].GetAmbePlus(), AMBEPLUS_SIZE);
        CYsfUtils::AdjustAmbeGain(ambe_copy, GAIN_DMR_TO_YSF / 10.0f);
        CYsfUtils::EncodeVD2Vch(ambe_copy, temp+35+(18*i));
    }
    // data
    switch (DvFrames[0].GetYsfPacketId())
    {
        case 0:
            // Dest
            payload.writeVDMode2Data(temp, (const unsigned char*)"**********");
            break;
        case 1:
            // Src
            ::memset(sz, ' ', sizeof(sz));
            Header.GetMyCallsign().GetCallsignString(sz);
            sz[::strlen(sz)] = ' ';
            payload.writeVDMode2Data(temp, (const unsigned char*)sz);
            break;
        case 2:
            // Down
            ::memset(sz, ' ', sizeof(sz));
            Header.GetRpt1Callsign().GetCallsignString(sz);
            sz[::strlen(sz)] = ' ';
            payload.writeVDMode2Data(temp, (const unsigned char*)sz);
            break;
        case 5:
            // Rem3+4
            // we need to provide a fake radioid for radios
            // to display src callsign
            payload.writeVDMode2Data(temp, (const unsigned char*)"     G0gBJ");
            break;
        case 6:
            // DT1
            // we need to issue a fake gps string with proper terminator
            // and crc for radios to display src callsign
            payload.writeVDMode2Data(temp, gps);
            break;
        default:
            payload.writeVDMode2Data(temp, (const unsigned char*)"          ");
            break;

    }
    Buffer->Append(temp+30, 120-30);

    // done
    return true;
}

bool CYsfProtocol::EncodeDvLastPacket(const CDvHeaderPacket &Header, CBuffer *Buffer) const
{
    uint8 tag[]  = { 'Y','S','F','D' };
    uint8 dest[] = { 'A','L','L',' ',' ',' ',' ',' ',' ',' ' };
    char  sz[YSF_CALLSIGN_LENGTH];
    uint8 fichd[YSF_FICH_LENGTH_BYTES];
    
    // tag
    Buffer->Set(tag, sizeof(tag));
    // rpt1
    ::memset(sz, ' ', sizeof(sz));
    Header.GetRpt1Callsign().GetCallsignString(sz);
    sz[::strlen(sz)] = ' ';
    Buffer->Append((uint8 *)sz, YSF_CALLSIGN_LENGTH);
    // my
    ::memset(sz, ' ', sizeof(sz));
    Header.GetMyCallsign().GetCallsignString(sz);
    sz[::strlen(sz)] = ' ';
    Buffer->Append((uint8 *)sz, YSF_CALLSIGN_LENGTH);
    // dest
    Buffer->Append(dest, 10);
    // eot status bit + net frame counter (<<1)
    Buffer->Append((uint8)0x01);
    // FS
    Buffer->Append((uint8 *)YSF_SYNC_BYTES, YSF_SYNC_LENGTH_BYTES);
    // FICH
    CYSFFICH fich;
    fich.setFI(YSF_FI_TERMINATOR);
    fich.setCS(2U);
    //fich.setFN(Header.GetYsfPacketId());
    fich.setFN(0U);
    fich.setFT(7U);
    fich.setDev(0U);
    fich.setMR(YSF_MR_BUSY);
    fich.setDT(YSF_DT_VD_MODE2);
    fich.setSQL(0U);
    fich.setSQ(0U);
    fich.encode(fichd);
    Buffer->Append(fichd, YSF_FICH_LENGTH_BYTES);
    // payload
    unsigned char csd1[20U], csd2[20U];
    ::memset(csd1, '*', YSF_CALLSIGN_LENGTH);
    ::memset(csd1 + YSF_CALLSIGN_LENGTH, ' ', YSF_CALLSIGN_LENGTH);
    Header.GetMyCallsign().GetCallsignString(sz);
    ::memcpy(csd1 + YSF_CALLSIGN_LENGTH, sz, ::strlen(sz));
    ::memset(csd2, ' ', YSF_CALLSIGN_LENGTH + YSF_CALLSIGN_LENGTH);
    CYSFPayload payload;
    uint8 temp[120];
    payload.writeHeader(temp, csd1, csd2);
    Buffer->Append(temp+30, 120-30);
    
    // done
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
// Wires-X packet decoding helpers

bool CYsfProtocol::IsValidwirexPacket(const CBuffer &Buffer, const CIp &Ip, CYSFFICH *Fich, CCallsign *Callsign, int *Cmd, int *Arg)
{
    uint8 tag[] = { 'Y','S','F','D' };
    uint8 DX_REQ[]    = {0x5DU, 0x71U, 0x5FU};
    uint8 CONN_REQ[]  = {0x5DU, 0x23U, 0x5FU};
    uint8 DISC_REQ[]  = {0x5DU, 0x2AU, 0x5FU};
    uint8 ALL_REQ[]   = {0x5DU, 0x66U, 0x5FU};
    uint8 command[300];
    CYSFPayload payload;
    bool valid = false;

    if ( (Buffer.size() == 155) && (Buffer.Compare(tag, sizeof(tag)) == 0) )
    {
        // decode YSH fich
        if ( Fich->decode(&(Buffer.data()[40])) )
        {
            //std::cout << (int)Fich->getDT() << ","
            //          << (int)Fich->getFI() << ","
            //          << (int)Fich->getFN() << ","
            //          << (int)Fich->getFT()
            //          << std::endl;
            valid = (Fich->getDT() == YSF_DT_DATA_FR_MODE);
            valid &= (Fich->getFI() == YSF_FI_COMMUNICATIONS);
            if ( valid )
            {
                // get callsign
                Callsign->SetCallsign(&(Buffer.data()[4]), CALLSIGN_LEN, false);
                Callsign->SetModule(YSF_MODULE_ID);
                // decode payload
                if ( Fich->getFN() == 0U )
                {
                    valid = false;
                }
                else if ( Fich->getFN() == 1U )
                {
                    valid &= payload.readDataFRModeData2(&(Buffer.data()[35]), command + 0U);
                }
                else
                {
                    valid &= payload.readDataFRModeData1(&(Buffer.data()[35]), command + (Fich->getFN() - 1U) * 20U + 0U);
                    if ( valid )
                    {
                        valid &= payload.readDataFRModeData2(&(Buffer.data()[35]), command + (Fich->getFN() - 1U) * 20U + 20U);
                    }
                }
                // check crc if end found.
                // endMarkerPos is captured for downstream bounds checks:
                // the response-code signature match below reads command[4]
                // and must not run if the end marker is found at a position
                // that would leave command[4] outside the CRC-validated
                // region 0..endMarkerPos+1.
                unsigned int endMarkerPos = 0U;
                if ( Fich->getFN() == Fich->getFT() )
                {
                    valid = false;
                    // Find the end marker
                    for (unsigned int i = Fich->getFN() * 20U; i > 0U; i--)
                    {
                        if (command[i] == 0x03U)
                        {
                            unsigned char crc = CCRC::addCRC(command, i + 1U);
                            if (crc == command[i + 1U])
                            {
                                valid = true;
                                endMarkerPos = i;
                            }
                            break;
                        }
                    }
                }
                // and crack the command
                if ( valid )
                {
                    // get argument
                    char buffer[4U];
                    ::memcpy(buffer, command + 5U + 2U, 3U);
                    buffer[3U] = 0x00U;
                    *Arg = ::atoi(buffer);
                    // and decode command
                    if (::memcmp(command + 1U, DX_REQ, 3U) == 0)
                    {
                        *Cmd = WIRESX_CMD_DX_REQ;
                        *Arg = 0;
                     }
                    else if (::memcmp(command + 1U, ALL_REQ, 3U) == 0)
                    {
                        // argument is start index of list
                       if ( *Arg > 0 )
                            (*Arg)--;
                        // ALL_REQ family — sub-code at command[5..6]
                        // distinguishes the variant for "long form"
                        // packets: "01" = ALL, "11" = SEARCH. Some Yaesu
                        // radios also send a SHORT form of this prefix
                        // with no sub-code at all (end marker lands
                        // before command[6]). Production observation:
                        // FTM-100D "LOCAL NEWS" uses the short form.
                        //
                        // Approach: try to match known sub-codes only
                        // when bytes are within the CRC-validated region
                        // (endMarkerPos >= 6). If no known sub-code
                        // matches — for either reason (short form OR
                        // unknown long-form sub-code) — fall into a
                        // single unified diagnostic block that logs the
                        // relevant bytes and drops via the UNKNOWN path.
                        //
                        // Pre-existing concern: the *Arg extraction at
                        // command[7..9] earlier in this function has
                        // the same endMarkerPos exposure for
                        // endMarkerPos < 10. Not addressed here; scope
                        // limited to closing the UB-on-*Cmd hole and
                        // giving us diagnostic visibility on variants.
                        bool subCodeRecognised = false;
                        if ( endMarkerPos >= 6U )
                        {
                            if ( ::memcmp(command + 5U, "01", 2) == 0 )
                            {
                                *Cmd = WIRESX_CMD_ALL_REQ;
                                subCodeRecognised = true;
                            }
                            else if ( ::memcmp(command + 5U, "11", 2) == 0 )
                            {
                                *Cmd = WIRESX_CMD_SEARCH_REQ;
                                subCodeRecognised = true;
                            }
                        }

                        if ( !subCodeRecognised )
                        {
                            // -- BEGIN INSTRUMENTATION (temporary) --
                            //
                            // Diagnostic for any unrecognised ALL_REQ
                            // family variant. Two distinct cases land
                            // here, distinguished by the log trailer:
                            //   - LONG FORM, unknown sub-code:
                            //     endMarkerPos >= 6, command[5..6] is
                            //     neither "01" nor "11". Log dumps the
                            //     two sub-code bytes as " sub XX XX".
                            //   - SHORT FORM, no sub-code at all:
                            //     endMarkerPos < 6 (end marker arrived
                            //     before command[6]). Log shows
                            //     "(short form, endMarkerPos=N)" so the
                            //     operator can see the exact packet
                            //     shape. FTM-100D "LOCAL NEWS" lives
                            //     here.
                            //
                            // Also closes a latent UB: previously *Cmd
                            // could be returned uninitialised when the
                            // sub-code dispatch fell through, and the
                            // caller would construct CWiresxCmd with
                            // garbage memory. The three "// KEEP" lines
                            // at the end of this block close that hole.
                            //
                            // Removal plan when no longer needed:
                            //   - Delete the peer-lookup block and the
                            //     std::cout block (everything above the
                            //     "// KEEP" lines).
                            //   - KEEP the three "// KEEP" lines.
                            //     Removing them reintroduces the UB.
                            //   - OR replace the whole `if (!subCode...)`
                            //     branch with proper sub-code dispatch
                            //     once we've identified all variants.
                            const char *origin = "direct client";
                            char peerLabel[64];
                            peerLabel[0] = '\0';
                            {
                                CPeers *peers = g_Reflector.GetPeers();
                                CPeer *peer = peers->FindPeerByIpPort(Ip, PROTOCOL_YSF);
                                if ( peer != NULL )
                                {
                                    char peerCs[CALLSIGN_LEN + 1];
                                    peer->GetCallsign().GetCallsignString(peerCs);
                                    ::snprintf(peerLabel, sizeof(peerLabel),
                                               "YSF peer %s", peerCs);
                                    origin = peerLabel;
                                }
                                g_Reflector.ReleasePeers();
                            }
                            char fromCs[CALLSIGN_LEN + 1];
                            Callsign->GetCallsignString(fromCs);
                            std::cout << "Wires-X command from " << fromCs
                                      << " @ " << Ip
                                      << " (" << origin << ") — code "
                                      << std::hex << std::uppercase << std::setfill('0')
                                      << std::setw(2) << (int)command[1] << " "
                                      << std::setw(2) << (int)command[2] << " "
                                      << std::setw(2) << (int)command[3]
                                      << std::dec << std::nouppercase << std::setfill(' ');
                            if ( endMarkerPos >= 6U )
                            {
                                // Long form — unknown sub-code bytes.
                                std::cout << " sub "
                                          << std::hex << std::uppercase << std::setfill('0')
                                          << std::setw(2) << (int)command[5] << " "
                                          << std::setw(2) << (int)command[6]
                                          << std::dec << std::nouppercase << std::setfill(' ');
                            }
                            else
                            {
                                // Short form — end marker before command[6].
                                std::cout << " (short form, endMarkerPos="
                                          << endMarkerPos << ")";
                            }
                            std::cout << " (UNKNOWN, Dropped)" << std::endl;
                            // -- END INSTRUMENTATION (logging) --
                            *Cmd  = WIRESX_CMD_UNKNOWN;     // KEEP (UB-closure)
                            *Arg  = 0;                       // KEEP (UB-closure)
                            valid = false;                   // KEEP (UB-closure)
                        }
                     }
                    else if (::memcmp(command + 1U, CONN_REQ, 3U) == 0)
                    {
                        *Cmd = WIRESX_CMD_CONN_REQ;
                    }
                    else if (::memcmp(command + 1U, DISC_REQ, 3U) == 0)
                    {
                        *Cmd = WIRESX_CMD_DISC_REQ;
                        *Arg = 0;
                    }
                    else
                    {
                        // All recognised request codes are handled above;
                        // everything reaching this branch is diagnostic-only
                        // and never processed. If a future Wires-X request
                        // is added (e.g. CAT_REQ '5D 67 5F' from G4KLX), it
                        // belongs in a new `else if` BEFORE this branch.
                        //
                        // Unrecognised command code at command[1..3], OR a
                        // documented server-response code that should never
                        // appear inbound (server responses are gateway->radio,
                        // not radio->gateway — a client sending one means a
                        // misconfigured peer, a relay loop, an RF bridge, a
                        // probe, or a bug somewhere upstream). Either way
                        // the packet is dropped — we never process unknown
                        // codes, and server responses don't belong in radio-
                        // to-gateway traffic.
                        //
                        // By the time we reach this branch the packet framing
                        // is fully validated (size, YSFD tag, FICH, end
                        // marker, CRC). The ONLY thing distinguishing this
                        // from a recognised request is the command code
                        // itself; everything else is known framing. The
                        // argument area at command[5..] is intentionally NOT
                        // dumped — for unknown / non-request codes its
                        // layout is undefined.
                        //
                        // The log line tags the code as UNKNOWN by default,
                        // or names the documented response (DX_RESP,
                        // CONN/DISC_RESP, ALL_RESP) if the 4-byte signature
                        // matches. Match includes the 0x26 marker at
                        // command[4] so a coincidental 5D ?? 5F with a
                        // different follow byte can't be mis-tagged as a
                        // response. Marker bytes are from
                        // g4klx/YSFClients/YSFGateway/WiresX.cpp.
                        //
                        // Peer-vs-client classification: look up the source
                        // IP+port in the registered peer list. The caller
                        // performs the same lookup immediately after we
                        // return (to decide whether to enqueue or silently
                        // ignore Wires-X from peers); doing it here just
                        // attaches the context to the log line.
                        static const uint8 DX_RESP[]   = {0x5DU, 0x51U, 0x5FU, 0x26U};
                        static const uint8 CONN_RESP[] = {0x5DU, 0x41U, 0x5FU, 0x26U};
                        static const uint8 ALL_RESP[]  = {0x5DU, 0x46U, 0x5FU, 0x26U};

                        // Response signatures are 4 bytes; gate on
                        // endMarkerPos so command[4] is always within the
                        // CRC-validated initialised region. A degenerate
                        // packet with the end marker at position < 4 cannot
                        // be a real response anyway (the response framing
                        // alone occupies command[1..4]), so refusing to
                        // tag it as one is correct as well as safe.
                        const char *cmdTag = "UNKNOWN";
                        if ( endMarkerPos >= 4U )
                        {
                            if ( ::memcmp(command + 1U, DX_RESP, 4U) == 0 )
                                cmdTag = "DX_RESP";
                            else if ( ::memcmp(command + 1U, CONN_RESP, 4U) == 0 )
                                cmdTag = "CONN/DISC_RESP";
                            else if ( ::memcmp(command + 1U, ALL_RESP, 4U) == 0 )
                                cmdTag = "ALL_RESP";
                        }

                        const char *origin = "direct client";
                        char peerLabel[64];
                        peerLabel[0] = '\0';
                        {
                            CPeers *peers = g_Reflector.GetPeers();
                            CPeer *peer = peers->FindPeerByIpPort(Ip, PROTOCOL_YSF);
                            if ( peer != NULL )
                            {
                                char peerCs[CALLSIGN_LEN + 1];
                                peer->GetCallsign().GetCallsignString(peerCs);
                                ::snprintf(peerLabel, sizeof(peerLabel),
                                           "YSF peer %s", peerCs);
                                origin = peerLabel;
                            }
                            g_Reflector.ReleasePeers();
                        }
                        char fromCs[CALLSIGN_LEN + 1];
                        Callsign->GetCallsignString(fromCs);
                        std::cout << "Wires-X command from " << fromCs
                                  << " @ " << Ip
                                  << " (" << origin << ") — code "
                                  << std::hex << std::uppercase << std::setfill('0')
                                  << std::setw(2) << (int)command[1] << " "
                                  << std::setw(2) << (int)command[2] << " "
                                  << std::setw(2) << (int)command[3]
                                  << std::dec << std::nouppercase << std::setfill(' ')
                                  << " (" << cmdTag << ", Dropped)"
                                  << std::endl;
                        *Cmd = WIRESX_CMD_UNKNOWN;
                        *Arg = 0;
                        valid = false;
                    }
                }
            }
        }
    }
    return valid;
}

// server status packet decoding helpers

bool CYsfProtocol::IsValidServerStatusPacket(const CBuffer &Buffer) const
{
    uint8 tag[] = { 'Y','S','F','S' };
     
    return ( (Buffer.size() >= 4) && (Buffer.Compare(tag, sizeof(tag)) == 0) );
}

// server status packet encoding helpers

bool CYsfProtocol::EncodeServerStatusPacket(CBuffer *Buffer) const
{
    uint8 tag[] = { 'Y','S','F','S' };
    uint8 description[] = { 'X','L','X',' ','r','e','f','l','e','c','t','o','r',' ' };
    uint8 callsign[16];
     
    // tag
    Buffer->Set(tag, sizeof(tag));
    // hash
    ::memset(callsign, ' ', sizeof(callsign));
    g_Reflector.GetCallsign().GetCallsign(callsign);
    char sz[16];
    ::sprintf(sz, "%05u", CalcHash(callsign, 16) % 100000U);
    Buffer->Append((uint8 *)sz, 5);
    // name
    Buffer->Append(callsign, 16);
    // desscription
    Buffer->Append(description, 14);
    // connected clients
    CClients *clients = g_Reflector.GetClients();
    int count = MIN(999, clients->GetSize());
    g_Reflector.ReleaseClients();
    ::sprintf(sz, "%03u", count);
    Buffer->Append((uint8 *)sz, 3);
    
    // done
    return true;
}

uint32 CYsfProtocol::CalcHash(const uint8 *buffer, int len) const
{
    uint32 hash = 0U;

    for ( int i = 0; i < len; i++)
    {
        hash += buffer[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);

    return hash;
}


////////////////////////////////////////////////////////////////////////////////////////
// uiStreamId helpers

uint32 CYsfProtocol::IpToStreamId(const CIp &ip) const
{
    return ip.GetAddr() ^ (uint32)(MAKEDWORD(ip.GetPort(), ip.GetPort()));
}

// Build the map key. (addr<<16)|port keeps the same source endpoint
// keyed identically for header / voice / terminator frames within one
// transmission, and distinguishes localhost peers using different
// ephemeral ports.
static inline uint64_t YsfSourceKey(const CIp &ip)
{
    return ((uint64_t)ip.GetAddr() << 16) | (uint64_t)(ip.GetPort() & 0xFFFFu);
}

// Allocate a fresh reflector-side stream-id for a NEW header arrival.
// Pulls the nonce from the process-wide CProtocol::AllocateGlobalStreamIdNonce
// — see cprotocol.cpp for the rationale (random seed at startup, shared
// across protocols to avoid inter-protocol sid collisions). Defensively
// retries if the candidate happens to collide with another live source's
// mapped sid in THIS protocol's m_SourceStates (only possible across a
// 65535-allocation wraparound where an earlier entry was never released
// — bounded growth on dropped terminators, see header comment).
uint32 CYsfProtocol::AllocateNewStreamIdForSource(const CIp &ip)
{
    std::lock_guard<std::mutex> lock(m_SourceStatesMutex);

    const uint64_t key = YsfSourceKey(ip);

    // Try up to 65535 attempts; in practice the first attempt almost
    // always succeeds because the active-source set is small.
    for (int attempts = 0; attempts < 65535; attempts++)
    {
        uint32 candidate = (uint32)AllocateGlobalStreamIdNonce();

        bool collision = false;
        for (const auto &entry : m_SourceStates)
        {
            if (entry.first != key && entry.second == candidate)
            {
                collision = true;
                break;
            }
        }
        if (!collision)
        {
            m_SourceStates[key] = candidate;
            return candidate;
        }
    }

    // Pathological fallback (would require 65535 simultaneous active
    // sources within this protocol, which is impossible on a single
    // UDP port). Fall back to the deterministic hash so we still
    // produce SOMETHING valid.
    uint32 fallback = IpToStreamId(ip);
    m_SourceStates[key] = fallback;
    return fallback;
}

// Look up the active sid for an inbound voice / terminator frame.
// Falls back to the deterministic IpToStreamId() when no header has
// been seen for this source — preserves the late-entry buffering path
// in CProtocol::OnDvFramePacketIn for orphan mid-stream packets.
uint32 CYsfProtocol::LookupStreamIdForSource(const CIp &ip) const
{
    std::lock_guard<std::mutex> lock(m_SourceStatesMutex);
    auto it = m_SourceStates.find(YsfSourceKey(ip));
    if (it != m_SourceStates.end())
    {
        return it->second;
    }
    return IpToStreamId(ip);
}

// Drop the IP→sid mapping after the terminator frame is constructed.
// The next header from this source will allocate a fresh sid.
void CYsfProtocol::ReleaseStreamIdForSource(const CIp &ip)
{
    std::lock_guard<std::mutex> lock(m_SourceStatesMutex);
    m_SourceStates.erase(YsfSourceKey(ip));
}

////////////////////////////////////////////////////////////////////////////////////////
// debug

#ifdef DEBUG_DUMPFILE
bool CYsfProtocol::DebugTestDecodePacket(const CBuffer &Buffer)
{
    uint8 tag[] = { 'Y','S','F','D' };
    static uint8 command[4098];
    static int len;
    CYSFFICH Fich;
    CYSFPayload payload;
    CBuffer dump;
    bool valid = false;
    
    if ( (Buffer.size() == 155) && (Buffer.Compare(tag, sizeof(tag)) == 0) )
    {
        // decode YSH fich
        if ( Fich.decode(&(Buffer.data()[40])) )
        {
            std::cout << (int)Fich.getDT() << ","
                      << (int)Fich.getFI() << ","
                      << (int)Fich.getBN() << ","
                      << (int)Fich.getBT() << ","
                      << (int)Fich.getFN() << ","
                      << (int)Fich.getFT() << " : ";
            
            switch ( Fich.getFI() )
            {
                case YSF_FI_HEADER:
                    len = 0;
                    ::memset(command, 0x00, sizeof(command));
                    std::cout << "Header" << std::endl;
                    break;
                case YSF_FI_TERMINATOR:
                    std::cout << "Trailer" << std::endl;
                    std::cout << "length of payload : " << len << std::endl;
                    dump.Set(command, len);
                    dump.DebugDump(g_Reflector.m_DebugFile);
                    dump.DebugDumpAscii(g_Reflector.m_DebugFile);
                    break;
                case YSF_FI_COMMUNICATIONS:
                    if ( Fich.getDT() == YSF_DT_DATA_FR_MODE )
                    {
                        valid = payload.readDataFRModeData1(&(Buffer.data()[35]), command + len);
                        len += 20;
                        valid &= payload.readDataFRModeData2(&(Buffer.data()[35]), command + len);
                        len += 20;
                        std::cout << "decoded ok" << std::endl;
                    }
                    break;
            }
        }
        else
        {
            std::cout << "invalid fich in packet" << std::endl;
        }
    }
    else
    {
        std::cout << "invalid size packet" << std::endl;
    }
    return valid;
}
#endif


bool CYsfProtocol::DebugDumpHeaderPacket(const CBuffer &Buffer)
{
    bool ok;
    CYSFFICH fich;
    CYSFPayload payload;
    uint8 data[200];

    :: memset(data, 0, sizeof(data));
    

    ok = IsValidDvPacket(Buffer, &fich);
    if ( ok && (fich.getFI() == YSF_FI_HEADER) )
    {
        ok &= payload.processHeaderData((unsigned char *)&(Buffer.data()[35]));
    }
    
    std::cout << "HD-" <<(ok ? "ok " : "xx ") << "src: " << payload.getSource() << "dest: " << payload.getDest() << std::endl;

    return ok;
}

bool CYsfProtocol::DebugDumpDvPacket(const CBuffer &Buffer)
{
    bool ok;
    CYSFFICH fich;
    CYSFPayload payload;
    uint8 data[200];

    :: memset(data, 0, sizeof(data));

    ok = IsValidDvPacket(Buffer, &fich);
    if ( ok && (fich.getFI() == YSF_FI_COMMUNICATIONS) )
    {
        ok &= payload.readVDMode2Data(&(Buffer.data()[35]), data);
    }

    std::cout << "DV-" <<(ok ? "ok " : "xx ") << "FN:" << (int)fich.getFN() << "  payload: " << (char *)data << std::endl;
 
    return ok;
}

bool CYsfProtocol::DebugDumpLastDvPacket(const CBuffer &Buffer)
{
    bool ok;
    CYSFFICH fich;
    CYSFPayload payload;
    uint8 data[200];

    :: memset(data, 0, sizeof(data));


    ok = IsValidDvPacket(Buffer, &fich);
    if ( ok && (fich.getFI() == YSF_FI_TERMINATOR) )
    {
        ok &= payload.processHeaderData((unsigned char *)&(Buffer.data()[35]));
    }

    std::cout << "TC-" <<(ok ? "ok " : "xx ") << "src: " << payload.getSource() << "dest: " << payload.getDest() << std::endl;

    return ok;
}

////////////////////////////////////////////////////////////////////////////////////////
// YSF peer helpers

void CYsfProtocol::HandleYsfPeerLinks(void)
{
    CBuffer buffer;

    // Snapshot the peer list under the GK lock, then release BEFORE acquiring
    // Peers lock. Prevents ABBA: RX holds Clients → GK (via MayLink).
    std::vector<CCallsignListItem> peerListSnapshot;
    {
        CPeerCallsignList *list = g_GateKeeper.GetPeerList();
        peerListSnapshot.assign(list->begin(), list->end());
        g_GateKeeper.ReleasePeerList();
    }

    CPeers *peers = g_Reflector.GetPeers();

    // check if all our connected YSF peers are still listed by gatekeeper
    // collect peers to remove (RemovePeer erases from vector, invalidating iterator)
    std::vector<CPeer *> toRemove;
    int index = -1;
    CPeer *peer = NULL;
    while ( (peer = peers->FindNextPeer(PROTOCOL_YSF, &index)) != NULL )
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
            toRemove.push_back(peer);
        }
    }
    for ( size_t i = 0; i < toRemove.size(); i++ )
    {
        std::cout << "YSF peer " << toRemove[i]->GetCallsign() << " removed - no longer in peer list" << std::endl;
        peers->RemovePeer(toRemove[i]);
    }

    // check if all YSF peers listed by gatekeeper are connected
    // if not, connect
    for ( size_t i = 0; i < peerListSnapshot.size(); i++ )
    {
        CCallsignListItem *item = &peerListSnapshot[i];

        // Only process YSF peers (callsign starts with "YSF")
        if ( !IsYsfPeerCallsign(item->GetCallsign()) )
        {
            continue;
        }

        if ( peers->FindPeer(item->GetCallsign(), PROTOCOL_YSF) == NULL )
        {
            // resolve again peer's IP in case it's a dynamic IP
            item->ResolveIp();

            // get the port (custom or default)
            uint16 port = item->GetPort();
            if ( port == 0 )
            {
                port = YSF_PORT;
            }

            // send poll packet to initiate connection
            std::cout << "YSF connecting to peer " << item->GetCallsign()
                      << " @ " << item->GetIp() << ":" << port
                      << " on module " << item->GetModules() << std::endl;
            EncodePollPacket(&buffer, g_Reflector.GetCallsign());
            m_Socket.Send(buffer, item->GetIp(), port);
        }
    }

    // done
    g_Reflector.ReleasePeers();
}

void CYsfProtocol::HandleYsfPeerKeepalives(void)
{
    CBuffer buffer;

    // Snapshot peer list under GK lock, release before Peers lock
    std::vector<CCallsignListItem> peerListSnapshot;
    {
        CPeerCallsignList *list = g_GateKeeper.GetPeerList();
        peerListSnapshot.assign(list->begin(), list->end());
        g_GateKeeper.ReleasePeerList();
    }

    CPeers *peers = g_Reflector.GetPeers();

    // send keepalive to all connected YSF peers, collect timed-out ones for removal
    std::vector<CPeer *> toRemove;
    int index = -1;
    CPeer *peer = NULL;
    while ( (peer = peers->FindNextPeer(PROTOCOL_YSF, &index)) != NULL )
    {
        CYsfPeer *ysfPeer = (CYsfPeer *)peer;

        // check if still alive
        if ( !ysfPeer->IsAlive() )
        {
            toRemove.push_back(peer);
        }
        else
        {
            // find the port for this peer from snapshot
            CCallsignListItem *item = NULL;
            for ( auto &snapItem : peerListSnapshot )
            {
                if ( snapItem.GetCallsign().HasSameCallsign(peer->GetCallsign()) )
                {
                    item = &snapItem;
                    break;
                }
            }
            uint16 port = YSF_PORT;
            if ( item != NULL && item->GetPort() != 0 )
            {
                port = item->GetPort();
            }

            // send keepalive poll
            EncodePollPacket(&buffer, g_Reflector.GetCallsign());
            m_Socket.Send(buffer, peer->GetIp(), port);
        }
    }
    for ( size_t i = 0; i < toRemove.size(); i++ )
    {
        std::cout << "YSF peer " << toRemove[i]->GetCallsign() << " keepalive timeout" << std::endl;
        peers->RemovePeer(toRemove[i]);
    }

    // done
    g_Reflector.ReleasePeers();
}

bool CYsfProtocol::IsYsfPeerCallsign(const CCallsign &callsign) const
{
    char cs[CALLSIGN_LEN + 1];
    callsign.GetCallsign((uint8 *)cs);
    cs[CALLSIGN_LEN] = '\0';
    return (::strncmp(cs, "YSF", 3) == 0);
}

CCallsignListItem *CYsfProtocol::FindYsfPeerByIp(CPeerCallsignList *list, const CIp &ip)
{
    // Search for a YSF peer matching this IP.
    //
    // Match on (address, port) when the interlink entry declares an explicit
    // port (item->GetPort() != 0). This disambiguates two YSF peers sharing
    // an IP address on different listening ports — the inbound source port
    // for YSF is the peer reflector's listening port (server-style protocol),
    // so comparing against the file-declared port routes each peer correctly.
    //
    // When the interlink entry has no explicit port, fall back to address-
    // only matching (preserves the single-peer-per-IP behaviour that existed
    // before this disambiguation was added). Operators who want multi-peer-
    // per-IP must specify ports in xlxd.interlink.
    //
    // m_uiPort is host byte order (set via atoi at parse time); CIp::GetPort
    // returns sin_port in network byte order — hence htons() at compare.
    for ( int i = 0; i < list->size(); i++ )
    {
        CCallsignListItem *item = &((list->data())[i]);

        // Check if this is a YSF peer (callsign starts with "YSF")
        if ( IsYsfPeerCallsign(item->GetCallsign()) )
        {
            if ( item->GetIp().GetAddr() == ip.GetAddr() )
            {
                uint16 itemPort = item->GetPort();
                if ( itemPort == 0 || ::htons(itemPort) == ip.GetPort() )
                {
                    return item;
                }
            }
        }
    }
    return NULL;
}

void CYsfProtocol::EncodePollPacket(CBuffer *buffer, const CCallsign &callsign) const
{
    uint8 tag[] = { 'Y','S','F','P' };
    char cs[YSF_CALLSIGN_LENGTH];

    // tag
    buffer->Set(tag, sizeof(tag));

    // callsign (10 bytes, space padded)
    ::memset(cs, ' ', sizeof(cs));
    callsign.GetCallsign((uint8 *)cs);
    buffer->Append((uint8 *)cs, YSF_CALLSIGN_LENGTH);
}
