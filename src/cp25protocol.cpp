//
//  cp25protocol.cpp
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

#include "main.h"
#include <string.h>
#include "cp25protocol.h"
#include "creflector.h"
#include "cgatekeeper.h"
#include "cp25peer.h"
#include "cp25peerclient.h"
#include "cpeercallsignlist.h"
#include "cdmriddirfile.h"
#include "cdmriddirhttp.h"

////////////////////////////////////////////////////////////////////////////////////////
// constructor

CP25Protocol::CP25Protocol()
{
    for (int i = 0; i < NB_OF_MODULES; i++)
    {
        m_StreamsCache[i].m_uiFrameCount = 0;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// operation

bool CP25Protocol::Init(void)
{
    bool ok;

    // base class
    ok = CProtocol::Init();

    // update the reflector callsign
    m_ReflectorCallsign.PatchCallsign(0, (const uint8 *)"P25", 3);

    // create our socket with ephemeral port (peer-only, outbound connections)
    ok &= m_Socket.Open(0);

    // update time
    m_LastPeerLinkTime.Now();
    m_LastPeerKeepaliveTime.Now();

    // report
    if ( ok )
    {
        std::cout << "P25 protocol initialized (peer-only mode)" << std::endl;
    }

    // done
    return ok;
}

void CP25Protocol::Close(void)
{
    // base class
    CProtocol::Close();
}

////////////////////////////////////////////////////////////////////////////////////////
// task

void CP25Protocol::RxTask(void)
{
    CBuffer     Buffer;
    CIp         Ip;
    CCallsign   Callsign;

    // handle incoming packets
    if ( m_Socket.Receive(&Buffer, &Ip, 20) != -1 )
    {
        // get first byte to determine packet type
        if ( Buffer.size() >= 1 )
        {
            uint8 cmd = Buffer.data()[0];

            if ( cmd == P25_CMD_POLL || cmd == P25_CMD_UNLINK )
            {
                // This is a poll response from a P25 reflector we connected to
                if ( IsValidPollPacket(Buffer, &Callsign) )
                {
                    // Check if peer exists — release Peers before any GK access
                    bool peerExists = false;
                    {
                        CPeers *peers = g_Reflector.GetPeers();
                        CPeer *existingPeer = peers->FindPeerByIpPort(Ip, PROTOCOL_P25);
                        if ( existingPeer != NULL )
                        {
                            existingPeer->Alive();
                            peerExists = true;
                        }
                        g_Reflector.ReleasePeers();
                    }

                    if ( peerExists )
                    {
                        // Also keep the corresponding client alive
                        CClients *clients = g_Reflector.GetClients();
                        int index = -1;
                        CClient *client = NULL;
                        while ( (client = clients->FindNextClient(PROTOCOL_P25, &index)) != NULL )
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
                        // Check if this is from a P25 peer in our peer list
                        // Snapshot GK data first, release GK, then acquire Peers+Clients
                        CCallsign peerCallsign;
                        char peerModule = ' ';
                        uint32_t p25Tg = 0;
                        bool foundInGk = false;
                        {
                            CPeerCallsignList *list = g_GateKeeper.GetPeerList();
                            CCallsignListItem *item = FindP25PeerByIp(list, Ip);
                            if ( item != NULL )
                            {
                                peerCallsign = item->GetCallsign();
                                peerModule = item->GetModules()[0];
                                char csStr[CALLSIGN_LEN + 1];
                                item->GetCallsign().GetCallsign((uint8 *)csStr);
                                csStr[CALLSIGN_LEN] = '\0';
                                p25Tg = (uint32_t)::atoi(csStr + 3);
                                foundInGk = true;
                            }
                            g_GateKeeper.ReleasePeerList();
                        }

                        if ( foundInGk )
                        {
                            // Create the P25 peer object with its client attached
                            // (AddPeer registers the client; RemovePeer cleans it up)
                            CVersion version(1, 0, 0);
                            char modules[2] = { peerModule, '\0' };

                            CP25Peer *newPeer = new CP25Peer(peerCallsign, Ip, modules, version);
                            newPeer->Alive();
                            newPeer->SetP25Tg(p25Tg);

                            std::cout << "P25 peer " << peerCallsign << " connected on module " << peerModule << std::endl;

                            // AddPeer registers peer and its clients in the reflector
                            CPeers *peers = g_Reflector.GetPeers();
                            peers->AddPeer(newPeer);
                            g_Reflector.ReleasePeers();
                        }
                    }
                }
            }
            else if ( cmd == P25_REC_LDU1_VOICE1 )
            {
                // Start of LDU1 - beginning of voice frame
                OnLduPacketIn(Buffer, Ip, true);
            }
            else if ( (cmd >= P25_REC_LDU1_VOICE2 && cmd <= P25_REC_LDU1_VOICE9) ||
                      (cmd >= P25_REC_LDU2_VOICE1 && cmd <= P25_REC_LDU2_VOICE9) )
            {
                // Voice continuation packet
                OnLduPacketIn(Buffer, Ip, false);
            }
            else if ( cmd == P25_REC_TERM )
            {
                // End of transmission
                OnTerminatorPacketIn(Ip);
            }
        }
    }

    // handle end of streaming timeout
    CheckStreamsTimeout();

}

////////////////////////////////////////////////////////////////////////////////////////
// TX task — runs on separate thread, never blocked by CloseStream

void CP25Protocol::TxTask(void)
{
    {
        std::unique_lock<std::mutex> lk(m_QueueCondMutex);
        m_QueueCondVar.wait_for(lk, std::chrono::milliseconds(20));
    }

    HandleQueue();

    if ( m_LastPeerLinkTime.DurationSinceNow() > P25_PEER_RECONNECT_PERIOD )
    {
        HandlePeerLinks();
        m_LastPeerLinkTime.Now();
    }

    if ( m_LastPeerKeepaliveTime.DurationSinceNow() > P25_PEER_KEEPALIVE_PERIOD )
    {
        HandlePeerKeepalives();
        m_LastPeerKeepaliveTime.Now();
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// packet handlers

void CP25Protocol::OnLduPacketIn(const CBuffer &Buffer, const CIp &Ip, bool isFirst)
{
    // Gather peer data under m_Peers lock, then release before calling
    // OnDvHeaderPacketIn/OnDvFramePacketIn which may acquire m_Clients.
    // Never hold both locks simultaneously to prevent deadlock.
    bool peerFound = false;
    char module = ' ';
    uint16_t peerTg = 0;

    {
        CPeers *peers = g_Reflector.GetPeers();
        CPeer *peer = peers->FindPeerByIpPort(Ip, PROTOCOL_P25);
        if ( peer != NULL )
        {
            peerFound = true;
            if ( peer->GetNbClients() > 0 )
            {
                module = peer->GetClient(0)->GetReflectorModule();
            }
            CP25Peer *p25Peer = dynamic_cast<CP25Peer *>(peer);
            if ( p25Peer != NULL )
            {
                peerTg = p25Peer->GetP25Tg();
            }
        }
        g_Reflector.ReleasePeers();
    }

    if ( !peerFound )
    {
        return;
    }

    uint8 cmd = Buffer.data()[0];

    // Lock order: Peers (acquired/released above for the peer check) →
    // m_SourceStatesMutex (taken inside the Allocate/Lookup helpers).
    // No reflector lock is held here at the call sites below, but if a
    // future change adds one, do NOT take m_SourceStatesMutex first and
    // then call into the reflector — that would invert this order and
    // risk deadlock with the OnTerminatorPacketIn path which holds Peers
    // while calling Lookup/Release.
    //
    // Allocate / look up the reflector-side stream-id. P25 is unlike
    // NXDN/YSF here — it has no explicit start-of-transmission frame
    // type. LDU1 (containing Voice1..Voice9) and LDU2 alternate every
    // ~180 ms throughout the transmission, so LDU1_VOICE1 fires every
    // 360 ms even mid-transmission. Allocating a fresh sid on every
    // LDU1_VOICE1 (the previous behaviour) cut a single P25
    // transmission into many short substreams — each new sid opened
    // a new xlxd stream that expired without a TDU. That spammed the
    // gatekeeper's strike-based loop-detector log line on every
    // physical transmission. (Note: that strike accumulation does
    // NOT actually block P25 TX — see the IsCallsignLoopBlocked
    // check on header construction below, which is what enforces
    // the block for P25 today.) So allocate ONLY when there is no
    // active mapping for this source (true start-of-transmission);
    // otherwise reuse the existing mapping (mid-transmission LDU1
    // cycle). The mapping is released by OnTerminatorPacketIn when
    // the TDU arrives, so the next LDU1_VOICE1 after a TDU is
    // correctly recognised as a new transmission.
    uint32 uiStreamId;
    if ( cmd == P25_REC_LDU1_VOICE1 && !HasActiveStreamIdForSource(Ip) )
    {
        uiStreamId = AllocateNewStreamIdForSource(Ip);
    }
    else
    {
        uiStreamId = LookupStreamIdForSource(Ip);
    }

    // Extract IMBE frame based on record type
    uint8 uiImbe[P25_IMBE_SIZE];
    if ( !ExtractImbeFromRecord(Buffer, uiImbe) )
    {
        return;
    }

    // Extract src/dst from specific records
    uint32_t srcId = 0;
    uint32_t dstId = 0;
    if ( cmd == P25_REC_LDU1_VOICE4 && Buffer.size() >= 4 )  // 0x65 has dst
    {
        dstId = (Buffer.data()[1] << 16) | (Buffer.data()[2] << 8) | Buffer.data()[3];
    }
    else if ( cmd == P25_REC_LDU1_VOICE5 && Buffer.size() >= 4 )  // 0x66 has src
    {
        srcId = (Buffer.data()[1] << 16) | (Buffer.data()[2] << 8) | Buffer.data()[3];
    }

    int iModId = g_Reflector.GetModuleIndex(module);

    // Store source/dest IDs if we got them (under slot lock — TX thread reads these)
    if ( iModId >= 0 && iModId < NB_OF_MODULES )
    {
        if ( srcId != 0 || dstId != 0 )
        {
            std::lock_guard<std::mutex> lock(m_StreamsCache[iModId].m_Mutex);
            if ( srcId != 0 )
                m_StreamsCache[iModId].m_uiSrcId = srcId;
            if ( dstId != 0 )
                m_StreamsCache[iModId].m_uiDstTg = dstId;
        }
    }

    // Check if this is LDU1 voice 1 (0x62) - start of new transmission
    if ( cmd == P25_REC_LDU1_VOICE1 )
    {
        // Check if we need to start a new stream
        CPacketStream *stream = GetStream(uiStreamId);
        if ( stream == NULL && iModId >= 0 && iModId < NB_OF_MODULES )
        {
            // Defer header creation until Voice5 (0x66) when source ID arrives
            std::lock_guard<std::mutex> lock(m_StreamsCache[iModId].m_Mutex);
            m_StreamsCache[iModId].ClearPendingFrames();
            m_StreamsCache[iModId].m_bPendingHeader = true;
            m_StreamsCache[iModId].m_uiPendingStreamId = uiStreamId;
            m_StreamsCache[iModId].m_PendingIp = Ip;
        }
    }

    // If we're deferring header creation, buffer this frame
    bool isPending = false;
    if ( iModId >= 0 && iModId < NB_OF_MODULES )
    {
        std::lock_guard<std::mutex> lock(m_StreamsCache[iModId].m_Mutex);
        isPending = m_StreamsCache[iModId].m_bPendingHeader;
    }

    if ( isPending )
    {
        // Check if this is Voice5 with source ID - time to create the header
        if ( cmd == P25_REC_LDU1_VOICE5 )
        {
            // Resolve callsign from source ID
            CCallsign csMY;
            bool foundCallsign = false;
            if ( srcId != 0 )
            {
                foundCallsign = P25IdToCallsign(srcId, &csMY);
            }
            if ( !foundCallsign )
            {
                // Fallback: use peer's TG as source identifier
                char szCallsign[16];
                ::snprintf(szCallsign, sizeof(szCallsign), "P%05u", peerTg);
                csMY.SetCallsign(szCallsign);
            }

            // RPT1 uses caller's callsign + 'B' — D-Star hotspot convention.
            // See cnxdnprotocol.cpp for the rationale.
            CCallsign rpt1 = csMY;
            rpt1.SetModule('B');
            CCallsign rpt2 = m_ReflectorCallsign;
            rpt2.SetModule('G');

            CDvHeaderPacket *header = new CDvHeaderPacket(csMY, CCallsign("CQCQCQ"), rpt1, rpt2, uiStreamId, 0);
            header->SetMySuffix("P25");

            // Snapshot pending frames under lock, then replay without lock
            // (OnDvHeaderPacketIn/OnDvFramePacketIn acquire Clients lock)
            std::vector<CDvFramePacket *> pendingCopy;
            {
                std::lock_guard<std::mutex> lock(m_StreamsCache[iModId].m_Mutex);
                pendingCopy.swap(m_StreamsCache[iModId].m_PendingFrames);
                m_StreamsCache[iModId].m_bPendingHeader = false;
            }

            // Per-callsign loop block. P25 has no MayTransmit call in
            // its path (peer-only protocol, peer link is trusted), so
            // this is the sole enforcement point. Without it the
            // gatekeeper's strike accumulation for P25 is dead state.
            // Synthetic callsigns from P25IdToCallsign / the "P<TG>"
            // fallback are stable per-source, so the strike-counter
            // and per-callsign map work the same as for YSF/NXDN.
            if ( g_GateKeeper.IsCallsignLoopBlocked(csMY, "P25 peer") )
            {
                // Drop header, pending frames (already swapped out of
                // m_PendingFrames), and the current Voice5 frame.
                //
                // Release the per-source stream-id mapping so the next
                // LDU1_VOICE1 from this source re-allocates a fresh
                // SID and re-enters this code path (rather than taking
                // the LookupStreamIdForSource branch and feeding
                // frames into OnDvFramePacketIn with no open stream,
                // which would orphan them silently). On block expiry,
                // the first LDU after the expiry then opens cleanly.
                ReleaseStreamIdForSource(Ip);
                delete header;
                for (size_t i = 0; i < pendingCopy.size(); i++)
                {
                    delete pendingCopy[i];
                }
                return;
            }

            OnDvHeaderPacketIn(header, Ip);

            // Replay buffered frames (ownership transfers to OnDvFramePacketIn)
            for (size_t i = 0; i < pendingCopy.size(); i++)
            {
                OnDvFramePacketIn(pendingCopy[i], &Ip);
            }

            // Process this frame (Voice5) normally
            CDvFramePacket *frame = new CDvFramePacket(uiStreamId, uiImbe);
            OnDvFramePacketIn(frame, &Ip);
        }
        else
        {
            // Buffer this frame until we get the source ID (cap at 36 to prevent unbounded growth)
            std::lock_guard<std::mutex> lock(m_StreamsCache[iModId].m_Mutex);
            if ( m_StreamsCache[iModId].m_PendingFrames.size() < 36 )
            {
                CDvFramePacket *frame = new CDvFramePacket(uiStreamId, uiImbe);
                m_StreamsCache[iModId].m_PendingFrames.push_back(frame);
            }
        }
    }
    else if ( iModId >= 0 && iModId < NB_OF_MODULES )
    {
        // Normal processing - stream already open or no pending header
        CDvFramePacket *frame = new CDvFramePacket(uiStreamId, uiImbe);
        OnDvFramePacketIn(frame, &Ip);
    }
}

void CP25Protocol::OnTerminatorPacketIn(const CIp &Ip)
{
    // Lock order: Peers → m_SourceStatesMutex. Lookup and Release are
    // called below while the Peers lock acquired here is still held.
    // Same caveat as OnLduPacketIn — do not call into the reflector
    // from inside the Allocate/Lookup/Release helpers, since that would
    // invert this order.
    CPeers *peers = g_Reflector.GetPeers();
    CPeer *peer = peers->FindPeerByIpPort(Ip, PROTOCOL_P25);

    if ( peer != NULL )
    {
        // Clean up any pending header state
        char module = ' ';
        if ( peer->GetNbClients() > 0 )
        {
            module = peer->GetClient(0)->GetReflectorModule();
        }
        int iModId = g_Reflector.GetModuleIndex(module);
        if ( iModId >= 0 && iModId < NB_OF_MODULES )
        {
            std::lock_guard<std::mutex> lock(m_StreamsCache[iModId].m_Mutex);
            m_StreamsCache[iModId].ClearPendingFrames();
        }

        uint32 uiStreamId = LookupStreamIdForSource(Ip);
        CPacketStream *stream = GetStream(uiStreamId);
        if ( stream != NULL )
        {
            uint8 emptyImbe[P25_IMBE_SIZE];
            ::memset(emptyImbe, 0, sizeof(emptyImbe));
            CDvLastFramePacket *lastFrame = new CDvLastFramePacket(emptyImbe, uiStreamId, (uint8)0, (uint8)0, (uint8)0);

            // Drop the IP→sid mapping unconditionally — the next
            // start-of-transmission (LDU1 Voice1) from this source will
            // allocate a fresh sid. Done before releasing peers so we
            // can't take the OnDvLastFramePacketIn path with a stale
            // mapping if peers-release races with another inbound TDU.
            ReleaseStreamIdForSource(Ip);

            g_Reflector.ReleasePeers();
            OnDvLastFramePacketIn(lastFrame, &Ip);
            return;
        }
        // No matching stream — still release the mapping so a duplicate
        // TDU doesn't leave a stale sid pinned for this source.
        ReleaseStreamIdForSource(Ip);
    }
    g_Reflector.ReleasePeers();
}

////////////////////////////////////////////////////////////////////////////////////////
// streams helpers

bool CP25Protocol::OnDvHeaderPacketIn(CDvHeaderPacket *Header, const CIp &Ip)
{
    bool newstream = false;

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
        CClient *client = NULL;

        // Look for matching P25 peer client
        int index = -1;
        CClient *c = NULL;
        while ( (c = clients->FindNextClient(PROTOCOL_P25, &index)) != NULL )
        {
            if ( c->IsPeer() && (c->GetIp() == Ip) )
            {
                client = c;
                break;
            }
        }

        if ( client != NULL )
        {
            // get client callsign
            via = client->GetCallsign();

            // set module from client's linked module
            Header->SetRpt2Module(client->GetReflectorModule());

            // save header callsigns before OpenStream — OpenStream transfers
            // Header into the router queue, where the router thread may delete it
            myCallsign = Header->GetMyCallsign();
            rpt2Callsign = Header->GetRpt2Callsign();

            // try to open the stream
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

        // delete header if not used
        if ( Header != NULL )
        {
            delete Header;
        }
    }
    else
    {
        // stream already open - tickle it
        stream->Tickle();
        delete Header;
    }

    return newstream;
}

////////////////////////////////////////////////////////////////////////////////////////
// queue helper

void CP25Protocol::HandleQueue(void)
{
    // drain queue into local vector
    std::vector<CPacket *> packets;
    m_Queue.Lock();
    while ( !m_Queue.empty() )
    {
        packets.push_back(m_Queue.front());
        m_Queue.pop();
    }
    m_Queue.Unlock();

    if ( packets.empty() )
        return;

    // build peer lookup maps once per drain cycle (stable across all packets)
    // peerTgByAddr: IP addr → P25 talk group (from Peers)
    // peerPortByAddr: IP addr → send port (from GK peer list)
    std::map<uint32_t, uint32_t> peerTgByAddr;
    {
        CPeers *peers = g_Reflector.GetPeers();
        int peerIdx = -1;
        CPeer *peer = NULL;
        while ( (peer = peers->FindNextPeer(PROTOCOL_P25, &peerIdx)) != NULL )
        {
            // FindNextPeer(PROTOCOL_P25) guarantees this is a CP25Peer
            CP25Peer *p25Peer = static_cast<CP25Peer *>(peer);
            {
                peerTgByAddr[p25Peer->GetIp().GetAddr()] = p25Peer->GetP25Tg();
            }
        }
        g_Reflector.ReleasePeers();
    }
    std::map<uint32_t, uint16_t> peerPortByAddr;
    {
        CPeerCallsignList *list = g_GateKeeper.GetPeerList();
        for ( int li = 0; li < list->size(); li++ )
        {
            CCallsignListItem *item = &((list->data())[li]);
            if ( IsP25PeerCallsign(item->GetCallsign()) && item->GetPort() != 0 )
            {
                item->ResolveIp();
                peerPortByAddr[item->GetIp().GetAddr()] = item->GetPort();
            }
        }
        g_GateKeeper.ReleasePeerList();
    }

    CBuffer buffer;
    buffer.reserve(64);

    for ( size_t pi = 0; pi < packets.size(); pi++ )
    {
        CPacket *packet = packets[pi];

        // get our sender's id
        int iModId = g_Reflector.GetModuleIndex(packet->GetModuleId());
        if ( iModId < 0 || iModId >= NB_OF_MODULES )
        {
            delete packet;
            continue;
        }

        // capture owner under slot lock, snapshot for send loops
        CIp ownerIpCopy;
        bool hasOwner = false;
        {
            std::lock_guard<std::mutex> lock(m_StreamsCache[iModId].m_Mutex);

            if ( packet->IsDvHeader() )
            {
                // capture owner IP from the live stream at header time
                m_StreamsCache[iModId].m_bHasOwner = false;
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

            hasOwner = m_StreamsCache[iModId].m_bHasOwner;
            ownerIpCopy = m_StreamsCache[iModId].m_OwnerIp;
        } // slot lock released

        // encode and send to P25 peers
        buffer.clear();

        if ( packet->IsDvHeader() )
        {
            // update local stream cache under slot lock
            {
                std::lock_guard<std::mutex> lock(m_StreamsCache[iModId].m_Mutex);
                m_StreamsCache[iModId].m_dvHeader = CDvHeaderPacket((const CDvHeaderPacket &)*packet);
                m_StreamsCache[iModId].m_uiSrcId = CallsignToP25Id(m_StreamsCache[iModId].m_dvHeader.GetMyCallsign());
                m_StreamsCache[iModId].m_uiFrameCount = 0;
                m_StreamsCache[iModId].m_uiDstTg = 0;
            }

            // Get destination TG from first connected P25 peer on this module
            // using pre-built peerTgByAddr map (no per-packet Peers lock needed)
            {
                CClients *clients = g_Reflector.GetClients();
                int index = -1;
                CClient *client = NULL;
                while ( (client = clients->FindNextClient(PROTOCOL_P25, &index)) != NULL )
                {
                    if ( client->IsPeer() &&
                         (client->GetReflectorModule() == packet->GetModuleId()) )
                    {
                        auto it = peerTgByAddr.find(client->GetIp().GetAddr());
                        if ( it != peerTgByAddr.end() )
                        {
                            std::lock_guard<std::mutex> lock(m_StreamsCache[iModId].m_Mutex);
                            m_StreamsCache[iModId].m_uiDstTg = it->second;
                        }
                        break;
                    }
                }
                g_Reflector.ReleaseClients();
            }

            // Header doesn't generate a P25 packet - we start sending on first voice frame
        }
        else if ( packet->IsLastPacket() )
        {
            // Reset frame counter under slot lock
            {
                std::lock_guard<std::mutex> lock(m_StreamsCache[iModId].m_Mutex);
                m_StreamsCache[iModId].m_uiFrameCount = 0;
            }

            // Send terminator to all P25 peers (using pre-built peerPortByAddr map)
            CClients *clients = g_Reflector.GetClients();
            int index = -1;
            CClient *client = NULL;
            while ( (client = clients->FindNextClient(PROTOCOL_P25, &index)) != NULL )
            {
                if ( client->IsPeer() &&
                     (client->GetReflectorModule() == packet->GetModuleId()) )
                {
                    // Skip if this stream originated from this client (prevent loop)
                    // Full IP:port match — remote hosts may run multiple modes on different ports
                    if ( hasOwner && ownerIpCopy == client->GetIp() )
                    {
                        continue;
                    }

                    auto pit = peerPortByAddr.find(client->GetIp().GetAddr());
                    if ( pit != peerPortByAddr.end() )
                    {
                        EncodeTerminatorPacket(&buffer);
                        m_Socket.SendVoice(buffer, client->GetIp(), pit->second);
                    }
                }
            }
            g_Reflector.ReleaseClients();
        }
        else
        {
            // Voice frame - send as appropriate LDU record
            CDvFramePacket *dvFrame = (CDvFramePacket *)packet;
            const uint8 *imbeData = dvFrame->GetAmbe(CODEC_IMBE);

            // If no IMBE data available (transcoding not complete), use silence
            if ( imbeData == NULL )
            {
                imbeData = P25_IMBE_SILENCE;
            }

            // Snapshot cache fields under slot lock
            uint32_t srcId;
            unsigned int p25step;
            {
                std::lock_guard<std::mutex> lock(m_StreamsCache[iModId].m_Mutex);
                srcId = m_StreamsCache[iModId].m_uiSrcId;
                p25step = m_StreamsCache[iModId].m_uiFrameCount % 18;
            }

            // Send using pre-built peerTgByAddr and peerPortByAddr maps
            CClients *clients = g_Reflector.GetClients();
            int index = -1;
            CClient *client = NULL;
            while ( (client = clients->FindNextClient(PROTOCOL_P25, &index)) != NULL )
            {
                if ( client->IsPeer() &&
                     (client->GetReflectorModule() == packet->GetModuleId()) )
                {
                    // Skip if this stream originated from this client (prevent loop)
                    // Full IP:port match — remote hosts may run multiple modes on different ports
                    if ( hasOwner && ownerIpCopy == client->GetIp() )
                    {
                        continue;
                    }

                    uint32_t addr = client->GetIp().GetAddr();
                    auto pit = peerPortByAddr.find(addr);
                    if ( pit != peerPortByAddr.end() )
                    {
                        uint32_t dstTg = 0;
                        auto it = peerTgByAddr.find(addr);
                        if ( it != peerTgByAddr.end() )
                        {
                            dstTg = it->second;
                        }

                        EncodeVoicePacket(&buffer, p25step, imbeData, srcId, dstTg);
                        m_Socket.SendVoice(buffer, client->GetIp(), pit->second);
                    }
                }
            }
            g_Reflector.ReleaseClients();

            {
                std::lock_guard<std::mutex> lock(m_StreamsCache[iModId].m_Mutex);
                m_StreamsCache[iModId].m_uiFrameCount++;
            }
        }

        // done
        delete packet;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// peer link helpers

void CP25Protocol::HandlePeerLinks(void)
{
    CBuffer buffer;

    // snapshot GK peer list into local structures, then release GK lock
    // before acquiring Peers lock — prevents GK+Peers lock-order inversion
    struct P25PeerInfo {
        CCallsign callsign;
        CIp       ip;
        uint16    port;
        char      modules[2];
    };
    std::vector<P25PeerInfo> gkPeers;
    {
        CPeerCallsignList *list = g_GateKeeper.GetPeerList();
        for ( int i = 0; i < list->size(); i++ )
        {
            CCallsignListItem *item = &((list->data())[i]);
            if ( IsP25PeerCallsign(item->GetCallsign()) )
            {
                P25PeerInfo info;
                info.callsign = item->GetCallsign();
                item->ResolveIp();
                info.ip = item->GetIp();
                info.port = item->GetPort();
                ::strncpy(info.modules, item->GetModules(), 1);
                info.modules[1] = '\0';
                gkPeers.push_back(info);
            }
        }
        g_GateKeeper.ReleasePeerList();
    }

    // now work with Peers lock only
    CPeers *peers = g_Reflector.GetPeers();

    // collect peers to remove (RemovePeer erases from vector, invalidating iterator)
    std::vector<CPeer *> toRemove;
    int index = -1;
    CPeer *peer = NULL;
    while ( (peer = peers->FindNextPeer(PROTOCOL_P25, &index)) != NULL )
    {
        bool found = false;
        for ( size_t i = 0; i < gkPeers.size(); i++ )
        {
            if ( peer->GetCallsign().HasSameCallsign(gkPeers[i].callsign) )
            {
                found = true;
                break;
            }
        }
        if ( !found )
        {
            toRemove.push_back(peer);
        }
    }
    for ( size_t i = 0; i < toRemove.size(); i++ )
    {
        std::cout << "P25 peer " << toRemove[i]->GetCallsign() << " removed - no longer in peer list" << std::endl;
        peers->RemovePeer(toRemove[i]);
    }

    // check if all P25 peers listed by gatekeeper are connected
    for ( size_t i = 0; i < gkPeers.size(); i++ )
    {
        if ( peers->FindPeer(gkPeers[i].callsign, PROTOCOL_P25) == NULL )
        {
            if ( gkPeers[i].port == 0 )
            {
                std::cout << "P25 peer " << gkPeers[i].callsign << " has no port configured - skipping" << std::endl;
                continue;
            }

            // send poll packet to initiate connection
            std::cout << "P25 connecting to peer " << gkPeers[i].callsign
                      << " @ " << gkPeers[i].ip << ":" << gkPeers[i].port
                      << " on module " << gkPeers[i].modules << std::endl;
            EncodePollPacket(&buffer, g_Reflector.GetCallsign());
            m_Socket.Send(buffer, gkPeers[i].ip, gkPeers[i].port);
        }
    }

    // done
    g_Reflector.ReleasePeers();
}

void CP25Protocol::HandlePeerKeepalives(void)
{
    CBuffer buffer;

    // snapshot callsign→port from GK list, release GK before acquiring Peers
    std::map<uint32_t, uint16_t> peerPortByAddr;
    {
        CPeerCallsignList *list = g_GateKeeper.GetPeerList();
        for ( int i = 0; i < list->size(); i++ )
        {
            CCallsignListItem *item = &((list->data())[i]);
            if ( IsP25PeerCallsign(item->GetCallsign()) && item->GetPort() != 0 )
            {
                item->ResolveIp();
                peerPortByAddr[item->GetIp().GetAddr()] = item->GetPort();
            }
        }
        g_GateKeeper.ReleasePeerList();
    }

    // now work with Peers lock only
    CPeers *peers = g_Reflector.GetPeers();

    // send keepalive to all connected P25 peers, collect timed-out ones for removal
    std::vector<CPeer *> toRemove;
    int index = -1;
    CPeer *peer = NULL;
    while ( (peer = peers->FindNextPeer(PROTOCOL_P25, &index)) != NULL )
    {
        CP25Peer *p25Peer = dynamic_cast<CP25Peer *>(peer);
        if ( p25Peer == NULL )
        {
            continue;
        }

        if ( !p25Peer->IsAlive() )
        {
            toRemove.push_back(peer);
        }
        else
        {
            // find the port for this peer from pre-built map
            auto it = peerPortByAddr.find(peer->GetIp().GetAddr());
            if ( it != peerPortByAddr.end() )
            {
                // send keepalive poll
                EncodePollPacket(&buffer, g_Reflector.GetCallsign());
                m_Socket.Send(buffer, peer->GetIp(), it->second);
            }
        }
    }
    for ( size_t i = 0; i < toRemove.size(); i++ )
    {
        std::cout << "P25 peer " << toRemove[i]->GetCallsign() << " keepalive timeout" << std::endl;
        peers->RemovePeer(toRemove[i]);
    }

    // done
    g_Reflector.ReleasePeers();
}

////////////////////////////////////////////////////////////////////////////////////////
// packet decoding helpers

bool CP25Protocol::IsValidPollPacket(const CBuffer &Buffer, CCallsign *callsign)
{
    bool valid = false;

    if ( Buffer.size() == P25_POLL_PACKET_SIZE )
    {
        if ( Buffer.data()[0] == P25_CMD_POLL || Buffer.data()[0] == P25_CMD_UNLINK )
        {
            // extract callsign (10 bytes after command byte)
            callsign->SetCallsign(Buffer.data() + 1, P25_CALLSIGN_LENGTH);
            valid = true;
        }
    }

    return valid;
}

bool CP25Protocol::ExtractImbeFromRecord(const CBuffer &Buffer, uint8 *imbe)
{
    if ( Buffer.size() < 1 )
        return false;

    uint8 cmd = Buffer.data()[0];
    int offset = -1;
    int minSize = 0;

    // Determine IMBE offset based on record type
    switch ( cmd )
    {
        case P25_REC_LDU1_VOICE1:  // 0x62 - 22 bytes, IMBE at offset 10
        case P25_REC_LDU2_VOICE1:  // 0x6B
            offset = 10;
            minSize = 21;
            break;
        case P25_REC_LDU1_VOICE2:  // 0x63 - 14 bytes, IMBE at offset 1
        case P25_REC_LDU2_VOICE2:  // 0x6C
            offset = 1;
            minSize = 12;
            break;
        case P25_REC_LDU1_VOICE3:  // 0x64 - 17 bytes, IMBE at offset 5
        case P25_REC_LDU1_VOICE4:  // 0x65
        case P25_REC_LDU1_VOICE5:  // 0x66
        case P25_REC_LDU1_VOICE6:  // 0x67
        case P25_REC_LDU1_VOICE7:  // 0x68
        case P25_REC_LDU1_VOICE8:  // 0x69
        case P25_REC_LDU2_VOICE3:  // 0x6D
        case P25_REC_LDU2_VOICE4:  // 0x6E
        case P25_REC_LDU2_VOICE5:  // 0x6F
        case P25_REC_LDU2_VOICE6:  // 0x70
        case P25_REC_LDU2_VOICE7:  // 0x71
        case P25_REC_LDU2_VOICE8:  // 0x72
            offset = 5;
            minSize = 16;
            break;
        case P25_REC_LDU1_VOICE9:  // 0x6A - 16 bytes, IMBE at offset 4
        case P25_REC_LDU2_VOICE9:  // 0x73
            offset = 4;
            minSize = 15;
            break;
        default:
            return false;
    }

    if ( Buffer.size() < (unsigned int)minSize )
        return false;

    ::memcpy(imbe, Buffer.data() + offset, P25_IMBE_SIZE);
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
// packet encoding helpers

void CP25Protocol::EncodePollPacket(CBuffer *buffer, const CCallsign &callsign) const
{
    buffer->clear();

    // command byte
    buffer->Append((uint8)P25_CMD_POLL);

    // callsign (10 bytes, space padded)
    char cs[P25_CALLSIGN_LENGTH];
    ::memset(cs, ' ', sizeof(cs));
    callsign.GetCallsign((uint8 *)cs);
    buffer->Append((uint8 *)cs, P25_CALLSIGN_LENGTH);
}

void CP25Protocol::EncodeVoicePacket(CBuffer *buffer, unsigned int step, const uint8 *imbe,
                                      uint32_t srcId, uint32_t dstId) const
{
    buffer->clear();

    unsigned char rec[32];

    switch ( step )
    {
        case 0x00U:  // LDU1 voice 1
            ::memcpy(rec, P25_REC62, 22);
            ::memcpy(rec + 10, imbe, P25_IMBE_SIZE);
            buffer->Append(rec, 22);
            break;
        case 0x01U:
            ::memcpy(rec, P25_REC63, 14);
            ::memcpy(rec + 1, imbe, P25_IMBE_SIZE);
            buffer->Append(rec, 14);
            break;
        case 0x02U:
            ::memcpy(rec, P25_REC64, 17);
            ::memcpy(rec + 5, imbe, P25_IMBE_SIZE);
            rec[1] = 0x00U;
            buffer->Append(rec, 17);
            break;
        case 0x03U:  // Contains destination TG
            ::memcpy(rec, P25_REC65, 17);
            ::memcpy(rec + 5, imbe, P25_IMBE_SIZE);
            rec[1] = (dstId >> 16) & 0xFFU;
            rec[2] = (dstId >> 8) & 0xFFU;
            rec[3] = (dstId >> 0) & 0xFFU;
            buffer->Append(rec, 17);
            break;
        case 0x04U:  // Contains source ID
            ::memcpy(rec, P25_REC66, 17);
            ::memcpy(rec + 5, imbe, P25_IMBE_SIZE);
            rec[1] = (srcId >> 16) & 0xFFU;
            rec[2] = (srcId >> 8) & 0xFFU;
            rec[3] = (srcId >> 0) & 0xFFU;
            buffer->Append(rec, 17);
            break;
        case 0x05U:
            ::memcpy(rec, P25_REC67, 17);
            ::memcpy(rec + 5, imbe, P25_IMBE_SIZE);
            buffer->Append(rec, 17);
            break;
        case 0x06U:
            ::memcpy(rec, P25_REC68, 17);
            ::memcpy(rec + 5, imbe, P25_IMBE_SIZE);
            buffer->Append(rec, 17);
            break;
        case 0x07U:
            ::memcpy(rec, P25_REC69, 17);
            ::memcpy(rec + 5, imbe, P25_IMBE_SIZE);
            buffer->Append(rec, 17);
            break;
        case 0x08U:
            ::memcpy(rec, P25_REC6A, 16);
            ::memcpy(rec + 4, imbe, P25_IMBE_SIZE);
            buffer->Append(rec, 16);
            break;
        case 0x09U:  // LDU2 voice 1
            ::memcpy(rec, P25_REC6B, 22);
            ::memcpy(rec + 10, imbe, P25_IMBE_SIZE);
            buffer->Append(rec, 22);
            break;
        case 0x0AU:
            ::memcpy(rec, P25_REC6C, 14);
            ::memcpy(rec + 1, imbe, P25_IMBE_SIZE);
            buffer->Append(rec, 14);
            break;
        case 0x0BU:
            ::memcpy(rec, P25_REC6D, 17);
            ::memcpy(rec + 5, imbe, P25_IMBE_SIZE);
            buffer->Append(rec, 17);
            break;
        case 0x0CU:
            ::memcpy(rec, P25_REC6E, 17);
            ::memcpy(rec + 5, imbe, P25_IMBE_SIZE);
            buffer->Append(rec, 17);
            break;
        case 0x0DU:
            ::memcpy(rec, P25_REC6F, 17);
            ::memcpy(rec + 5, imbe, P25_IMBE_SIZE);
            buffer->Append(rec, 17);
            break;
        case 0x0EU:
            ::memcpy(rec, P25_REC70, 17);
            ::memcpy(rec + 5, imbe, P25_IMBE_SIZE);
            rec[1] = 0x80U;
            buffer->Append(rec, 17);
            break;
        case 0x0FU:
            ::memcpy(rec, P25_REC71, 17);
            ::memcpy(rec + 5, imbe, P25_IMBE_SIZE);
            buffer->Append(rec, 17);
            break;
        case 0x10U:
            ::memcpy(rec, P25_REC72, 17);
            ::memcpy(rec + 5, imbe, P25_IMBE_SIZE);
            buffer->Append(rec, 17);
            break;
        case 0x11U:
            ::memcpy(rec, P25_REC73, 16);
            ::memcpy(rec + 4, imbe, P25_IMBE_SIZE);
            buffer->Append(rec, 16);
            break;
    }
}

void CP25Protocol::EncodeTerminatorPacket(CBuffer *buffer) const
{
    buffer->clear();
    buffer->Append((uint8 *)P25_REC80, 17);
}

////////////////////////////////////////////////////////////////////////////////////////
// peer helpers

bool CP25Protocol::IsP25PeerCallsign(const CCallsign &callsign) const
{
    char cs[CALLSIGN_LEN + 1];
    callsign.GetCallsign((uint8 *)cs);
    cs[CALLSIGN_LEN] = '\0';
    // P25 peers use "P25nnnnn" format
    return (::strncmp(cs, "P25", 3) == 0) && (cs[3] >= '0') && (cs[3] <= '9');
}

CCallsignListItem *CP25Protocol::FindP25PeerByIp(CPeerCallsignList *list, const CIp &ip)
{
    // Search for a P25 peer matching this IP. Port-aware disambiguation —
    // see FindYsfPeerByIp in cysfprotocol.cpp for the full rationale.
    // Briefly: if the interlink entry declares an explicit port, match
    // (address, port); otherwise fall back to address-only for backward
    // compatibility with single-peer-per-IP configurations.
    for ( int i = 0; i < list->size(); i++ )
    {
        CCallsignListItem *item = &((list->data())[i]);

        // Check if this is a P25 peer (callsign starts with "P25" + digit)
        if ( IsP25PeerCallsign(item->GetCallsign()) )
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

////////////////////////////////////////////////////////////////////////////////////////
// P25 ID helpers

uint32_t CP25Protocol::CallsignToP25Id(const CCallsign &callsign) const
{
    // P25 subscriber IDs are the same as DMR IDs
    // Look up the DMR ID for this callsign
    uint32_t dmrid = 0;

    g_DmridDir.Lock();
    {
        dmrid = g_DmridDir.FindDmrid(callsign);
    }
    g_DmridDir.Unlock();

    // If DMR ID found and valid (within P25 24-bit range), use it
    if (dmrid > 0 && dmrid <= P25_ID_MAX)
    {
        return dmrid;
    }

    // Fallback: use a hash of the callsign for unknown stations
    char cs[CALLSIGN_LEN + 1];
    callsign.GetCallsign((uint8 *)cs);
    cs[CALLSIGN_LEN] = '\0';

    uint32_t hash = 0;
    for (int i = 0; cs[i] != '\0' && cs[i] != ' '; i++)
    {
        hash = hash * 31 + cs[i];
    }
    return (hash & 0x00FFFFFF) | 0x00000001;  // Ensure non-zero and 24-bit
}

bool CP25Protocol::P25IdToCallsign(uint32_t p25Id, CCallsign *callsign) const
{
    // P25 subscriber IDs are the same as DMR IDs
    // Look up the callsign from the DMR ID directory
    if (p25Id == 0 || p25Id > P25_ID_MAX)
    {
        return false;
    }

    g_DmridDir.Lock();
    {
        const CCallsign *foundCs = g_DmridDir.FindCallsign(p25Id);
        if (foundCs != NULL)
        {
            *callsign = *foundCs;
            g_DmridDir.Unlock();
            return true;
        }
    }
    g_DmridDir.Unlock();

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////
// uiStreamId helpers

uint32 CP25Protocol::IpToStreamId(const CIp &ip) const
{
    // XOR port into both halves to distinguish localhost peers on different ports
    return ip.GetAddr() ^ (uint32)(MAKEDWORD(ip.GetPort(), ip.GetPort()));
}

// Build the map key. (addr<<16)|port keeps the same source endpoint
// keyed identically for header / voice / TDU frames within one
// transmission, and distinguishes localhost peers using different
// ephemeral ports.
static inline uint64_t P25SourceKey(const CIp &ip)
{
    return ((uint64_t)ip.GetAddr() << 16) | (uint64_t)(ip.GetPort() & 0xFFFFu);
}

// Allocate a fresh reflector-side stream-id for a NEW transmission
// (LDU1 Voice1). Uses the process-wide CProtocol::AllocateGlobalStreamIdNonce
// — see cprotocol.cpp for the rationale (random seed at startup,
// shared across protocols to avoid inter-protocol sid collisions).
// The per-protocol collision-against-m_SourceStates check is kept as
// defence-in-depth for the rare wrap-around case.
uint32 CP25Protocol::AllocateNewStreamIdForSource(const CIp &ip)
{
    std::lock_guard<std::mutex> lock(m_SourceStatesMutex);

    const uint64_t key = P25SourceKey(ip);

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

    // Pathological fallback (would need 65535 simultaneous active sources).
    uint32 fallback = IpToStreamId(ip);
    m_SourceStates[key] = fallback;
    return fallback;
}

// Look up the active sid for a non-Voice1 LDU frame or a TDU. Falls
// back to the deterministic IpToStreamId() when no header has been
// seen for this source — preserves the late-entry buffering path in
// CProtocol::OnDvFramePacketIn for orphan mid-stream packets.
uint32 CP25Protocol::LookupStreamIdForSource(const CIp &ip) const
{
    std::lock_guard<std::mutex> lock(m_SourceStatesMutex);
    auto it = m_SourceStates.find(P25SourceKey(ip));
    if (it != m_SourceStates.end())
    {
        return it->second;
    }
    return IpToStreamId(ip);
}

// Test whether this source has an active stream-id mapping (i.e. a
// transmission is currently in progress and its sid hasn't been
// released by a TDU yet). Used by the LDU1_VOICE1 dispatch above to
// distinguish a true start-of-transmission (no active mapping → fresh
// allocate) from a mid-transmission LDU1 cycle (active mapping →
// reuse existing sid). See the dispatch comment in OnLduPacketIn for
// the full rationale.
bool CP25Protocol::HasActiveStreamIdForSource(const CIp &ip) const
{
    std::lock_guard<std::mutex> lock(m_SourceStatesMutex);
    return m_SourceStates.find(P25SourceKey(ip)) != m_SourceStates.end();
}

// Drop the IP→sid mapping after the TDU is processed. The next LDU1
// Voice1 from this source will allocate a fresh sid.
void CP25Protocol::ReleaseStreamIdForSource(const CIp &ip)
{
    std::lock_guard<std::mutex> lock(m_SourceStatesMutex);
    m_SourceStates.erase(P25SourceKey(ip));
}
