//
//  cm17protocol.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 28/11/2025.
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
#include "cm17client.h"
#include "cm17protocol.h"
#include "creflector.h"
#include "cgatekeeper.h"

// Note: Software transcoding removed - M17 <-> AMBE transcoding will be
// handled by ambed when Codec2 support is added there

////////////////////////////////////////////////////////////////////////////////////////
// operation

bool CM17Protocol::Init(void)
{
    bool ok;

    // base class
    ok = CProtocol::Init();

    // create our socket
    ok &= m_Socket.Open(M17_PORT);

    // update time
    m_LastKeepaliveTime.Now();

    // done
    return ok;
}

void CM17Protocol::Close(void)
{
    // notify all connected M17 clients that the reflector is shutting down
    // so they disconnect immediately rather than waiting for keepalive timeout
    CClients *clients = g_Reflector.GetClients();
    int index = -1;
    CClient *client = NULL;
    while ( (client = clients->FindNextClient(PROTOCOL_M17, &index)) != NULL )
    {
        CBuffer disconnect;
        EncodeDisconnectPacket(&disconnect, client->GetReflectorModule());
        m_Socket.Send(disconnect, client->GetIp());
    }
    g_Reflector.ReleaseClients();

    // base class stops the thread
    CProtocol::Close();
}

////////////////////////////////////////////////////////////////////////////////////////
// task

void CM17Protocol::RxTask(void)
{
    CBuffer             Buffer;
    CIp                 Ip;
    CCallsign           Callsign;
    char                Module;
    bool                ListenOnly;
    CDvHeaderPacket     *Header;
    CDvFramePacket      *Frame;
    CDvLastFramePacket  *LastFrame;

    // handle incoming packets
    if ( m_Socket.Receive(&Buffer, &Ip, 20) != -1 )
    {
        // crack the packet
        // Check order: last frame (EOT) first, then header (seq 0), then regular frame (seq > 0)
        if ( IsValidDvLastFramePacket(Buffer, &LastFrame) )
        {
            // check if this is from a listen-only client
            CClients *clients = g_Reflector.GetClients();
            CClient *client = clients->FindClient(Ip, PROTOCOL_M17);
            if ( client != NULL )
            {
                CM17Client *m17client = dynamic_cast<CM17Client *>(client);
                if ( m17client != NULL && m17client->IsListenOnly() )
                {
                    // listen-only clients cannot transmit
                    g_Reflector.ReleaseClients();
                    delete LastFrame;
                    return;
                }
            }
            g_Reflector.ReleaseClients();

            // M17 last frame also contains TWO codec2 frames
            // Process first codec2 frame as regular frame, then second as last frame
            uint8 codec2_frame1[8];
            uint8 codec2_frame2[8];
            ::memcpy(codec2_frame1, Buffer.data() + 36, 8);
            ::memcpy(codec2_frame2, Buffer.data() + 36 + 8, 8);

            uint16 streamId = LastFrame->GetStreamId();

            // Some M17 clients send the EOT with a byte-swapped stream ID.
            // If the stream ID doesn't match an active stream, try swapped.
            if ( GetStream(streamId, &Ip) == NULL )
            {
                uint16 swappedId = (uint16)(((streamId >> 8) & 0xFF) | ((streamId & 0xFF) << 8));
                if ( GetStream(swappedId, &Ip) != NULL )
                {
                    streamId = swappedId;
                }
            }

            uint8 m17seq = LastFrame->GetPacketId();
            // Multiply by 2 for 40ms->20ms conversion, mask to uint8 range
            uint8 pid1 = (uint8)((m17seq * 2) & 0xFF);
            uint8 pid2 = (uint8)((m17seq * 2 + 1) & 0xFF);

            // Create regular frame for first codec2 frame
            CDvFramePacket *Frame1 = new CDvFramePacket(codec2_frame1, streamId, pid1);

            // Update last frame with second codec2 frame and correct packet ID
            delete LastFrame;
            LastFrame = new CDvLastFramePacket(codec2_frame2, streamId, pid2);

            // Process first frame as regular, then last frame closes stream
            OnDvFramePacketIn(Frame1, &Ip);
            OnDvLastFramePacketIn(LastFrame, &Ip);
        }
        else if ( IsValidDvHeaderPacket(Buffer, &Header) )
        {
            // check if this is from a listen-only client
            CClients *clients = g_Reflector.GetClients();
            CClient *client = clients->FindClient(Ip, PROTOCOL_M17);
            if ( client != NULL )
            {
                CM17Client *m17client = dynamic_cast<CM17Client *>(client);
                if ( m17client != NULL && m17client->IsListenOnly() )
                {
                    // listen-only clients cannot transmit
                    g_Reflector.ReleaseClients();
                    delete Header;
                    return;
                }
            }
            g_Reflector.ReleaseClients();

            // callsign muted?
            if ( g_GateKeeper.MayTransmit(Header->GetMyCallsign(), Ip, PROTOCOL_M17) )
            {
                // handle it
                OnDvHeaderPacketIn(Header, Ip);
            }
            else
            {
                delete Header;
            }
        }
        else if ( IsValidDvFramePacket(Buffer, &Frame) )
        {
            // check if this is from a listen-only client
            CClients *clients = g_Reflector.GetClients();
            CClient *client = clients->FindClient(Ip, PROTOCOL_M17);
            if ( client != NULL )
            {
                CM17Client *m17client = dynamic_cast<CM17Client *>(client);
                if ( m17client != NULL && m17client->IsListenOnly() )
                {
                    // listen-only clients cannot transmit
                    g_Reflector.ReleaseClients();
                    delete Frame;
                    return;
                }
            }
            g_Reflector.ReleaseClients();

            // M17 packets contain TWO codec2 frames (40ms total, 16 bytes)
            // We need to create a second frame packet for the second codec2 frame
            // The first 8 bytes are already in Frame, extract second 8 bytes
            uint8 codec2_frame2[8];
            ::memcpy(codec2_frame2, Buffer.data() + 36 + 8, 8);  // second codec2 frame

            // Create second frame packet with incremented packet ID
            // Multiply by 2 for 40ms->20ms conversion, mask to uint8 range
            CDvFramePacket *Frame2 = new CDvFramePacket(codec2_frame2, Frame->GetStreamId(),
                                                         (uint8)(((Frame->GetPacketId() * 2) + 1) & 0xFF));

            // Update first frame's packet ID to be even (Frame1 = seq*2, Frame2 = seq*2+1)
            // We need to recreate Frame with correct packet ID
            uint8 codec2_frame1[8];
            ::memcpy(codec2_frame1, Buffer.data() + 36, 8);  // first codec2 frame
            uint16 streamId = Frame->GetStreamId();
            uint8 pid1 = (uint8)((Frame->GetPacketId() * 2) & 0xFF);
            delete Frame;
            Frame = new CDvFramePacket(codec2_frame1, streamId, pid1);

            // Process both frames
            OnDvFramePacketIn(Frame, &Ip);
            OnDvFramePacketIn(Frame2, &Ip);
        }
        else if ( IsValidConnectPacket(Buffer, &Callsign, &Module, &ListenOnly) )
        {
            std::cout << "M17 connect packet from " << Callsign << " at " << Ip << " for module " << Module
                      << (ListenOnly ? " (listen-only)" : "") << std::endl;

            // callsign authorized?
            if ( g_GateKeeper.MayLink(Callsign, Ip, PROTOCOL_M17) )
            {
                // add client if needed
                CClients *clients = g_Reflector.GetClients();
                CClient *client = clients->FindClient(Callsign, Ip, PROTOCOL_M17);

                // client already connected?
                if ( client == NULL )
                {
                    std::cout << "M17 login from " << Callsign << " at " << Ip
                              << (ListenOnly ? " as listen-only" : "") << std::endl;

                    // create the client and link to module
                    CM17Client *newclient = new CM17Client(Callsign, Ip, Module, ListenOnly);

                    // append
                    clients->AddClient(newclient);
                }
                else
                {
                    // update module and listen-only status
                    client->SetReflectorModule(Module);
                    CM17Client *m17client = dynamic_cast<CM17Client *>(client);
                    if ( m17client != NULL )
                    {
                        m17client->SetListenOnly(ListenOnly);
                    }
                    client->Alive();
                }

                g_Reflector.ReleaseClients();

                // acknowledge
                EncodeConnectAckPacket(&Buffer);
                m_Socket.Send(Buffer, Ip);
            }
            else
            {
                // deny
                EncodeConnectNackPacket(&Buffer);
                m_Socket.Send(Buffer, Ip);
            }
        }
        else if ( IsValidDisconnectPacket(Buffer, &Callsign) )
        {
            std::cout << "M17 disconnect packet from " << Callsign << " at " << Ip << std::endl;

            // find client & remove it
            CClients *clients = g_Reflector.GetClients();
            CClient *client = clients->FindClient(Ip, PROTOCOL_M17);
            if ( client != NULL )
            {
                clients->RemoveClient(client);
                g_Reflector.ReleaseClients();

                // acknowledge disconnection only for known clients
                EncodeDisconnectedPacket(&Buffer);
                m_Socket.Send(Buffer, Ip);
            }
            else
            {
                g_Reflector.ReleaseClients();
                // don't ACK disconnects from unknown IPs — avoids revealing server to probers
            }
        }
        else if ( IsValidKeepAlivePacket(Buffer, &Callsign) )
        {
            // find all clients with that callsign & ip and keep them alive
            CClients *clients = g_Reflector.GetClients();
            int index = -1;
            CClient *client = NULL;
            while ( (client = clients->FindNextClient(Callsign, Ip, PROTOCOL_M17, &index)) != NULL )
            {
                client->Alive();
            }
            g_Reflector.ReleaseClients();
        }
    }

    // handle end of streaming timeout
    CheckStreamsTimeout();

}

////////////////////////////////////////////////////////////////////////////////////////
// TX task — runs on separate thread, never blocked by CloseStream

void CM17Protocol::TxTask(void)
{
    // wait for queue notification or 20ms timeout
    {
        std::unique_lock<std::mutex> lk(m_QueueCondMutex);
        m_QueueCondVar.wait_for(lk, std::chrono::milliseconds(20));
    }

    // IMPORTANT: HandleQueue MUST run before CheckForMissedEOT.
    // HandleQueue processes the real last frame and clears m_bStreamActive.
    // If the order were reversed, CheckForMissedEOT would see a stale
    // m_bStreamActive flag and fire a spurious synthetic EOT on every
    // normal stream close.
    HandleQueue();
    CheckForMissedEOT();

    // keep client alive
    if ( m_LastKeepaliveTime.DurationSinceNow() > M17_KEEPALIVE_PERIOD )
    {
        HandleKeepalives();

        // update time
        m_LastKeepaliveTime.Now();
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// streams helpers

bool CM17Protocol::OnDvHeaderPacketIn(CDvHeaderPacket *Header, const CIp &Ip)
{
    bool newstream = false;

    // set default suffix if not already set
    if ( !Header->HasMySuffix() )
    {
        Header->SetMySuffix("M17");
    }

    // find the stream (match on both StreamId and IP to avoid false matches)
    CPacketStream *stream = GetStream(Header->GetStreamId(), &Ip);
    if ( stream == NULL )
    {
        // no stream open yet, open a new one
        // first find this client
        CClient *client = g_Reflector.GetClients()->FindClient(Ip, PROTOCOL_M17);
        if ( client != NULL )
        {
            // check module is valid
            if ( client->HasReflectorModule() && g_Reflector.IsValidModule(client->GetReflectorModule()) )
            {
                // update header module
                Header->SetRpt2Module(client->GetReflectorModule());

                // save callsigns before OpenStream transfers Header ownership
                CCallsign myCs = Header->GetMyCallsign();
                CCallsign rpt1Cs = Header->GetRpt1Callsign();
                CCallsign rpt2Cs = Header->GetRpt2Callsign();
                char savedModule = client->GetReflectorModule();

                // try to open the stream
                if ( (stream = g_Reflector.OpenStream(Header, client)) != NULL )
                {
                    m_Streams.push_back(stream);
                    newstream = true;
                    Header = NULL;
                    std::cout << "M17 stream opened for " << myCs
                              << " on module " << savedModule << std::endl;

                    // update cache (slot lock protects against TX thread reads)
                    int iModId = g_Reflector.GetModuleIndex(savedModule);
                    if ( iModId >= 0 && iModId < NB_OF_MODULES )
                    {
                        std::lock_guard<std::mutex> lock(m_StreamsCache[iModId].m_Mutex);
                        m_StreamsCache[iModId].m_dvHeader = CDvHeaderPacket(myCs, CCallsign("CQCQCQ"), rpt1Cs, rpt2Cs, 0, 0);
                    }

                    g_Reflector.GetUsers()->Hearing(myCs, rpt1Cs, rpt2Cs);
                    g_Reflector.ReleaseUsers();
                }
                else if ( g_Reflector.TryLateEntry(Header, client) )
                {
                    Header = NULL;
                }
            }
        }

        // release
        g_Reflector.ReleaseClients();

        // delete header if needed
        if ( !newstream && Header != NULL )
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

void CM17Protocol::RefreshClientCache(int iModId)
{
    // bounds check
    if ( iModId < 0 || iModId >= NB_OF_MODULES )
        return;

    // Check freshness under cache lock, release before acquiring Clients lock
    {
        std::lock_guard<std::mutex> lock(m_ClientCache[iModId].m_Mutex);
        if ( m_ClientCache[iModId].m_bInitialized &&
             m_ClientCache[iModId].m_LastRefresh.DurationSinceNow() < M17_CLIENT_CACHE_REFRESH_INTERVAL )
        {
            return;
        }
    }

    // Scan clients WITHOUT holding cache lock
    char moduleId = (char)('A' + iModId);
    std::vector<CIp> freshIps;

    CClients *clients = g_Reflector.GetClients();
    int index = -1;
    CClient *client = NULL;
    while ( (client = clients->FindNextClient(PROTOCOL_M17, &index)) != NULL )
    {
        if ( !client->IsAMaster() && (client->GetReflectorModule() == moduleId) )
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

void CM17Protocol::SendToModuleClients(int iModId, const CBuffer &buffer, uint16 streamId, bool hasOwner, const CIp &ownerIp)
{
    // bounds check
    if ( iModId < 0 || iModId >= NB_OF_MODULES )
        return;

    // refresh the cache if stale
    RefreshClientCache(iModId);

    // send to cached clients (cache mutex is TX-thread-only, safe to hold during sends)
    std::lock_guard<std::mutex> lock(m_ClientCache[iModId].m_Mutex);
    for ( const auto &ip : m_ClientCache[iModId].m_ClientIps )
    {
        if ( hasOwner && ownerIp == ip )
        {
            continue;
        }

        m_Socket.SendVoice(buffer, ip);
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// M17 EOT synthesis — detect streams that ended without a proper last frame

void CM17Protocol::CheckForMissedEOT(void)
{
    // Timer-based abandoned-stream detection: no cross-thread stream state access.
    //
    // Trigger condition: m_bStreamActive is still true AND no upstream DV
    // frame (or last frame) has been observed on this module's output queue
    // for MISSED_EOT_TIMEOUT seconds. The timestamp tracks upstream-side
    // activity (see m_LastUpstreamActivityTime in cm17protocol.h for the
    // full rationale — particularly why "M17 wire emit" was the wrong
    // proxy and how it produced premature EOTs when ambed starved).
    //
    // Sizing of MISSED_EOT_TIMEOUT: the transcoder pipeline has up to
    // ~300 ms of latency (160 ms jitter buffer + ambed RTT). After upstream
    // genuinely stops sending, the last DV frame takes that long to
    // traverse the pipeline and arrive in HandleQueue, where it refreshes
    // m_LastUpstreamActivityTime one final time and then (if it is the
    // last frame) clears m_bStreamActive at line 735 — in which case
    // this check can no longer fire at all.
    //
    // Normal close:  last frame arrives within ~300ms, HandleQueue clears
    //                m_bStreamActive before the 500ms window expires — timer never fires.
    // Timeout/crash: upstream goes silent without a last frame; jitter drains
    //                in ~160 ms, then this timer fires 500 ms later (~660 ms
    //                after real silence) — well under STREAM_TIMEOUT (1.6 s).
    // New stream before timeout: the header branch in HandleQueue detects
    //                m_bStreamActive and calls SendSyntheticEOT first.
    static const double MISSED_EOT_TIMEOUT = 0.5;  // 500ms — must exceed worst-case
                                                    // pipeline delay (~300ms) and be
                                                    // well under STREAM_TIMEOUT (1.6s)

    // skip all mutex acquisitions when no streams are active
    if ( m_nActiveStreams.load(std::memory_order_relaxed) <= 0 )
        return;

    for ( int i = 0; i < NB_OF_MODULES; i++ )
    {
        bool shouldSend = false;
        {
            std::lock_guard<std::mutex> lock(m_StreamsCache[i].m_Mutex);
            shouldSend = m_StreamsCache[i].m_bStreamActive &&
                         m_StreamsCache[i].m_LastUpstreamActivityTime.DurationSinceNow() > MISSED_EOT_TIMEOUT;
        }
        if ( shouldSend )
        {
            SendSyntheticEOT(i);
        }
    }
}

void CM17Protocol::SendSyntheticEOT(int iModId)
{
    if ( iModId < 0 || iModId >= NB_OF_MODULES )
        return;

    // build a 54-byte M17 EOT packet using cached stream data (under slot lock)
    static const uint8 CODEC2_SILENCE[8] = { 0xC0, 0x00, 0x6A, 0x43, 0x9C, 0xE4, 0x21, 0x08 };
    uint8 tag[] = { 'M','1','7',' ' };

    CBuffer buffer;
    {
        std::lock_guard<std::mutex> lock(m_StreamsCache[iModId].m_Mutex);

        if ( !m_StreamsCache[iModId].m_bStreamActive || m_StreamsCache[iModId].m_uiOutboundStreamId == 0 )
            return;

        buffer.Set(tag, sizeof(tag));

        uint16 streamId = m_StreamsCache[iModId].m_uiOutboundStreamId;
        buffer.Append((uint8)HIBYTE(streamId));
        buffer.Append((uint8)LOBYTE(streamId));

        uint8 dest[6];
        EncodeCallsign(m_StreamsCache[iModId].m_dvHeader.GetUrCallsign(), dest);
        buffer.Append(dest, 6);

        uint8 source[6];
        EncodeCallsign(m_StreamsCache[iModId].m_dvHeader.GetMyCallsign(), source);
        buffer.Append(source, 6);

        buffer.Append((uint8)0);
        buffer.Append((uint8)M17_FRAMETYPE_VOICE);

        for ( int i = 0; i < 14; i++ )
            buffer.Append((uint8)0);

        uint16 seqWithEOT = (m_StreamsCache[iModId].m_uiFrameSequence & 0x7FFF) | 0x8000;
        buffer.Append((uint8)HIBYTE(seqWithEOT));
        buffer.Append((uint8)LOBYTE(seqWithEOT));

        buffer.Append((uint8 *)CODEC2_SILENCE, 8);
        buffer.Append((uint8 *)CODEC2_SILENCE, 8);

        buffer.Append((uint8)0);
        buffer.Append((uint8)0);

        std::cout << "M17 synthetic EOT sent for module " << (char)('A' + iModId)
                  << " sid " << streamId << std::endl;

        if ( m_StreamsCache[iModId].m_bStreamActive )
        {
            m_StreamsCache[iModId].m_bStreamActive = false;
            m_nActiveStreams.fetch_sub(1, std::memory_order_relaxed);
        }
        m_StreamsCache[iModId].m_bHasPendingCodec2 = false;
        m_StreamsCache[iModId].m_uiFrameSequence = 0;
    } // slot lock released before network I/O

    // send to ALL clients on this module — no owner exclusion
    // (every M17 client needs the EOT to close their stream state machine)
    RefreshClientCache(iModId);
    std::vector<CIp> localIps;
    {
        std::lock_guard<std::mutex> lock(m_ClientCache[iModId].m_Mutex);
        localIps = m_ClientCache[iModId].m_ClientIps;
    }
    for ( const CIp &ip : localIps )
    {
        m_Socket.SendVoice(buffer, ip);
    }

    // Also push a CDvLastFramePacket into the reflector stream so the router
    // distributes it to all other protocols (DCS, DExtra, etc.). Without this,
    // cross-mode listeners never receive the EOT.
    static const uint8 CODEC2_SILENCE_FRAME[8] = { 0xC0, 0x00, 0x6A, 0x43, 0x9C, 0xE4, 0x21, 0x08 };
    char moduleChar = (char)('A' + iModId);
    CPacketStream *stream = g_Reflector.GetStream(moduleChar);
    if ( stream != NULL )
    {
        // Read the internal stream ID under the slot lock (already released above)
        uint16 internalSid;
        {
            std::lock_guard<std::mutex> lock(m_StreamsCache[iModId].m_Mutex);
            internalSid = m_StreamsCache[iModId].m_uiInternalStreamId;
        }

        CDvLastFramePacket *lastFrame = new CDvLastFramePacket(CODEC2_SILENCE_FRAME, internalSid, 0);
        stream->Lock();
        if ( stream->IsOpen() && stream->GetStreamId() == internalSid )
        {
            stream->push(lastFrame);  // push directly to stream queue (bypass codec)
        }
        else
        {
            delete lastFrame;  // stream already closed or reused — discard
        }
        stream->Unlock();
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// queue helper

void CM17Protocol::HandleQueue(void)
{
    // drain all packets into a local vector under queue lock
    std::vector<CPacket *> packets;
    m_Queue.Lock();
    while ( !m_Queue.empty() )
    {
        packets.push_back(m_Queue.front());
        m_Queue.pop();
    }
    m_Queue.Unlock();

    // process drained packets without holding the queue lock
    for ( auto *packet : packets )
    {
        // get our sender's id
        int iModId = g_Reflector.GetModuleIndex(packet->GetModuleId());

        // Bounds check for module index
        if ( iModId < 0 || iModId >= NB_OF_MODULES )
        {
            delete packet;
            continue;
        }

        // check for orphaned stream before acquiring slot lock (SendSyntheticEOT does I/O)
        if ( packet->IsDvHeader() )
        {
            bool needsSyntheticEOT = false;
            {
                std::lock_guard<std::mutex> lock(m_StreamsCache[iModId].m_Mutex);
                needsSyntheticEOT = m_StreamsCache[iModId].m_bStreamActive;
            }
            if ( needsSyntheticEOT )
            {
                SendSyntheticEOT(iModId);
            }
        }

        // encode under slot lock, send without it
        CBuffer buffer;
        bool hasOwner;
        CIp ownerIp;
        {
            std::lock_guard<std::mutex> lock(m_StreamsCache[iModId].m_Mutex);
            // snapshot owner exclusion fields for send loop
            hasOwner = m_StreamsCache[iModId].m_bHasOwner;
            ownerIp = m_StreamsCache[iModId].m_OwnerIp;

            // check if it's header
            if ( packet->IsDvHeader() )
            {

                // update local stream cache
                m_StreamsCache[iModId].m_dvHeader = CDvHeaderPacket((const CDvHeaderPacket &)*packet);
                m_StreamsCache[iModId].m_uiInternalStreamId = packet->GetStreamId();
                m_StreamsCache[iModId].m_uiOutboundStreamId = m_StreamsCache[iModId].m_uiNextStreamId++;
                if ( m_StreamsCache[iModId].m_uiNextStreamId == 0 )
                    m_StreamsCache[iModId].m_uiNextStreamId = 1;
                m_StreamsCache[iModId].m_uiFrameSequence = 0;
                m_StreamsCache[iModId].m_bHasPendingCodec2 = false;
                m_StreamsCache[iModId].ResetStats();

                // capture owner IP — lock order: slot mutex → stream lock
                m_StreamsCache[iModId].m_bHasOwner = false;
                char moduleChar = (char)('A' + iModId);
                CPacketStream *stream = g_Reflector.GetStream(moduleChar);
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
            // check if it's a last frame
            else if ( packet->IsLastPacket() )
            {
                EncodeDvLastFramePacket((const CDvLastFramePacket &)*packet,
                                         m_StreamsCache[iModId].m_uiOutboundStreamId,
                                         &buffer);
            }
            // otherwise, just a regular DV frame
            else if ( packet->IsDvFrame() )
            {
                CDvFramePacket *dvPacket = (CDvFramePacket *)packet;
                m_StreamsCache[iModId].m_uiPacketsIn++;

                if ( dvPacket->HasCodec2Data() )
                {
                    EncodeDvFramePacket(*dvPacket,
                                        m_StreamsCache[iModId].m_uiOutboundStreamId,
                                        &buffer);
                }
            }

            // clear active flag on last frame
            if ( packet->IsLastPacket() )
            {
                if ( m_StreamsCache[iModId].m_bStreamActive )
                {
                    m_StreamsCache[iModId].m_bStreamActive = false;
                    m_nActiveStreams.fetch_sub(1, std::memory_order_relaxed);
                }
            }

            // Upstream activity timestamp — refresh on every DV-frame (and
            // last-frame, which is a CDvFramePacket subclass so IsDvFrame()
            // is true) regardless of whether ambed's Codec2 response arrived
            // in time to produce wire bytes. The previous version gated this
            // update on `buffer.size() > 0`, which conflated "M17 emitted
            // wire bytes" with "upstream is still alive" — they diverge
            // whenever the ambed pipeline starves, and the divergence
            // caused premature synthetic-EOT firing mid-transmission. See
            // the m_LastUpstreamActivityTime header comment for the full
            // rationale. Headers are intentionally excluded: m_bStreamActive
            // is still false at that point (set on first wire-emitted frame
            // below) so CheckForMissedEOT can't fire on a header-only stream
            // regardless of this timestamp.
            if ( packet->IsDvFrame() )
            {
                m_StreamsCache[iModId].m_LastUpstreamActivityTime.Now();
            }

            // Stream-active flag — kept gated on wire-emission so the flag
            // means "we told M17 listeners about this stream." Synthetic-EOT
            // cleanup is only meaningful for streams listeners are following;
            // a Codec2-empty stream that never emitted wire bytes doesn't
            // need an EOT (listeners have no state machine to tear down).
            if ( buffer.size() > 0 )
            {
                if ( packet->IsDvFrame() && !packet->IsLastPacket() )
                {
                    if ( !m_StreamsCache[iModId].m_bStreamActive )
                    {
                        m_StreamsCache[iModId].m_bStreamActive = true;
                        m_nActiveStreams.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            }
        } // slot lock released before network I/O

        // send it (no slot lock held)
        if ( buffer.size() > 0 )
        {
            SendToModuleClients(iModId, buffer, packet->GetStreamId(), hasOwner, ownerIp);
        }

        // done
        delete packet;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// keepalive helpers

void CM17Protocol::HandleKeepalives(void)
{
    // M17 protocol keepalive is client task
    // here, send ping packets and check that all clients are still alive

    // iterate on clients
    CClients *clients = g_Reflector.GetClients();
    int index = -1;
    CClient *client = NULL;
    std::vector<CClient *> toRemove;
    while ( (client = clients->FindNextClient(PROTOCOL_M17, &index)) != NULL )
    {
        // is this client busy?
        if ( client->IsAMaster() )
        {
            // yes, just tickle it
            client->Alive();
        }
        // check it's still with us
        else if ( !client->IsAlive() )
        {
            // no, disconnect - send disconnect packet first (like mrefd does)
            CBuffer disconnect;
            EncodeDisconnectPacket(&disconnect, client->GetReflectorModule());
            m_Socket.Send(disconnect, client->GetIp());

            std::cout << "M17 client " << client->GetCallsign() << " keepalive timeout" << std::endl;
            toRemove.push_back(client);
        }
        else
        {
            // send keepalive (PING with encoded callsign, like mrefd)
            CBuffer keepalive;
            EncodePingPacket(&keepalive);
            m_Socket.Send(keepalive, client->GetIp());
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

bool CM17Protocol::IsValidConnectPacket(const CBuffer &Buffer, CCallsign *callsign, char *module, bool *listenOnly)
{
    bool valid = false;
    *listenOnly = false;

    if ( Buffer.size() == 11 )
    {
        // check for CONN (normal client) or LSTN (listen-only client)
        if ( Buffer.Compare((uint8 *)"CONN", 4) == 0 )
        {
            // normal connect
            *listenOnly = false;
        }
        else if ( Buffer.Compare((uint8 *)"LSTN", 4) == 0 )
        {
            // listen-only connect
            *listenOnly = true;
        }
        else
        {
            return false;
        }

        // decode callsign
        valid = DecodeCallsign(Buffer.data() + 4, callsign);
        if ( valid )
        {
            // get module
            *module = (char)Buffer.data()[10];

            // validate module - reject if not A-Z (like mrefd does)
            if ( *module < 'A' || *module > 'Z' )
            {
                valid = false;
            }
        }
    }
    return valid;
}

bool CM17Protocol::IsValidDisconnectPacket(const CBuffer &Buffer, CCallsign *callsign)
{
    bool valid = false;

    if ( (Buffer.size() == 10) && (Buffer.Compare((uint8 *)"DISC", 4) == 0) )
    {
        // decode callsign
        valid = DecodeCallsign(Buffer.data() + 4, callsign);
    }
    return valid;
}

bool CM17Protocol::IsValidKeepAlivePacket(const CBuffer &Buffer, CCallsign *callsign)
{
    bool valid = false;

    // accept both PING and PONG as keepalive (like mrefd does)
    if ( Buffer.size() == 10 )
    {
        const uint8 *data = Buffer.data();
        if ( (data[0] == 'P') &&
             (data[1] == 'I' || data[1] == 'O') &&
             (data[2] == 'N') &&
             (data[3] == 'G') )
        {
            // decode callsign
            valid = DecodeCallsign(Buffer.data() + 4, callsign);
        }
    }
    return valid;
}

bool CM17Protocol::IsValidDvHeaderPacket(const CBuffer &Buffer, CDvHeaderPacket **header)
{
    bool valid = false;
    *header = NULL;

    if ( (Buffer.size() == 54) && (Buffer.Compare((uint8 *)"M17 ", 4) == 0) )
    {
        // check frame type
        uint8 frameType = Buffer.data()[19];

        if ( frameType == M17_FRAMETYPE_VOICE )
        {
            // get sequence number with EOT flag
            uint16 sequenceWithEOT = MAKEWORD(Buffer.data()[35], Buffer.data()[34]);

            // Header is detected on sequence 0 (first frame of stream) without EOT flag
            if ( (sequenceWithEOT & 0x7FFF) == 0 && !(sequenceWithEOT & 0x8000) )
            {
                // get stream ID
                uint16 streamId = MAKEWORD(Buffer.data()[5], Buffer.data()[4]);

                // decode callsigns
                CCallsign csSource, csDest;
                bool srcOk = DecodeCallsign(Buffer.data() + 12, &csSource);
                bool dstOk = DecodeCallsign(Buffer.data() + 6, &csDest);

                // Destination can be reflector (M17-XXX) or broadcast (ALL)
                bool dstValid = dstOk || IsValidM17Destination(csDest);

                if ( srcOk && dstValid )
                {
                    // build header
                    // RPT1 uses caller's callsign + 'B' — D-Star hotspot convention.
                    // See cnxdnprotocol.cpp for the rationale.
                    CCallsign rpt1 = csSource;
                    rpt1.SetModule('B');
                    CCallsign rpt2 = m_ReflectorCallsign;
                    rpt2.SetModule(' ');

                    *header = new CDvHeaderPacket(csSource, csDest, rpt1, rpt2, streamId, 0);
                    // Check source callsign validity
                    valid = csSource.IsValid();

                    if ( !valid )
                    {
                        delete *header;
                        *header = NULL;
                    }
                }
            }
        }
    }
    return valid;
}

bool CM17Protocol::IsValidDvFramePacket(const CBuffer &Buffer, CDvFramePacket **frame)
{
    bool valid = false;
    *frame = NULL;

    if ( (Buffer.size() == 54) && (Buffer.Compare((uint8 *)"M17 ", 4) == 0) )
    {
        // check frame type
        uint8 frameType = Buffer.data()[19];

        if ( frameType == M17_FRAMETYPE_VOICE )
        {
            // get sequence number with EOT flag
            uint16 sequenceWithEOT = MAKEWORD(Buffer.data()[35], Buffer.data()[34]);
            uint16 sequence = sequenceWithEOT & 0x7FFF;

            // Only match frames with sequence > 0 and no EOT flag
            // (sequence 0 is handled as header, EOT is handled as last frame)
            if ( sequence > 0 && !(sequenceWithEOT & 0x8000) )
            {
                // get stream ID
                uint16 streamId = MAKEWORD(Buffer.data()[5], Buffer.data()[4]);

                // get codec2 payload (16 bytes)
                uint8 codec2[16];
                ::memcpy(codec2, Buffer.data() + 36, 16);

                // create frame packet (cast sequence to uint8 for D-Star packet ID)
                *frame = new CDvFramePacket(codec2, streamId, (uint8)(sequence & 0xFF));
                valid = true;
            }
        }
    }
    return valid;
}

bool CM17Protocol::IsValidDvLastFramePacket(const CBuffer &Buffer, CDvLastFramePacket **frame)
{
    bool valid = false;
    *frame = NULL;

    if ( (Buffer.size() == 54) && (Buffer.Compare((uint8 *)"M17 ", 4) == 0) )
    {
        // check frame type
        uint8 frameType = Buffer.data()[19];

        if ( frameType == M17_FRAMETYPE_VOICE )
        {
            // get stream ID
            uint16 streamId = MAKEWORD(Buffer.data()[5], Buffer.data()[4]);

            // get sequence number with EOT flag
            uint16 sequenceWithEOT = MAKEWORD(Buffer.data()[35], Buffer.data()[34]);

            // check EOT flag (bit 15)
            if ( sequenceWithEOT & 0x8000 )
            {
                uint16 sequence = sequenceWithEOT & 0x7FFF;

                // get codec2 payload
                uint8 codec2[16];
                ::memcpy(codec2, Buffer.data() + 36, 16);

                // create last frame packet (cast sequence to uint8 for D-Star packet ID)
                *frame = new CDvLastFramePacket(codec2, streamId, (uint8)(sequence & 0xFF));
                valid = true;
            }
        }
    }
    return valid;
}

////////////////////////////////////////////////////////////////////////////////////////
// packet encoding helpers

void CM17Protocol::EncodeConnectAckPacket(CBuffer *Buffer)
{
    uint8 tag[] = { 'A','C','K','N' };
    Buffer->Set(tag, sizeof(tag));
}

void CM17Protocol::EncodeConnectNackPacket(CBuffer *Buffer)
{
    uint8 tag[] = { 'N','A','C','K' };
    Buffer->Set(tag, sizeof(tag));
}

void CM17Protocol::EncodeDisconnectPacket(CBuffer *Buffer, char module)
{
    // DISC + encoded callsign (10 bytes total, like mrefd)
    uint8 tag[] = { 'D','I','S','C' };
    Buffer->Set(tag, sizeof(tag));

    // encode reflector callsign with module
    CCallsign cs = m_ReflectorCallsign;
    if ( module != ' ' && module != 0 )
    {
        cs.SetModule(module);
    }
    uint8 encoded[6];
    EncodeCallsign(cs, encoded);
    Buffer->Append(encoded, 6);
}

void CM17Protocol::EncodeDisconnectedPacket(CBuffer *Buffer)
{
    // simple 4-byte DISC acknowledgment (like mrefd)
    uint8 tag[] = { 'D','I','S','C' };
    Buffer->Set(tag, sizeof(tag));
}

void CM17Protocol::EncodePingPacket(CBuffer *Buffer)
{
    // PING + encoded callsign (10 bytes total, like mrefd)
    uint8 tag[] = { 'P','I','N','G' };
    Buffer->Set(tag, sizeof(tag));

    // encode reflector callsign
    uint8 encoded[6];
    EncodeCallsign(m_ReflectorCallsign, encoded);
    Buffer->Append(encoded, 6);
}


bool CM17Protocol::EncodeDvFramePacket(const CDvFramePacket &Packet, uint16 streamId, CBuffer *Buffer)
{
    // Get module ID to access cached data
    int iModId = g_Reflector.GetModuleIndex(Packet.GetModuleId());

    // Bounds check for module index
    if ( iModId < 0 || iModId >= NB_OF_MODULES )
    {
        return false;
    }

    // M17 uses 40ms packets with 2 x 8-byte Codec2 frames
    // XLXd internally uses 20ms frames, so we buffer every other frame
    // and only send when we have two frames

    // Check if we have a pending frame
    if ( !m_StreamsCache[iModId].m_bHasPendingCodec2 )
    {
        // No pending frame - buffer this one and don't send yet
        if ( Packet.HasCodec2Data() )
        {
            ::memcpy(m_StreamsCache[iModId].m_uiPendingCodec2, Packet.GetCodec2(), 8);
        }
        else
        {
            static const uint8 CODEC2_SILENCE[8] = { 0xC0, 0x00, 0x6A, 0x43, 0x9C, 0xE4, 0x21, 0x08 };
            ::memcpy(m_StreamsCache[iModId].m_uiPendingCodec2, CODEC2_SILENCE, 8);
        }
        m_StreamsCache[iModId].m_bHasPendingCodec2 = true;
        return true;  // buffered, nothing to send yet
    }

    // We have a pending frame - combine with this one and send
    uint8 tag[] = { 'M','1','7',' ' };

    // set magic
    Buffer->Set(tag, sizeof(tag));

    // stream ID (big-endian per M17 spec)
    Buffer->Append((uint8)HIBYTE(streamId));
    Buffer->Append((uint8)LOBYTE(streamId));

    // destination callsign (6 bytes) - use reflector callsign
    uint8 dest[6];
    EncodeCallsign(m_StreamsCache[iModId].m_dvHeader.GetUrCallsign(), dest);
    Buffer->Append(dest, 6);

    // source callsign (6 bytes) - use originator's callsign
    uint8 source[6];
    EncodeCallsign(m_StreamsCache[iModId].m_dvHeader.GetMyCallsign(), source);
    Buffer->Append(source, 6);

    // reserved (1 byte)
    Buffer->Append((uint8)0);

    // frame type
    Buffer->Append((uint8)M17_FRAMETYPE_VOICE);

    // reserved (14 bytes)
    for ( int i = 0; i < 14; i++ )
    {
        Buffer->Append((uint8)0);
    }

    // M17 frame sequence (big-endian, incremented for each 40ms packet sent)
    uint16 m17seq = m_StreamsCache[iModId].m_uiFrameSequence++;
    Buffer->Append((uint8)HIBYTE(m17seq & 0x7FFF));
    Buffer->Append((uint8)LOBYTE(m17seq & 0x7FFF));

    // codec2 payload (16 bytes = 2 x 8-byte Codec2 frames)
    // First 8 bytes: the pending (buffered) frame
    Buffer->Append(m_StreamsCache[iModId].m_uiPendingCodec2, 8);

    // Second 8 bytes: current frame
    if ( Packet.HasCodec2Data() )
    {
        Buffer->Append(Packet.GetCodec2(), 8);
    }
    else
    {
        for ( int i = 0; i < 8; i++ )
            Buffer->Append((uint8)0);
    }

    // reserved (2 bytes)
    Buffer->Append((uint8)0);
    Buffer->Append((uint8)0);

    // clear the pending flag
    m_StreamsCache[iModId].m_bHasPendingCodec2 = false;

    return true;
}

bool CM17Protocol::EncodeDvLastFramePacket(const CDvLastFramePacket &Packet, uint16 streamId, CBuffer *Buffer)
{
    // Get module ID to access cached header for callsigns
    int iModId = g_Reflector.GetModuleIndex(Packet.GetModuleId());

    // Bounds check for module index
    if ( iModId < 0 || iModId >= NB_OF_MODULES )
    {
        return false;
    }

    uint8 tag[] = { 'M','1','7',' ' };

    // set magic
    Buffer->Set(tag, sizeof(tag));

    // stream ID (big-endian per M17 spec)
    Buffer->Append((uint8)HIBYTE(streamId));
    Buffer->Append((uint8)LOBYTE(streamId));

    // destination callsign (6 bytes) - use reflector callsign
    uint8 dest[6];
    EncodeCallsign(m_StreamsCache[iModId].m_dvHeader.GetUrCallsign(), dest);
    Buffer->Append(dest, 6);

    // source callsign (6 bytes) - use originator's callsign
    uint8 source[6];
    EncodeCallsign(m_StreamsCache[iModId].m_dvHeader.GetMyCallsign(), source);
    Buffer->Append(source, 6);

    // reserved (1 byte)
    Buffer->Append((uint8)0);

    // frame type
    Buffer->Append((uint8)M17_FRAMETYPE_VOICE);

    // reserved (14 bytes)
    for ( int i = 0; i < 14; i++ )
    {
        Buffer->Append((uint8)0);
    }

    // M17 frame sequence with EOT flag (big-endian, bit 15 set)
    uint16 m17seq = m_StreamsCache[iModId].m_uiFrameSequence;
    uint16 seqWithEOT = (m17seq & 0x7FFF) | 0x8000;
    Buffer->Append((uint8)HIBYTE(seqWithEOT));
    Buffer->Append((uint8)LOBYTE(seqWithEOT));

    // Codec2 3200bps silence (encoded from 160 zero PCM samples)
    static const uint8 CODEC2_SILENCE[8] = { 0xC0, 0x00, 0x6A, 0x43, 0x9C, 0xE4, 0x21, 0x08 };

    // codec2 payload (16 bytes = 2 x 8-byte Codec2 frames)
    // For the last frame, send pending frame (if any) + current frame
    if ( m_StreamsCache[iModId].m_bHasPendingCodec2 )
    {
        // First 8 bytes: the pending (buffered) frame
        Buffer->Append(m_StreamsCache[iModId].m_uiPendingCodec2, 8);
    }
    else
    {
        // No pending frame - use proper Codec2 silence
        Buffer->Append(CODEC2_SILENCE, 8);
    }

    // Second 8 bytes: current frame
    if ( Packet.HasCodec2Data() )
    {
        Buffer->Append(Packet.GetCodec2(), 8);
    }
    else
    {
        // No Codec2 data (transcoded stream) - use proper silence
        Buffer->Append(CODEC2_SILENCE, 8);
    }

    // reserved (2 bytes)
    Buffer->Append((uint8)0);
    Buffer->Append((uint8)0);

    // Reset the cache for next stream
    m_StreamsCache[iModId].m_bHasPendingCodec2 = false;
    m_StreamsCache[iModId].m_uiFrameSequence = 0;

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
// M17 callsign encoding/decoding helpers

void CM17Protocol::EncodeCallsign(const CCallsign &Callsign, uint8 *encoded) const
{
    char cs[10];
    ::memset(cs, 0, sizeof(cs));

    // get callsign string (without module)
    Callsign.GetCallsignString(cs);
    int len = MIN(9, (int)::strlen(cs));

    // pad with spaces
    for ( int i = len; i < 9; i++ )
    {
        cs[i] = ' ';
    }

    // convert to base-40
    uint64 val = CallsignToBase40(cs);

    // encode to 6 bytes (48 bits)
    encoded[0] = (val >> 40) & 0xFF;
    encoded[1] = (val >> 32) & 0xFF;
    encoded[2] = (val >> 24) & 0xFF;
    encoded[3] = (val >> 16) & 0xFF;
    encoded[4] = (val >> 8) & 0xFF;
    encoded[5] = val & 0xFF;
}

bool CM17Protocol::DecodeCallsign(const uint8 *encoded, CCallsign *callsign) const
{
    // decode from 6 bytes to 48-bit value
    uint64 val = 0;
    val |= ((uint64)encoded[0]) << 40;
    val |= ((uint64)encoded[1]) << 32;
    val |= ((uint64)encoded[2]) << 24;
    val |= ((uint64)encoded[3]) << 16;
    val |= ((uint64)encoded[4]) << 8;
    val |= ((uint64)encoded[5]);

    // Check for special M17 addresses
    if ( val == 0xFFFFFFFFFFFFULL )
    {
        // Broadcast address (@ALL)
        callsign->SetCallsign("ALL");
        return true;  // Valid special address
    }
    else if ( val == 0 )
    {
        // Empty/unset address
        callsign->SetCallsign("");
        return false;
    }

    // convert from base-40
    char cs[10];
    Base40ToCallsign(val, cs);

    // trim trailing spaces
    int len = 9;
    while ( len > 0 && cs[len-1] == ' ' )
    {
        cs[len-1] = 0;
        len--;
    }
    cs[9] = 0;

    // create callsign
    callsign->SetCallsign(cs);

    return callsign->IsValid();
}

uint64 CM17Protocol::CallsignToBase40(const char *cs) const
{
    uint64 val = 0;
    const char *alphabet = M17_CALLSIGN_ALPHABET;

    // M17 encoding processes callsign from RIGHT to LEFT (index 8 down to 0)
    // This matches mrefd's CSIn() implementation
    int len = (int)::strlen(cs);
    if ( len > 9 ) len = 9;

    for ( int i = len - 1; i >= 0; i-- )
    {
        char c = cs[i];
        int idx = 0;

        // find character in alphabet
        for ( int j = 0; j < 40; j++ )
        {
            if ( alphabet[j] == c )
            {
                idx = j;
                break;
            }
        }

        val = val * 40 + idx;
    }

    return val;
}

void CM17Protocol::Base40ToCallsign(uint64 val, char *cs) const
{
    const char *alphabet = M17_CALLSIGN_ALPHABET;

    // M17 decoding fills callsign from LEFT to RIGHT (index 0 to 8)
    // This matches mrefd's CodeIn() implementation
    ::memset(cs, 0, 10);
    int i = 0;
    while ( val && i < 9 )
    {
        cs[i++] = alphabet[val % 40];
        val /= 40;
    }
}

bool CM17Protocol::IsValidM17Destination(const CCallsign &Callsign) const
{
    // Check if valid amateur callsign
    if ( Callsign.IsValid() )
    {
        return true;
    }

    // Get the callsign string
    char cs[16];
    Callsign.GetCallsignString(cs);

    // Check for broadcast address
    if ( ::strcmp(cs, "ALL") == 0 )
    {
        return true;
    }

    // Check for M17 reflector address: M17-XXX where X is alphanumeric
    if ( ::strlen(cs) == 7 &&
         cs[0] == 'M' && cs[1] == '1' && cs[2] == '7' && cs[3] == '-' )
    {
        // Check that the last 3 characters are alphanumeric
        bool valid = true;
        for ( int i = 4; i < 7; i++ )
        {
            if ( !((cs[i] >= 'A' && cs[i] <= 'Z') ||
                   (cs[i] >= '0' && cs[i] <= '9')) )
            {
                valid = false;
                break;
            }
        }
        if ( valid )
        {
            return true;
        }
    }

    return false;
}
