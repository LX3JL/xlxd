//
//  cnxdnprotocol.cpp
//  xlxd
//
//  Created for NXDN Reflector peering support
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
#include "cnxdnprotocol.h"
#include "cnxdnutils.h"
#include "cysfutils.h"
#include "creflector.h"
#include "cgatekeeper.h"
#include "cnxdnpeer.h"
#include "cnxdnpeerclient.h"
#include "cpeercallsignlist.h"
#include "cnxdniddirfile.h"
#include "cnxdniddirhttp.h"

////////////////////////////////////////////////////////////////////////////////////////
// constructor

CNxdnProtocol::CNxdnProtocol()
{
}

////////////////////////////////////////////////////////////////////////////////////////
// operation

bool CNxdnProtocol::Init(void)
{
    bool ok;

    // base class
    ok = CProtocol::Init();

    // update the reflector callsign
    m_ReflectorCallsign.PatchCallsign(0, (const uint8 *)"NXDN", 4);

    // create our socket with ephemeral port (peer-only, outbound connections)
    ok &= m_Socket.Open(0);

    // update time
    m_LastPeerLinkTime.Now();
    m_LastPeerKeepaliveTime.Now();

    // done
    return ok;
}

void CNxdnProtocol::Close(void)
{
    // base class
    CProtocol::Close();
}

////////////////////////////////////////////////////////////////////////////////////////
// task

void CNxdnProtocol::RxTask(void)
{
    CBuffer     Buffer;
    CIp         Ip;
    CCallsign   Callsign;
    uint16_t    uiTg;
    uint16_t    uiSrcId;
    uint16_t    uiDstId;
    uint8_t     uiFlags;
    uint8_t     uiPayload[NXDN_VOICE_PAYLOAD_SIZE];

    // handle incoming packets
    if ( m_Socket.Receive(&Buffer, &Ip, 20) != -1 )
    {
        // crack the packet
        if ( IsValidDataPacket(Buffer, &uiSrcId, &uiDstId, &uiFlags, uiPayload) )
        {
            // only handle group voice calls
            if ( (uiFlags & NXDN_FLAG_GROUP) && !(uiFlags & NXDN_FLAG_DATA) )
            {
                // Deferred call for OnDvLastFramePacketIn (must be called without peers lock
                // because it calls HandleQueue which tries to acquire peers lock again)
                CDvLastFramePacket *deferredLastFrame = NULL;
                // Deferred call for OnDvHeaderPacketIn so the per-callsign
                // loop-block check (IsCallsignLoopBlocked → LoopMutex)
                // happens AFTER we release Peers. Avoids introducing a
                // novel Peers → LoopMutex ordering that no other code
                // path uses. The deferred values are captured under the
                // Peers lock and consumed below the ReleasePeers call.
                CDvHeaderPacket *deferredHeader = NULL;
                uint16_t deferredSrcId = 0;
                uint16_t deferredDstId = 0;

                // find the peer
                CPeers *peers = g_Reflector.GetPeers();
                CPeer *peer = peers->FindPeerByIpPort(Ip, PROTOCOL_NXDN);

                if ( peer != NULL )
                {
                    // Lock order: Peers → m_SourceStatesMutex. The Allocate /
                    // Lookup / Release helpers below take m_SourceStatesMutex
                    // internally while we still hold the Peers lock acquired
                    // at line 103. Do NOT add code that takes a reflector
                    // lock (Peers, Clients, etc.) inside any helper that
                    // also takes m_SourceStatesMutex — that would invert
                    // this order and risk deadlock.
                    //
                    // get stream id — header allocates fresh, voice and
                    // trailer look up the active mapping. See the helper
                    // comments in cnxdnprotocol.h for why deterministic
                    // IpToStreamId() collides on rapid re-keys.
                    uint32 uiStreamId;
                    if ( uiFlags & NXDN_FLAG_HEADER )
                    {
                        uiStreamId = AllocateNewStreamIdForSource(Ip);
                    }
                    else
                    {
                        uiStreamId = LookupStreamIdForSource(Ip);
                    }

                    if ( uiFlags & NXDN_FLAG_HEADER )
                    {
                        // this is a header frame - start new stream
                        // Look up callsign from NXDN ID database
                        CCallsign csMY;
                        g_NxdnIdDir.Lock();
                        {
                            const CCallsign *foundCs = g_NxdnIdDir.FindCallsign(uiSrcId);
                            if ( foundCs != NULL )
                            {
                                csMY = *foundCs;
                            }
                            else
                            {
                                // Fallback: use NXDN ID as callsign (NXnnnnn format)
                                char szCallsign[16];
                                ::snprintf(szCallsign, sizeof(szCallsign), "N%05u", uiSrcId);
                                csMY.SetCallsign(szCallsign);
                            }
                        }
                        g_NxdnIdDir.Unlock();
                        // get the module this peer is linked to
                        char module = ' ';
                        if ( peer->GetNbClients() > 0 )
                        {
                            module = peer->GetClient(0)->GetReflectorModule();
                        }

                        // RPT1 uses the caller's own callsign with 'B' — matches
                        // the D-Star hotspot convention (DVAP/Pi-Star/MMDVM/openSPOT
                        // all stamp RPT1 as "<user> B"). This makes cross-mode
                        // traffic indistinguishable from a legitimate D-Star hotspot
                        // transmission, so strict receivers (Icom G3 gateways) accept
                        // it rather than rejecting on an unknown-repeater lookup.
                        CCallsign rpt1 = csMY;
                        rpt1.SetModule('B');
                        CCallsign rpt2 = m_ReflectorCallsign;
                        rpt2.SetModule('G');

                        // Reset YSF subpacket ID counter for new stream
                        int iModId = g_Reflector.GetModuleIndex(module);
                        if ( iModId >= 0 && iModId < NB_OF_MODULES )
                        {
                            std::lock_guard<std::mutex> lock(m_StreamsCache[iModId].m_Mutex);
                            m_StreamsCache[iModId].m_uiYsfSubId = 0;
                        }

                        // create header packet
                        CDvHeaderPacket *header = new CDvHeaderPacket(csMY, CCallsign("CQCQCQ"), rpt1, rpt2, uiStreamId, 0);

                        // Defer OnDvHeaderPacketIn — the per-callsign loop
                        // block check happens after Peers is released.
                        // See the deferredHeader declaration above.
                        deferredHeader = header;
                        deferredSrcId = uiSrcId;
                        deferredDstId = uiDstId;
                    }
                    else if ( uiFlags & NXDN_FLAG_TRAILER )
                    {
                        // end of transmission
                        // find the stream
                        CPacketStream *stream = GetStream(uiStreamId);
                        if ( stream != NULL )
                        {
                            // create last frame packet with empty AMBE data
                            // Defer the OnDvLastFramePacketIn call until after peers lock is released
                            uint8 uiAmbe[AMBE_SIZE];
                            ::memset(uiAmbe, 0x00, sizeof(uiAmbe));
                            deferredLastFrame = new CDvLastFramePacket(uiAmbe, uiStreamId, (uint8)0, (uint8)0, (uint8)0);
                        }
                        // Drop the IP→sid mapping unconditionally — the next
                        // header from this source allocates fresh. NXDN radios
                        // commonly emit the trailer flag on multiple frames at
                        // end-of-transmission for redundancy; first call here
                        // releases, subsequent trailers fall back to the
                        // deterministic IpToStreamId() which won't match any
                        // open stream and so are dropped harmlessly in
                        // CProtocol::OnDvLastFramePacketIn.
                        ReleaseStreamIdForSource(Ip);
                    }
                    else
                    {
                        // voice frame - extract 4 AMBE frames from NXDN payload
                        uint8 ambe0[9], ambe1[9], ambe2[9], ambe3[9];
                        uint8 *ambes[4] = { ambe0, ambe1, ambe2, ambe3 };
                        CNxdnUtils::DecodeVoice(uiPayload, ambes);

                        // Adjust gain for NXDN→DMR direction
                        for (int j = 0; j < 4; j++) {
                            CYsfUtils::AdjustAmbeGain(ambes[j], GAIN_NXDN_TO_DMR / 10.0f);
                        }

                        // Get the module to find the frame counter
                        int iModId = -1;
                        if ( peer->GetNbClients() > 0 )
                        {
                            char module = peer->GetClient(0)->GetReflectorModule();
                            iModId = g_Reflector.GetModuleIndex(module);
                        }

                        // Send all 4 frames with properly cycling YSF IDs
                        // YSF needs: PacketId cycles 0-7, SubId cycles 0-4, FrameId for net frame counter
                        for (int i = 0; i < 4; i++)
                        {
                            uint8 ysfSubId = 0;
                            uint8 ysfPacketId = 0;
                            uint8 ysfFrameId = 0;
                            if ( iModId >= 0 && iModId < NB_OF_MODULES )
                            {
                                std::lock_guard<std::mutex> lock(m_StreamsCache[iModId].m_Mutex);
                                uint16 currentPid = m_StreamsCache[iModId].m_uiYsfSubId;
                                ysfSubId = currentPid % 5;
                                ysfPacketId = (currentPid / 5) % 8;
                                ysfFrameId = (((currentPid / 5) + 1) & 0x7FU) << 1;
                                m_StreamsCache[iModId].m_uiYsfSubId++;
                            }
                            CDvFramePacket *frame = new CDvFramePacket(ambes[i], uiStreamId, ysfPacketId, ysfSubId, ysfFrameId);
                            OnDvFramePacketIn(frame, &Ip);
                        }
                    }
                }
                g_Reflector.ReleasePeers();

                // Process the deferred header outside Peers. Peer traffic
                // bypasses MayTransmit by design, but is still subject
                // to the per-callsign loop block. NXDN has no equivalent
                // of the isPeer || MayTransmit check in YSF / DCS / etc.
                // because this entire branch only runs when peer != NULL
                // — so EVERY incoming NXDN header is peer-sourced and
                // needs the loop-block consultation. See cysfprotocol.cpp
                // for the rationale.
                if ( deferredHeader != NULL )
                {
                    if ( g_GateKeeper.IsCallsignLoopBlocked(
                            deferredHeader->GetMyCallsign(), "NXDN peer") )
                    {
                        delete deferredHeader;
                        deferredHeader = NULL;
                    }
                    else
                    {
                        OnDvHeaderPacketIn(deferredHeader, Ip,
                                           deferredSrcId, deferredDstId);
                    }
                }

                // Now call OnDvLastFramePacketIn after peers lock is released
                // This is necessary because OnDvLastFramePacketIn calls HandleQueue
                // which tries to acquire peers lock again (mutex is not recursive)
                if ( deferredLastFrame != NULL )
                {
                    OnDvLastFramePacketIn(deferredLastFrame, &Ip);
                }
            }
        }
        else if ( IsValidPollPacket(Buffer, &Callsign, &uiTg) )
        {
            // This is a poll response from an NXDN reflector we connected to
            // Check if peer exists — release Peers before any GK access
            bool peerExists = false;
            {
                CPeers *peers = g_Reflector.GetPeers();
                CPeer *existingPeer = peers->FindPeerByIpPort(Ip, PROTOCOL_NXDN);
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
                while ( (client = clients->FindNextClient(PROTOCOL_NXDN, &index)) != NULL )
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
                // Check if this is from an NXDN peer in our peer list
                // Snapshot GK data first, release GK, then acquire Peers+Clients
                CCallsign peerCallsign;
                char peerModule = ' ';
                uint16_t nxdnId = 0;
                bool foundInGk = false;
                {
                    CPeerCallsignList *list = g_GateKeeper.GetPeerList();
                    CCallsignListItem *item = FindNxdnPeerByIp(list, Ip);
                    if ( item != NULL )
                    {
                        peerCallsign = item->GetCallsign();
                        peerModule = item->GetModules()[0];
                        char csStr[CALLSIGN_LEN + 1];
                        item->GetCallsign().GetCallsign((uint8 *)csStr);
                        csStr[CALLSIGN_LEN] = '\0';
                        nxdnId = (uint16_t)::atoi(csStr + 2);
                        foundInGk = true;
                    }
                    g_GateKeeper.ReleasePeerList();
                }

                if ( foundInGk )
                {
                    // Create the NXDN peer object with its client attached
                    // (AddPeer registers the client; RemovePeer cleans it up)
                    CVersion version(1, 0, 0);
                    char modules[2] = { peerModule, '\0' };

                    CNxdnPeer *newPeer = new CNxdnPeer(peerCallsign, Ip, modules, version);
                    newPeer->Alive();
                    newPeer->SetNxdnId(nxdnId);

                    std::cout << "NXDN peer " << peerCallsign << " connected on module " << peerModule << std::endl;

                    // AddPeer registers peer and its clients in the reflector
                    CPeers *peers = g_Reflector.GetPeers();
                    peers->AddPeer(newPeer);
                    g_Reflector.ReleasePeers();
                }
            }
        }
    }

    // handle end of streaming timeout
    CheckStreamsTimeout();

}

////////////////////////////////////////////////////////////////////////////////////////
// TX task — runs on separate thread, never blocked by CloseStream

void CNxdnProtocol::TxTask(void)
{
    // wait for queue notification or 20ms timeout
    {
        std::unique_lock<std::mutex> lk(m_QueueCondMutex);
        m_QueueCondVar.wait_for(lk, std::chrono::milliseconds(20));
    }

    HandleQueue();

    // handle peer connections
    if ( m_LastPeerLinkTime.DurationSinceNow() > NXDN_PEER_RECONNECT_PERIOD )
    {
        HandlePeerLinks();
        m_LastPeerLinkTime.Now();
    }

    // handle peer keepalives
    if ( m_LastPeerKeepaliveTime.DurationSinceNow() > NXDN_PEER_KEEPALIVE_PERIOD )
    {
        HandlePeerKeepalives();
        m_LastPeerKeepaliveTime.Now();
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// streams helpers

bool CNxdnProtocol::OnDvHeaderPacketIn(CDvHeaderPacket *Header, const CIp &Ip, uint16_t srcId, uint16_t dstId)
{
    bool newstream = false;

    // set default suffix if not already set
    if ( !Header->HasMySuffix() )
    {
        Header->SetMySuffix("NXDN");
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
        CClient *client = NULL;

        // Look for matching NXDN peer client
        int index = -1;
        CClient *c = NULL;
        while ( (c = clients->FindNextClient(PROTOCOL_NXDN, &index)) != NULL )
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

            // save header data before OpenStream — OpenStream transfers
            // Header into the router queue, where the router thread may delete it
            myCallsign = Header->GetMyCallsign();
            rpt2Callsign = Header->GetRpt2Callsign();
            CDvHeaderPacket headerCopy(*Header);
            int iModId = g_Reflector.GetModuleIndex(Header->GetRpt2Module());

            // try to open the stream
            if ( (stream = g_Reflector.OpenStream(Header, client)) != NULL )
            {
                // keep the handle — Header ownership transferred to stream
                m_Streams.push_back(stream);
                newstream = true;
                Header = NULL;

                // cache stream info under slot lock (TX thread reads these)
                if ( iModId >= 0 && iModId < NB_OF_MODULES )
                {
                    std::lock_guard<std::mutex> lock(m_StreamsCache[iModId].m_Mutex);
                    m_StreamsCache[iModId].m_dvHeader = headerCopy;
                    m_StreamsCache[iModId].m_uiSrcId = srcId;
                    m_StreamsCache[iModId].m_uiDstId = dstId;
                }
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

void CNxdnProtocol::HandleQueue(void)
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

    // build peer ID lookup map once per drain cycle (stable across all packets)
    std::map<uint32_t, uint16_t> peerIdByAddr;
    {
        CPeers *peers = g_Reflector.GetPeers();
        int peerIdx = -1;
        CPeer *peer = NULL;
        while ( (peer = peers->FindNextPeer(PROTOCOL_NXDN, &peerIdx)) != NULL )
        {
            // FindNextPeer(PROTOCOL_NXDN) guarantees this is a CNxdnPeer
            CNxdnPeer *nxdnPeer = static_cast<CNxdnPeer *>(peer);
            {
                peerIdByAddr[nxdnPeer->GetIp().GetAddr()] = nxdnPeer->GetNxdnId();
            }
        }
        g_Reflector.ReleasePeers();
    }

    CBuffer buffer;
    buffer.reserve(64);

    for ( size_t pi = 0; pi < packets.size(); pi++ )
    {
        CPacket *packet = packets[pi];

        // get our sender's id
        int iModId = g_Reflector.GetModuleIndex(packet->GetModuleId());

        // bounds check
        if ( iModId < 0 || iModId >= NB_OF_MODULES )
        {
            delete packet;
            continue;
        }

        // capture owner and shared fields under slot lock, snapshot for send loops
        CIp ownerIpCopy;
        bool hasOwner = false;
        uint16_t srcIdCopy = 0;
        uint16_t dstIdCopy = 0;
        {
            std::lock_guard<std::mutex> lock(m_StreamsCache[iModId].m_Mutex);

            if ( packet->IsDvHeader() )
            {
                // capture owner IP at header time and store in cache
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
            srcIdCopy = m_StreamsCache[iModId].m_uiSrcId;
            dstIdCopy = m_StreamsCache[iModId].m_uiDstId;
        } // slot lock released

        // encode and send to NXDN peers
        buffer.clear();

        if ( packet->IsDvHeader() )
        {
            // resolve NXDN ID outside the slot lock (CallsignToNxdnId acquires NxdnIdDir lock)
            CDvHeaderPacket headerCopy((const CDvHeaderPacket &)*packet);
            uint16_t headerSrcId = CallsignToNxdnId(headerCopy.GetMyCallsign());

            // update local stream cache under slot lock
            {
                std::lock_guard<std::mutex> lock(m_StreamsCache[iModId].m_Mutex);
                m_StreamsCache[iModId].m_dvHeader = headerCopy;
                m_StreamsCache[iModId].m_uiSrcId = headerSrcId;
                m_StreamsCache[iModId].m_uiFrameCount = 0;
                m_StreamsCache[iModId].m_uiPacketCount = 0;
                m_StreamsCache[iModId].m_uiDstId = 0;
            } // slot lock released before peer/client iteration

            // Create header data packet with proper VCALL FACCH1 payload
            uint8_t payload[NXDN_VOICE_PAYLOAD_SIZE];

            // Send header to peers using pre-built peerIdByAddr map
            CClients *clients = g_Reflector.GetClients();
            int index = -1;
            CClient *client = NULL;
            uint16_t firstDstId = 0;
            bool firstDstStored = false;
            while ( (client = clients->FindNextClient(PROTOCOL_NXDN, &index)) != NULL )
            {
                // Note: For NXDN we send to peers even if they're the master,
                // because NXDNReflector handles source filtering internally
                if ( client->IsPeer() &&
                     (client->GetReflectorModule() == packet->GetModuleId()) )
                {
                    // Skip if this stream originated from this client (prevent loop)
                    // Full IP:port match — remote hosts may run multiple modes on different ports
                    if ( hasOwner && ownerIpCopy == client->GetIp() )
                    {
                        continue;
                    }

                    auto it = peerIdByAddr.find(client->GetIp().GetAddr());
                    if ( it != peerIdByAddr.end() )
                    {
                        uint16_t dstId = it->second;

                        // capture first dstId for SACCH encoding (written to cache after loop)
                        if ( !firstDstStored )
                        {
                            firstDstId = dstId;
                            firstDstStored = true;
                        }

                        // Encode VCALL header payload with proper LICH, SACCH, and
                        // FACCH1-encoded Layer3 VCALL message (per-peer dstId)
                        CNxdnUtils::EncodeVcallHeader(payload, headerSrcId, dstId);

                        // Encode header packet (voice - use DSCP marking)
                        uint8_t flags = NXDN_FLAG_GROUP | NXDN_FLAG_HEADER;
                        EncodeDataPacket(&buffer, headerSrcId, dstId, flags, payload);
                        m_Socket.SendVoice(buffer, client->GetIp());
                    }
                }
            }
            g_Reflector.ReleaseClients();

            // write dstId to cache and update local snapshots (no lock needed for
            // headerSrcId/firstDstId — they're local values from this TX iteration)
            {
                std::lock_guard<std::mutex> lock(m_StreamsCache[iModId].m_Mutex);
                if ( firstDstStored )
                    m_StreamsCache[iModId].m_uiDstId = firstDstId;
            }
            srcIdCopy = headerSrcId;
            dstIdCopy = firstDstStored ? firstDstId : 0;
        }
        else if ( packet->IsLastPacket() )
        {
            // refresh srcId/dstId in case header was processed earlier in this drain cycle
            {
                std::lock_guard<std::mutex> lock(m_StreamsCache[iModId].m_Mutex);
                srcIdCopy = m_StreamsCache[iModId].m_uiSrcId;
                dstIdCopy = m_StreamsCache[iModId].m_uiDstId;
            }

            // Flush any remaining buffered frames (pad with silence)
            if ( m_StreamsCache[iModId].m_uiFrameCount > 0 )
            {
                // Pad remaining frames with silence
                while ( m_StreamsCache[iModId].m_uiFrameCount < 4 )
                {
                    ::memcpy(m_StreamsCache[iModId].m_ambeFrames[m_StreamsCache[iModId].m_uiFrameCount],
                             CNxdnUtils::AMBE_SILENCE, 9);
                    m_StreamsCache[iModId].m_uiFrameCount++;
                }

                // Encode and send final voice packet
                uint8_t payload[NXDN_VOICE_PAYLOAD_SIZE];
                uint8_t *ambes[4] = {
                    m_StreamsCache[iModId].m_ambeFrames[0],
                    m_StreamsCache[iModId].m_ambeFrames[1],
                    m_StreamsCache[iModId].m_ambeFrames[2],
                    m_StreamsCache[iModId].m_ambeFrames[3]
                };
                CNxdnUtils::EncodeVoice(ambes, payload,
                    srcIdCopy, dstIdCopy,
                    m_StreamsCache[iModId].m_uiPacketCount++);

                // Send using pre-built peerIdByAddr map
                CClients *clients = g_Reflector.GetClients();
                int index = -1;
                CClient *client = NULL;
                while ( (client = clients->FindNextClient(PROTOCOL_NXDN, &index)) != NULL )
                {
                    // Note: For NXDN we send to peers even if they're the master
                    if ( client->IsPeer() &&
                         (client->GetReflectorModule() == packet->GetModuleId()) )
                    {
                        // Skip if this stream originated from this client (prevent loop)
                        // Full IP:port match — remote hosts may run multiple modes on different ports
                        if ( hasOwner && ownerIpCopy == client->GetIp() )
                        {
                            continue;
                        }

                        auto it = peerIdByAddr.find(client->GetIp().GetAddr());
                        if ( it != peerIdByAddr.end() )
                        {
                            uint16_t dstId = it->second;

                            uint8_t flags = NXDN_FLAG_GROUP;
                            EncodeDataPacket(&buffer, srcIdCopy, dstId, flags, payload);
                            m_Socket.SendVoice(buffer, client->GetIp());
                        }
                    }
                }
                g_Reflector.ReleaseClients();

                m_StreamsCache[iModId].m_uiFrameCount = 0;
            }

            // Create trailer packet using pre-built peerIdByAddr map
            uint8_t payload[NXDN_VOICE_PAYLOAD_SIZE];
            CClients *clients = g_Reflector.GetClients();
            int index = -1;
            CClient *client = NULL;
            while ( (client = clients->FindNextClient(PROTOCOL_NXDN, &index)) != NULL )
            {
                // Note: For NXDN we send to peers even if they're the master
                if ( client->IsPeer() &&
                     (client->GetReflectorModule() == packet->GetModuleId()) )
                {
                    // Skip if this stream originated from this client (prevent loop)
                    // Full IP:port match — remote hosts may run multiple modes on different ports
                    if ( hasOwner && ownerIpCopy == client->GetIp() )
                    {
                        continue;
                    }

                    auto it = peerIdByAddr.find(client->GetIp().GetAddr());
                    if ( it != peerIdByAddr.end() )
                    {
                        uint16_t dstId = it->second;

                        // Encode TX_REL payload with proper LICH, SACCH, and
                        // FACCH1-encoded Layer3 TX_REL message (per-peer dstId)
                        CNxdnUtils::EncodeTxRel(payload, srcIdCopy, dstId);

                        uint8_t flags = NXDN_FLAG_GROUP | NXDN_FLAG_TRAILER;
                        EncodeDataPacket(&buffer, srcIdCopy, dstId, flags, payload);
                        m_Socket.SendVoice(buffer, client->GetIp());
                    }
                }
            }
            g_Reflector.ReleaseClients();
        }
        else
        {
            // refresh srcId/dstId in case header was processed earlier in this drain cycle
            {
                std::lock_guard<std::mutex> lock(m_StreamsCache[iModId].m_Mutex);
                srcIdCopy = m_StreamsCache[iModId].m_uiSrcId;
                dstIdCopy = m_StreamsCache[iModId].m_uiDstId;
            }

            // Voice frame - buffer until we have 4 frames for NXDN
            CDvFramePacket *dvFrame = (CDvFramePacket *)packet;
            const uint8 *ambeData = dvFrame->GetAmbePlus();

            // Copy AMBE data to buffer (bounds check prevents overflow)
            if ( m_StreamsCache[iModId].m_uiFrameCount < 4 )
            {
                ::memcpy(m_StreamsCache[iModId].m_ambeFrames[m_StreamsCache[iModId].m_uiFrameCount],
                         ambeData, 9);
                m_StreamsCache[iModId].m_uiFrameCount++;
            }

            // When we have 4 frames, encode and send NXDN packet
            if ( m_StreamsCache[iModId].m_uiFrameCount >= 4 )
            {
                // Adjust gain for DMR→NXDN direction
                for (unsigned int j = 0; j < 4; j++) {
                    CYsfUtils::AdjustAmbeGain(m_StreamsCache[iModId].m_ambeFrames[j], GAIN_DMR_TO_NXDN / 10.0f);
                }

                // Encode 4 AMBE frames into NXDN payload
                uint8_t payload[NXDN_VOICE_PAYLOAD_SIZE];
                uint8_t *ambes[4] = {
                    m_StreamsCache[iModId].m_ambeFrames[0],
                    m_StreamsCache[iModId].m_ambeFrames[1],
                    m_StreamsCache[iModId].m_ambeFrames[2],
                    m_StreamsCache[iModId].m_ambeFrames[3]
                };
                CNxdnUtils::EncodeVoice(ambes, payload,
                    srcIdCopy, dstIdCopy,
                    m_StreamsCache[iModId].m_uiPacketCount++);

                // Send to all NXDN peers on this module.
                // Send using pre-built peerIdByAddr map
                CClients *clients = g_Reflector.GetClients();
                int index = -1;
                CClient *client = NULL;
                while ( (client = clients->FindNextClient(PROTOCOL_NXDN, &index)) != NULL )
                {
                    // Note: For NXDN we send to peers even if they're the master,
                    // because NXDNReflector handles source filtering internally
                    if ( client->IsPeer() &&
                         (client->GetReflectorModule() == packet->GetModuleId()) )
                    {
                        // Skip if this stream originated from this client (prevent loop)
                        // Full IP:port match — remote hosts may run multiple modes on different ports
                        if ( hasOwner && ownerIpCopy == client->GetIp() )
                        {
                            continue;
                        }

                        auto it = peerIdByAddr.find(client->GetIp().GetAddr());
                        if ( it != peerIdByAddr.end() )
                        {
                            uint16_t dstId = it->second;

                            uint8_t flags = NXDN_FLAG_GROUP;
                            EncodeDataPacket(&buffer, srcIdCopy, dstId, flags, payload);
                            m_Socket.SendVoice(buffer, client->GetIp());
                        }
                    }
                }
                g_Reflector.ReleaseClients();

                // Reset frame counter
                m_StreamsCache[iModId].m_uiFrameCount = 0;
            }
        }

        // done
        delete packet;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// peer link helpers

void CNxdnProtocol::HandlePeerLinks(void)
{
    CBuffer buffer;

    // snapshot GK peer list into local structures, then release GK lock
    // before acquiring Peers lock — prevents GK+Peers lock-order inversion
    struct NxdnPeerInfo {
        CCallsign callsign;
        CIp       ip;
        uint16    port;
        uint16_t  tg;
        char      modules[2];
    };
    std::vector<NxdnPeerInfo> gkPeers;
    {
        CPeerCallsignList *list = g_GateKeeper.GetPeerList();
        for ( int i = 0; i < list->size(); i++ )
        {
            CCallsignListItem *item = &((list->data())[i]);
            if ( IsNxdnPeerCallsign(item->GetCallsign()) )
            {
                NxdnPeerInfo info;
                info.callsign = item->GetCallsign();
                item->ResolveIp();
                info.ip = item->GetIp();
                info.port = item->GetPort();
                // extract TG from callsign (e.g., "NX12345" -> 12345)
                char csStr[CALLSIGN_LEN + 1];
                item->GetCallsign().GetCallsign((uint8 *)csStr);
                csStr[CALLSIGN_LEN] = '\0';
                info.tg = (uint16_t)::atoi(csStr + 2);
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
    while ( (peer = peers->FindNextPeer(PROTOCOL_NXDN, &index)) != NULL )
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
        std::cout << "NXDN peer " << toRemove[i]->GetCallsign() << " removed - no longer in peer list" << std::endl;
        peers->RemovePeer(toRemove[i]);
    }

    // check if all NXDN peers listed by gatekeeper are connected
    for ( size_t i = 0; i < gkPeers.size(); i++ )
    {
        if ( peers->FindPeer(gkPeers[i].callsign, PROTOCOL_NXDN) == NULL )
        {
            if ( gkPeers[i].port == 0 )
            {
                continue;
            }

            // send poll packet to initiate connection
            std::cout << "NXDN connecting to peer " << gkPeers[i].callsign
                      << " @ " << gkPeers[i].ip << ":" << gkPeers[i].port
                      << " on module " << gkPeers[i].modules << std::endl;
            EncodePollPacket(&buffer, g_Reflector.GetCallsign(), gkPeers[i].tg);
            m_Socket.Send(buffer, gkPeers[i].ip, gkPeers[i].port);
        }
    }

    // done
    g_Reflector.ReleasePeers();
}

void CNxdnProtocol::HandlePeerKeepalives(void)
{
    CBuffer buffer;

    // snapshot callsign→port from GK list, release GK before acquiring Peers
    std::map<uint32_t, uint16_t> peerPortByAddr;
    {
        CPeerCallsignList *list = g_GateKeeper.GetPeerList();
        for ( int i = 0; i < list->size(); i++ )
        {
            CCallsignListItem *item = &((list->data())[i]);
            if ( IsNxdnPeerCallsign(item->GetCallsign()) && item->GetPort() != 0 )
            {
                item->ResolveIp();
                peerPortByAddr[item->GetIp().GetAddr()] = item->GetPort();
            }
        }
        g_GateKeeper.ReleasePeerList();
    }

    // now work with Peers lock only
    CPeers *peers = g_Reflector.GetPeers();

    // send keepalive to all connected NXDN peers, collect timed-out ones for removal
    std::vector<CPeer *> toRemove;
    int index = -1;
    CPeer *peer = NULL;
    while ( (peer = peers->FindNextPeer(PROTOCOL_NXDN, &index)) != NULL )
    {
        CNxdnPeer *nxdnPeer = dynamic_cast<CNxdnPeer *>(peer);
        if ( nxdnPeer == NULL )
        {
            continue;
        }

        if ( !nxdnPeer->IsAlive() )
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
                EncodePollPacket(&buffer, g_Reflector.GetCallsign(), nxdnPeer->GetNxdnId());
                m_Socket.Send(buffer, peer->GetIp(), it->second);
            }
        }
    }
    for ( size_t i = 0; i < toRemove.size(); i++ )
    {
        std::cout << "NXDN peer " << toRemove[i]->GetCallsign() << " keepalive timeout" << std::endl;
        peers->RemovePeer(toRemove[i]);
    }

    // done
    g_Reflector.ReleasePeers();
}

////////////////////////////////////////////////////////////////////////////////////////
// packet decoding helpers

bool CNxdnProtocol::IsValidPollPacket(const CBuffer &Buffer, CCallsign *callsign, uint16_t *tg)
{
    bool valid = false;

    if ( Buffer.size() == NXDN_POLL_PACKET_SIZE )
    {
        if ( ::memcmp(Buffer.data(), NXDN_POLL_PREFIX, 5) == 0 )
        {
            // extract callsign
            callsign->SetCallsign(Buffer.data() + 5, NXDN_CALLSIGN_LENGTH);

            // extract TG (big endian)
            *tg = (Buffer.data()[15] << 8) | Buffer.data()[16];

            valid = true;
        }
    }

    return valid;
}

bool CNxdnProtocol::IsValidDataPacket(const CBuffer &Buffer, uint16_t *srcId, uint16_t *dstId, uint8_t *flags, uint8_t *payload)
{
    bool valid = false;

    if ( Buffer.size() == NXDN_DATA_PACKET_SIZE )
    {
        if ( ::memcmp(Buffer.data(), NXDN_DATA_PREFIX, 5) == 0 )
        {
            // extract source ID (big endian)
            *srcId = (Buffer.data()[5] << 8) | Buffer.data()[6];

            // extract destination ID (big endian)
            *dstId = (Buffer.data()[7] << 8) | Buffer.data()[8];

            // extract flags
            *flags = Buffer.data()[9];

            // extract payload
            ::memcpy(payload, Buffer.data() + 10, NXDN_VOICE_PAYLOAD_SIZE);

            valid = true;
        }
    }

    return valid;
}

////////////////////////////////////////////////////////////////////////////////////////
// packet encoding helpers

void CNxdnProtocol::EncodePollPacket(CBuffer *buffer, const CCallsign &callsign, uint16_t tg) const
{
    char cs[NXDN_CALLSIGN_LENGTH];

    buffer->clear();

    // prefix
    buffer->Append((uint8 *)NXDN_POLL_PREFIX, 5);

    // callsign (10 bytes, space padded)
    ::memset(cs, ' ', sizeof(cs));
    callsign.GetCallsign((uint8 *)cs);
    buffer->Append((uint8 *)cs, NXDN_CALLSIGN_LENGTH);

    // TG (big endian)
    buffer->Append((uint8)((tg >> 8) & 0xFF));
    buffer->Append((uint8)(tg & 0xFF));
}

void CNxdnProtocol::EncodeDataPacket(CBuffer *buffer, uint16_t srcId, uint16_t dstId, uint8_t flags, const uint8_t *payload) const
{
    buffer->clear();

    // prefix
    buffer->Append((uint8 *)NXDN_DATA_PREFIX, 5);

    // source ID (big endian)
    buffer->Append((uint8)((srcId >> 8) & 0xFF));
    buffer->Append((uint8)(srcId & 0xFF));

    // destination ID (big endian)
    buffer->Append((uint8)((dstId >> 8) & 0xFF));
    buffer->Append((uint8)(dstId & 0xFF));

    // flags
    buffer->Append(flags);

    // payload
    buffer->Append((uint8 *)payload, NXDN_VOICE_PAYLOAD_SIZE);
}

////////////////////////////////////////////////////////////////////////////////////////
// peer helpers

bool CNxdnProtocol::IsNxdnPeerCallsign(const CCallsign &callsign) const
{
    char cs[CALLSIGN_LEN + 1];
    callsign.GetCallsign((uint8 *)cs);
    cs[CALLSIGN_LEN] = '\0';
    // NXDN peers use "NXnnnnn" format (7 chars, fits in standard callsign)
    return (::strncmp(cs, "NX", 2) == 0) && (cs[2] >= '0') && (cs[2] <= '9');
}

CCallsignListItem *CNxdnProtocol::FindNxdnPeerByIp(CPeerCallsignList *list, const CIp &ip)
{
    // Search for an NXDN peer matching this IP. Port-aware disambiguation
    // — see FindYsfPeerByIp in cysfprotocol.cpp for the full rationale.
    // Briefly: if the interlink entry declares an explicit port, match
    // (address, port); otherwise fall back to address-only for backward
    // compatibility with single-peer-per-IP configurations.
    for ( int i = 0; i < list->size(); i++ )
    {
        CCallsignListItem *item = &((list->data())[i]);

        // Check if this is an NXDN peer (callsign starts with "NX" + digit)
        if ( IsNxdnPeerCallsign(item->GetCallsign()) )
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
// NXDN ID helpers

uint16_t CNxdnProtocol::CallsignToNxdnId(const CCallsign &callsign) const
{
    uint16_t nxdnId = 0;

    // Look up NXDN ID from database
    g_NxdnIdDir.Lock();
    {
        nxdnId = g_NxdnIdDir.FindNxdnId(callsign);
    }
    g_NxdnIdDir.Unlock();

    // Return found ID or default NXDN2DMR ID if not found
    return (nxdnId != 0) ? nxdnId : NXDN_ID_NXDN2DMR;
}

////////////////////////////////////////////////////////////////////////////////////////
// uiStreamId helpers

uint32 CNxdnProtocol::IpToStreamId(const CIp &ip) const
{
    // XOR port into both halves to distinguish localhost peers on different ports
    return ip.GetAddr() ^ (uint32)(MAKEDWORD(ip.GetPort(), ip.GetPort()));
}

// Build the map key. (addr<<16)|port keeps the same source endpoint
// keyed identically for header / voice / trailer frames within one
// transmission, and distinguishes localhost peers using different
// ephemeral ports.
static inline uint64_t NxdnSourceKey(const CIp &ip)
{
    return ((uint64_t)ip.GetAddr() << 16) | (uint64_t)(ip.GetPort() & 0xFFFFu);
}

// Allocate a fresh reflector-side stream-id for a NEW header arrival.
// Uses the process-wide CProtocol::AllocateGlobalStreamIdNonce — see
// cprotocol.cpp for the rationale (random seed at startup, shared
// across protocols to avoid inter-protocol sid collisions). The
// per-protocol collision-against-m_SourceStates check is kept as
// defence-in-depth for the rare wrap-around case.
uint32 CNxdnProtocol::AllocateNewStreamIdForSource(const CIp &ip)
{
    std::lock_guard<std::mutex> lock(m_SourceStatesMutex);

    const uint64_t key = NxdnSourceKey(ip);

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

// Look up the active sid for a voice or trailer frame. Falls back to
// the deterministic IpToStreamId() when no header has been seen for
// this source — preserves the late-entry buffering path in
// CProtocol::OnDvFramePacketIn for orphan mid-stream packets.
uint32 CNxdnProtocol::LookupStreamIdForSource(const CIp &ip) const
{
    std::lock_guard<std::mutex> lock(m_SourceStatesMutex);
    auto it = m_SourceStates.find(NxdnSourceKey(ip));
    if (it != m_SourceStates.end())
    {
        return it->second;
    }
    return IpToStreamId(ip);
}

// Drop the IP→sid mapping after the trailer is processed. The next
// header from this source will allocate a fresh sid.
void CNxdnProtocol::ReleaseStreamIdForSource(const CIp &ip)
{
    std::lock_guard<std::mutex> lock(m_SourceStatesMutex);
    m_SourceStates.erase(NxdnSourceKey(ip));
}
