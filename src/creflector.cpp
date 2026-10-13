//
//  creflector.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 31/10/2015.
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
#include "creflector.h"
#include "cgatekeeper.h"
#include "cdmriddirfile.h"
#include "cdmriddirhttp.h"
#include "ctranscoder.h"
#include "cysfnodedirfile.h"
#include "cysfnodedirhttp.h"
#include "cnxdniddirfile.h"
#include "cnxdniddirhttp.h"

////////////////////////////////////////////////////////////////////////////////////////
// constructor

CReflector::CReflector()
{
    m_bStopThreads = false;
    m_XmlReportThread = NULL;
    m_JsonReportThread = NULL;
    for ( int i = 0; i < NB_OF_MODULES; i++ )
    {
        m_RouterThreads[i] = NULL;
    }
    ::memset(m_Mac, 0, sizeof(m_Mac));
#ifdef DEBUG_DUMPFILE
    m_DebugFile.open("/Users/jean-luc/Desktop/xlxdebug.txt");
#endif
}

CReflector::CReflector(const CCallsign &callsign)
{
#ifdef DEBUG_DUMPFILE
    m_DebugFile.close();
#endif
    m_bStopThreads = false;
    m_XmlReportThread = NULL;
    m_JsonReportThread = NULL;
    for ( int i = 0; i < NB_OF_MODULES; i++ )
    {
        m_RouterThreads[i] = NULL;
    }
    m_Callsign = callsign;
    ::memset(m_Mac, 0, sizeof(m_Mac));
}

////////////////////////////////////////////////////////////////////////////////////////
// destructor

CReflector::~CReflector()
{
    m_bStopThreads = true;
    if ( m_XmlReportThread != NULL )
    {
        m_XmlReportThread->join();
        delete m_XmlReportThread;
    }
    if ( m_JsonReportThread != NULL )
    {
        m_JsonReportThread->join();
        delete m_JsonReportThread;
    }
    for ( int i = 0; i < NB_OF_MODULES; i++ )
    {
        if ( m_RouterThreads[i] != NULL )
        {
             m_RouterThreads[i]->join();
             delete m_RouterThreads[i];
        }
    }
}


////////////////////////////////////////////////////////////////////////////////////////
// operation

bool CReflector::Start(void)
{
    bool ok = true;

    // reset stop flag
    m_bStopThreads = false;
    
    // init gate keeper
    ok &= g_GateKeeper.Init();
    
    // init dmrid directory
    g_DmridDir.Init();
    
    // init wiresx node directory
    g_YsfNodeDir.Init();

    // init nxdn id directory
    g_NxdnIdDir.Init();

    // init the transcoder
    g_Transcoder.Init();
    
    // create protocols
    ok &= m_Protocols.Init();
    
    // if ok, start threads
    if ( ok )
    {
        // start one thread per reflector module
        for ( int i = 0; i < NB_OF_MODULES; i++ )
        {
            m_RouterThreads[i] = new std::thread(CReflector::RouterThread, this, &(m_Streams[i]));
        }

        // start the reporting threads
        m_XmlReportThread = new std::thread(CReflector::XmlReportThread, this);
#ifdef JSON_MONITOR
        m_JsonReportThread = new std::thread(CReflector::JsonReportThread, this);
#endif
    }
    else
    {
        m_Protocols.Close();
    }
    
    // done
    return ok;
}

void CReflector::Stop(void)
{
    // stop & delete all threads
    m_bStopThreads = true;

    // stop & delete report threads
    if ( m_XmlReportThread != NULL )
    {
        m_XmlReportThread->join();
        delete m_XmlReportThread;
        m_XmlReportThread = NULL;
    }
    if ( m_JsonReportThread != NULL )
    {
        m_JsonReportThread->join();
        delete m_JsonReportThread;
        m_JsonReportThread = NULL;
    }

    // stop & delete all router thread
    for ( int i = 0; i < NB_OF_MODULES; i++ )
    {
        if ( m_RouterThreads[i] != NULL )
        {
            m_RouterThreads[i]->join();
            delete m_RouterThreads[i];
            m_RouterThreads[i] = NULL;
        }
    }

    // close protocols
    m_Protocols.Close();

    // close transcoder
    g_Transcoder.Close();
    
    // close gatekeeper
    g_GateKeeper.Close();
    
    // close databases
    g_DmridDir.Close();
    g_YsfNodeDir.Close();

}

////////////////////////////////////////////////////////////////////////////////////////
// stream opening & closing

CPacketStream *CReflector::OpenStream(CDvHeaderPacket *DvHeader, CClient *client)
{
    CPacketStream *retStream = NULL;

    // Caller MUST hold the Clients lock. This function may TEMPORARILY
    // release it during the transcoder handshake (see Phase 2 below). On
    // return the Clients lock is always re-acquired, but the `client`
    // pointer passed in may have been freed if the client's keepalive
    // expired mid-handshake — DO NOT dereference it after this returns.
    // Existing callers only use `client` on the failure path (via
    // TryLateEntry), which runs when we return NULL BEFORE the release/
    // re-acquire cycle, so they are safe.

    // check sid is not NULL
    if ( DvHeader->GetStreamId() == 0 )
    {
        return NULL;
    }

    // check if client is valid candidate
    if ( !m_Clients.IsClient(client) || client->IsAMaster() )
    {
        return NULL;
    }

    // check if no stream with same streamid already open (prevent loops)
    if ( IsStreamOpen(DvHeader) )
    {
        std::cout << "Detected stream loop on module " << DvHeader->GetRpt2Module()
                  << " for client " << client->GetCallsign()
                  << " with sid " << DvHeader->GetStreamId() << std::endl;
        return NULL;
    }

    // get the module's queue
    char module = DvHeader->GetRpt2Module();
    CPacketStream *stream = GetStream(module);
    if ( stream == NULL )
    {
        return NULL;
    }

    // ----- Phase 1: reserve the stream (stream lock + Clients lock held) -----
    uint8 inputCodec = CODEC_NONE;
    uint32 transcodeSourceCodecs = 0U;
    uint32 neededCodecs = 0U;
    bool opened = false;
    stream->Lock();
    if ( stream->Open(*DvHeader, client) )
    {
        // mark client as master so it can't be deleted
        client->SetMasterOfModule(module);
        client->Heard();
        retStream = stream;

        // The header is deferred — it stays in m_DvHeader with
        // m_bHasPendingHeader set, and AttachCodecStream() below emits
        // a fresh copy to the base queue just before draining the
        // pre-codec buffer. Keeps header → first-voice timing tight on
        // the wire (~<50ms gap) instead of the ~1-1.3s gap the old
        // immediate-push produced while AMBEd negotiated a channel.

        inputCodec = stream->GetInputCodec();
        // Capture the source's full codec set (single-codec for native
        // protocols, multi-codec for XLX interlink peers) and what
        // clients on this module need. Both reads need the Clients lock,
        // which the caller holds across this whole Phase 1 block, so we
        // do them here before releasing for Phase 2.
        transcodeSourceCodecs = client->GetAvailableCodecs();
        neededCodecs          = GetNeededCodecsOnModule(module);
        opened = true;

        if ( client->IsPeer() )
        {
            std::cout << "Opening stream on module " << module << " for "
                      << DvHeader->GetMyCallsign() << " via " << client->GetCallsign()
                      << " with sid " << DvHeader->GetStreamId() << std::endl;
        }
        else
        {
            std::cout << "Opening stream on module " << module << " for "
                      << DvHeader->GetMyCallsign() << " on " << client->GetCallsign()
                      << " with sid " << DvHeader->GetStreamId() << std::endl;
        }
        OnStreamOpen(stream->GetUserCallsign());
    }
    stream->Unlock();

    if ( !opened )
    {
        return NULL;
    }

    // Header ownership: on success Open() copied the header into m_DvHeader
    // by value. The caller's pointer is no longer needed here — previously
    // stream->Push(DvHeader) transferred it into the queue and the queue
    // would delete it on drain. Since we no longer push at Phase 1, we
    // must delete it ourselves to keep the caller-visible contract
    // unchanged ("on success, OpenStream consumes the pointer").
    delete DvHeader;
    DvHeader = NULL;

    // ----- Phase 2: negotiate transcoder WITHOUT holding reflector locks -----
    // g_Transcoder.GetStream() blocks up to ~1050ms waiting for AMBEd. With
    // Clients released, other protocol threads can continue processing voice
    // frames, keepalives, and new connects in parallel.
    //
    // Voice frames arriving for THIS stream during the negotiation are
    // buffered in the stream's pre-codec ring buffer (by CPacketStream::Push
    // under the stream lock) and replayed through the transcoder in Phase 3.
    ReleaseClients();

    // Decide whether a transcode is required. We engage AMBEd iff a
    // codec needed by some client on the module is NOT already
    // provided by the source's wire frame. Single-codec sources
    // (D-Star/DMR/YSF/NXDN/M17/P25 native) almost always trigger the
    // transcode because the always-on AMBE+/AMBE+2 baseline can't
    // both be satisfied by a single native codec. XLX peers may or
    // may not trigger depending on which codecs the rev carries vs
    // which clients are connected to the module:
    //   - rev 3 + only D-Star/DMR/YSF/NXDN: no transcode (AMBE+ and
    //     AMBE+2 both in wire frame; no M17/P25 demand)
    //   - rev 3 + M17 client: no transcode (Codec2 in wire frame)
    //   - rev 3 + P25 client: transcode (IMBE missing — this is the
    //     P25-over-XLX bug this design fixes)
    //   - rev 1/2 + M17 or P25 client: transcode (whichever optional
    //     codec is missing)
    //
    // Source-codec preference when multiple are available:
    //   AMBE+2 > AMBE+ > Codec2 > IMBE
    // (most-common cross-mode pairing first; IMBE only as input if
    // it's literally the only thing available, which would only
    // happen for native P25 sources).
    uint8 transcodeInputCodec = CODEC_NONE;
    if ( transcodeSourceCodecs != 0U )
    {
        const uint32 missing = neededCodecs & ~transcodeSourceCodecs;
        if ( missing != 0U )
        {
            // Pick the preferred source codec from what's available.
            if      ( transcodeSourceCodecs & (1U << CODEC_AMBE2PLUS) ) transcodeInputCodec = CODEC_AMBE2PLUS;
            else if ( transcodeSourceCodecs & (1U << CODEC_AMBEPLUS)  ) transcodeInputCodec = CODEC_AMBEPLUS;
            else if ( transcodeSourceCodecs & (1U << CODEC_CODEC2)    ) transcodeInputCodec = CODEC_CODEC2;
            else if ( transcodeSourceCodecs & (1U << CODEC_IMBE)      ) transcodeInputCodec = CODEC_IMBE;
        }
    }
    // Backward-compat fallback: if the inputCodec from the stream is
    // non-NONE (native single-codec source) but our new logic didn't
    // request a transcode (shouldn't happen given the always-on
    // AMBE+/AMBE+2 baseline, but defence-in-depth), keep the prior
    // behaviour of engaging the transcoder with the native codec.
    // Avoids any chance of regression for paths that worked before.
    if ( transcodeInputCodec == CODEC_NONE && inputCodec != CODEC_NONE )
    {
        transcodeInputCodec = inputCodec;
    }

    CCodecStream *codecStream = NULL;
    if ( transcodeInputCodec != CODEC_NONE )
    {
        codecStream = g_Transcoder.GetStream(stream, transcodeInputCodec);
    }

    // ----- Phase 3: attach the codec stream (stream lock only) -----
    // AttachCodecStream handles the case where CloseStream fired during
    // Phase 2 (it releases the codec stream and drops buffered frames).
    stream->Lock();
    stream->AttachCodecStream(codecStream);
    stream->Unlock();

    // Re-acquire Clients for the caller. The `client` parameter may now be
    // stale (see contract comment at top); the caller must not dereference
    // it. The reflector's ReleaseStreamOwner path handles stale owner
    // pointers on the stream itself.
    GetClients();

    return retStream;
}

bool CReflector::CloseStream(CPacketStream *stream)
{
    //
    if ( stream == NULL )
    {
        return false;
    }

    // Serialise CloseStream on this stream. The old design relied on
    // a single-caller invariant for the drain-loop's lock-free read of
    // m_CodecStream, but that invariant can break when e.g. the
    // RouterThread's orphan-close path races with a protocol's
    // OnDvLastFramePacketIn close path on the same stream. If another
    // thread is already closing this stream, bail out — there is
    // nothing useful for us to do and proceeding would race on
    // m_CodecStream. Return false so the caller knows it was a no-op
    // (the RouterThread relies on this to suppress a log-spam loop
    // while the other closer is still draining).
    if ( !stream->TryBeginClose() )
    {
        return false;
    }

    // Wait for the transcoder pipeline to drain before closing.
    // Call IsEmpty() WITHOUT holding the stream lock to avoid ABBA deadlock:
    // the drain loop would hold stream lock then acquire codec lock (via
    // CCodecStream::IsEmpty), while CCodecStream::Task() holds codec lock
    // then acquires stream lock (for jitter buffer release).
    //
    // After the ambed-decoupling rework (jitter buffer releases packets
    // on a fixed 20 ms cadence regardless of ambed state), the drain
    // proceeds at one packet per JITTER_BUFFER_FRAME_MS once the source
    // stops pushing — so worst-case wall-clock is roughly the jitter
    // buffer occupancy × 20 ms. Steady-state occupancy is ~8 frames
    // (~160 ms); a burst-at-close pathology can leave up to ~50 frames
    // (~1000 ms). The 2000 ms cap is the safety bound for the
    // pathological case; it's not normally reached.
    //
    // The stall-detection logic that used to live here was specifically
    // for the pre-rework architecture where ambed could wedge the
    // pipeline indefinitely (packets piled in m_LocalQueue, never
    // released). With ambed now structurally optional in the audio
    // path, "stall" is no longer a possible state — the jitter timer
    // always advances. So the drain reduces to: "wait until empty, or
    // until the absolute cap as a hard safety bound."
    static const int MAX_DRAIN_WAIT_MS = 2000;
    static const int DRAIN_POLL_MS = 10;

    int  waitedMs = 0;
    bool bEmpty = false;

    while ( waitedMs < MAX_DRAIN_WAIT_MS )
    {
        bEmpty = stream->IsEmpty();
        if ( bEmpty ) break;

        CTimePoint::TaskSleepFor(DRAIN_POLL_MS);
        waitedMs += DRAIN_POLL_MS;
    }

    if ( !bEmpty )
    {
        // Reached the absolute cap. The pathological case where this
        // can fire today is a Phase-2 OpenStream burst that leaves >100
        // frames queued, which exceeds the cap's release budget. Log
        // for diagnosis; the destructor will drop the remaining
        // packets and the per-CCodecStream warning line will quantify
        // the loss.
        std::cout << "Warning: CloseStream drain hit " << MAX_DRAIN_WAIT_MS
                  << "ms cap, some packets may be lost" << std::endl;
    }

    // Snapshot what we need under the stream + clients locks, then
    // release both before calling into the GateKeeper. This keeps the
    // GateKeeper LoopMutex completely outside the Clients lock scope
    // (eliminates a Clients → LoopMutex ordering that other paths must
    // avoid forever) without sacrificing correctness: the snapshot holds
    // everything ReportStreamClose needs, and the client pointer is
    // nulled under the stream lock so no one can dereference it after.
    GetClients();
    stream->Lock();

    CClient *client = stream->GetOwnerClient();
    CClient *clientToDisconnect = NULL;
    bool        hadClient = false;
    char        module = ' ';
    CCallsign   userCallsign;
    uint32_t    packetCount = 0;
    bool        clientIsPeer = false;
    if ( client != NULL )
    {
        hadClient = true;
        module = GetStreamModule(stream);
        userCallsign = stream->GetUserCallsign();
        packetCount = stream->GetPacketCount();
        clientIsPeer = client->IsPeer();

        // client no longer a master
        client->NotAMaster();

        // Capture the pointer for potential loop-disconnect below, then
        // NULL it under the stream lock so no other thread can
        // dereference it after we release the clients lock.
        // GetOwnerIp() uses the cached copy set at Open() time.
        if ( !clientIsPeer )
        {
            clientToDisconnect = client;
        }
        stream->SetOwnerClient(NULL);
    }

    // release clients
    ReleaseClients();

    // unlock before closing
    // to avoid double lock in associated
    // codecstream close/thread-join
    stream->Unlock();

    // Call GateKeeper and post notifications without any reflector
    // locks held. ReportStreamClose takes the GateKeeper LoopMutex;
    // keeping that outside the Clients lock means any future path that
    // needs Clients while holding LoopMutex will not deadlock here.
    bool shouldDisconnect = false;
    if ( hadClient )
    {
        shouldDisconnect = g_GateKeeper.ReportStreamClose(
            module,
            userCallsign,
            packetCount,
            clientIsPeer
        );

        g_Reflector.OnStreamClose(userCallsign);

        std::cout << "Closing stream of module " << module << std::endl;
    }

    // close stream — resets state and releases transcoder stream
    stream->Close();

    // now safe to disconnect the looping client
    if ( shouldDisconnect && clientToDisconnect != NULL )
    {
        GetClients();
        if ( m_Clients.IsClient(clientToDisconnect) )
        {
            std::cout << "Loop detection: removing client " << clientToDisconnect->GetCallsign() << std::endl;
            m_Clients.RemoveClient(clientToDisconnect);
        }
        ReleaseClients();
    }

    // check for pending late entry on this module
    int moduleIdx = -1;
    for ( int i = 0; i < (int)m_Streams.size(); i++ )
    {
        if ( &m_Streams[i] == stream )
        {
            moduleIdx = i;
            break;
        }
    }
    if ( moduleIdx >= 0 )
    {
        PromotePendingEntry(moduleIdx);
    }

    // release the close serialization — the next CloseStream on this
    // stream is now permitted to run (after the next Open() cycle, the
    // stream is no longer "closing").
    stream->EndClose();

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
// late entry support

bool CReflector::TryLateEntry(CDvHeaderPacket *DvHeader, CClient *client)
{
    // clients MUST be locked by caller
    char module = DvHeader->GetRpt2Module();
    int idx = GetModuleIndex(module);
    if ( idx < 0 )
        return false;

    std::lock_guard<std::mutex> lock(m_PendingMutex[idx]);

    // only one pending entry per module
    if ( m_PendingEntries[idx].IsActive() )
    {
        return false;
    }

    // stash it
    m_PendingEntries[idx].Set(DvHeader, client);

    std::cout << "Late entry: stashed pending on module " << module
              << " for " << client->GetCallsign()
              << " with sid " << DvHeader->GetStreamId() << std::endl;

    return true;
}

bool CReflector::BufferPendingFrame(uint16 streamId, CPacket *frame)
{
    for ( int i = 0; i < NB_OF_MODULES; i++ )
    {
        std::lock_guard<std::mutex> lock(m_PendingMutex[i]);
        CPendingEntry &pending = m_PendingEntries[i];

        if ( !pending.IsActive() || pending.GetStreamId() != streamId )
            continue;

        // check expiry
        if ( pending.IsExpired() )
        {
            std::cout << "Late entry: pending expired on module " << GetModuleLetter(i) << std::endl;
            pending.Clear();
            return false;
        }

        // if promoted, forward directly to stream
        if ( pending.IsPromoted() )
        {
            CPacketStream *stream = pending.GetPromotedStream();
            stream->Lock();
            stream->Push(frame);
            stream->Unlock();
            return true;
        }

        // if buffering, try to buffer
        if ( pending.IsBuffering() )
        {
            if ( pending.BufferFrame(frame) )
            {
                return true;
            }
            // buffer full — BufferFrame already transitioned to HEADER_ONLY
            std::cout << "Late entry: buffer full on module " << GetModuleLetter(i) << ", switching to pure late entry" << std::endl;
            return false;
        }

        // HEADER_ONLY state — just drop the frame
        return false;
    }

    return false;
}

CPacketStream *CReflector::GetPromotedStream(uint16 streamId)
{
    for ( int i = 0; i < NB_OF_MODULES; i++ )
    {
        std::lock_guard<std::mutex> lock(m_PendingMutex[i]);
        CPendingEntry &pending = m_PendingEntries[i];

        if ( pending.IsPromoted() && pending.GetStreamId() == streamId )
        {
            return pending.GetPromotedStream();
        }
    }
    return NULL;
}

void CReflector::CancelPendingEntry(uint16 streamId)
{
    for ( int i = 0; i < NB_OF_MODULES; i++ )
    {
        std::lock_guard<std::mutex> lock(m_PendingMutex[i]);
        CPendingEntry &pending = m_PendingEntries[i];

        if ( pending.IsActive() && pending.GetStreamId() == streamId )
        {
            std::cout << "Late entry: cancelled pending on module " << GetModuleLetter(i) << std::endl;
            pending.Clear();
            return;
        }
    }
}

void CReflector::PromotePendingEntry(int moduleIdx)
{
    // Lock order: Clients THEN PendingMutex
    // TryLateEntry acquires Clients (held by caller) then PendingMutex,
    // so we must use the same order here to prevent ABBA deadlock.
    GetClients();
    std::lock_guard<std::mutex> lock(m_PendingMutex[moduleIdx]);
    CPendingEntry &pending = m_PendingEntries[moduleIdx];

    if ( !pending.IsActive() )
    {
        ReleaseClients();
        return;
    }

    if ( pending.IsExpired() )
    {
        std::cout << "Late entry: pending expired on module " << GetModuleLetter(moduleIdx) << std::endl;
        pending.Clear();
        ReleaseClients();
        return;
    }

    // re-validate client
    CClient *client = pending.GetClient();

    if ( !m_Clients.IsClient(client) || client->IsAMaster() )
    {
        std::cout << "Late entry: client no longer valid on module " << GetModuleLetter(moduleIdx) << std::endl;
        pending.Clear();
        ReleaseClients();
        return;
    }

    // try to open the stream
    CPacketStream *stream = &m_Streams[moduleIdx];
    stream->Lock();
    if ( stream->Open(*(pending.GetHeader()), client) )
    {
        // mark client as master
        client->SetMasterOfModule(GetModuleLetter(moduleIdx));
        client->Heard();

        // The header is emitted by AttachCodecStream() below via the new
        // m_bHasPendingHeader mechanism (set inside Open()). We previously
        // did an explicit stream->Push(new CDvHeaderPacket(...)) here, but
        // that produces a duplicate header on the base queue now that
        // AttachCodecStream also emits one — double-open on D-Star receivers,
        // wasted bandwidth on others. Rely on AttachCodecStream for the
        // single authoritative emission.

        // Complete the two-phase open. Late-entry promotion runs under the
        // Clients lock and cannot afford a blocking AMBEd handshake here, so
        // we attach a NULL codec stream with transcodeRequested=false to
        // suppress the spurious "ambed unavailable" warning. AttachCodecStream
        // emits the deferred header, clears m_bCodecPending, and routes any
        // buffered pre-codec frames straight to the stream queue. Cross-mode
        // transcoding is unavailable for late-entry streams — acceptable
        // because the promotion itself already implies we lost the start of
        // the transmission.
        stream->AttachCodecStream(NULL, /*transcodeRequested=*/false);

        stream->Unlock();

        std::cout << "Late entry: promoted on module " << GetModuleLetter(moduleIdx)
                  << " for " << client->GetCallsign() << std::endl;

        // notify
        OnStreamOpen(stream->GetUserCallsign());

        // replay buffered frames
        pending.ReplayInto(stream);

        // transition to promoted state for ongoing frame forwarding
        pending.Promote(stream);

        ReleaseClients();
    }
    else
    {
        // shouldn't happen — we just closed the stream
        stream->Unlock();
        std::cout << "Late entry: failed to open stream on module " << GetModuleLetter(moduleIdx) << std::endl;
        pending.Clear();
        ReleaseClients();
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// router threads

void CReflector::RouterThread(CReflector *This, CPacketStream *streamIn)
{
    // get our module
    uint8 uiModuleId = This->GetStreamModule(streamIn);

    // get on input queue
    CPacket *packet;

    while ( !This->m_bStopThreads )
    {
        // any packet in our input queue ?
        streamIn->Lock();
        if ( !streamIn->empty() )
        {
            // get the packet
            packet = streamIn->front();
            streamIn->pop();
        }
        else
        {
            packet = NULL;
        }
        streamIn->Unlock();

        // route it
        if ( packet != NULL )
        {
            // set origin
            packet->SetModuleId(uiModuleId);

            // iterate on all protocols
            for ( int i = 0; i < This->m_Protocols.Size(); i++ )
            {
                // duplicate packet
                CPacket *packetClone = packet->Duplicate();

                // get protocol
                CProtocol *protocol = This->m_Protocols.GetProtocol(i);

                // if packet is header, update RPT2 according to protocol
                if ( packetClone->IsDvHeader() )
                {
                    // get our callsign
                    CCallsign csRPT = protocol->GetReflectorCallsign();
                    csRPT.SetModule(This->GetStreamModule(streamIn));
                    ((CDvHeaderPacket *)packetClone)->SetRpt2Callsign(csRPT);
                }

                // and push it
                CPacketQueue *queue = protocol->GetQueue();
                queue->push(packetClone);
                protocol->ReleaseQueue();
            }

            // done
            delete packet;
            packet = NULL;
        }
        else
        {
            // Safety net: if stream is open but expired (no packets for
            // STREAM_TIMEOUT), close it. This catches promoted late-entry
            // streams that aren't tracked in any protocol's m_Streams.
            //
            // CloseStream may be a no-op if another thread (e.g. the
            // protocol's CheckStreamsTimeout path) already grabbed the
            // close serialisation via TryBeginClose. In that case the
            // other thread is still draining the stream, so it stays
            // IsOpen() && IsExpired() until that drain completes — if
            // we logged unconditionally and skipped the sleep we'd burn
            // a CPU spinning on the same orphan. Only log when we
            // actually performed the close, and always sleep so the
            // loop can't spin regardless of outcome.
            streamIn->Lock();
            bool orphaned = streamIn->IsOpen() && streamIn->IsExpired();
            streamIn->Unlock();
            if ( orphaned )
            {
                if ( This->CloseStream(streamIn) )
                {
                    std::cout << "Router: closed expired stream on module " << (char)uiModuleId << std::endl;
                }
            }
            // wait a bit (always — even after a close, to keep this
            // loop from busy-waiting on a stream another thread is
            // draining)
            CTimePoint::TaskSleepFor(10);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// report threads

void CReflector::XmlReportThread(CReflector *This)
{
    while ( !This->m_bStopThreads )
    {
        // report to xml file
        std::ofstream xmlFile;
        xmlFile.open(XML_PATH, std::ios::out | std::ios::trunc);
        if ( xmlFile.is_open() )
        {
            // write xml file
            This->WriteXmlFile(xmlFile);

            // and close file
            xmlFile.close();
        }
#ifndef DEBUG_NO_ERROR_ON_XML_OPEN_FAIL
        else
        {
            std::cout << "Failed to open " << XML_PATH  << std::endl;
        }
#endif

        // and wait a bit
        CTimePoint::TaskSleepFor(XML_UPDATE_PERIOD * 1000);
    }
}

void CReflector::JsonReportThread(CReflector *This)
{
    CUdpSocket Socket;
    CBuffer    Buffer;
    CIp        Ip;
    bool       bOn;

    // init variable
    bOn = false;

    // create listening socket
    if ( Socket.Open(JSON_PORT) )
    {
        // and loop
        while ( !This->m_bStopThreads )
        {
            // any command ?
            if ( Socket.Receive(&Buffer, &Ip, 50) != -1 )
            {
                // check verb
                if ( Buffer.Compare((uint8 *)"hello", 5) == 0 )
                {
                    std::cout << "Monitor socket connected with " << Ip << std::endl;

                    // connected
                    bOn = true;

                    // announce ourselves
                    This->SendJsonReflectorObject(Socket, Ip);

					// dump tables
					This->SendJsonNodesObject(Socket, Ip);
					This->SendJsonStationsObject(Socket, Ip);
                }
                else if ( Buffer.Compare((uint8 *)"bye", 3) == 0 )
                {
                    std::cout << "Monitor socket disconnected" << std::endl;

                    // diconnected
                    bOn = false;
                }
            }

            // any notifications ?
            CNotification notification;
            This->m_Notifications.Lock();
            if ( !This->m_Notifications.empty() )
            {
                // get the packet
                notification = This->m_Notifications.front();
                This->m_Notifications.pop();
            }
            This->m_Notifications.Unlock();

            // handle it
            if ( bOn )
            {
                switch ( notification.GetId() )
                {
                    case NOTIFICATION_CLIENTS:
                    case NOTIFICATION_PEERS:
                        //std::cout << "Monitor updating nodes table" << std::endl;
                        This->SendJsonNodesObject(Socket, Ip);
                        break;
                    case NOTIFICATION_USERS:
                        //std::cout << "Monitor updating stations table" << std::endl;
                        This->SendJsonStationsObject(Socket, Ip);
                        break;
                    case NOTIFICATION_STREAM_OPEN:
                        //std::cout << "Monitor notify station " << notification.GetCallsign() << "going ON air" << std::endl;
                        This->SendJsonStationsObject(Socket, Ip);
                        This->SendJsonOnairObject(Socket, Ip, notification.GetCallsign());
                        break;
                    case NOTIFICATION_STREAM_CLOSE:
                        //std::cout << "Monitor notify station " << notification.GetCallsign() << "going OFF air" << std::endl;
                        This->SendJsonOffairObject(Socket, Ip, notification.GetCallsign());
                        break;
                   case NOTIFICATION_NONE:
                    default:
                        // nothing to do, just sleep a bit
                        CTimePoint::TaskSleepFor(250);
                        break;
                }
            }
        }
    }
    else
    {
        std::cout << "Error creating monitor socket" << std::endl;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// notifications

void CReflector::OnPeersChanged(void)
{
    CNotification notification(NOTIFICATION_PEERS);
    
    m_Notifications.Lock();
    m_Notifications.push(notification);
    m_Notifications.Unlock();
}

void CReflector::OnClientsChanged(void)
{
    CNotification notification(NOTIFICATION_CLIENTS);

    m_Notifications.Lock();
    m_Notifications.push(notification);
    m_Notifications.Unlock();
}

void CReflector::OnUsersChanged(void)
{
    CNotification notification(NOTIFICATION_USERS);

    m_Notifications.Lock();
    m_Notifications.push(notification);
    m_Notifications.Unlock();
}

void CReflector::OnStreamOpen(const CCallsign &callsign)
{
    CNotification notification(NOTIFICATION_STREAM_OPEN, callsign);

    m_Notifications.Lock();
    m_Notifications.push(notification);
    m_Notifications.Unlock();
}

void CReflector::OnStreamClose(const CCallsign &callsign)
{
    CNotification notification(NOTIFICATION_STREAM_CLOSE, callsign);

    m_Notifications.Lock();
    m_Notifications.push(notification);
    m_Notifications.Unlock();
}

////////////////////////////////////////////////////////////////////////////////////////
// modules & queues

int CReflector::GetModuleIndex(char module) const
{
    int i = (int)module - (int)'A';
    if ( (i < 0) || (i >= NB_OF_MODULES) )
    {
        i = -1;
    }
    return i;
}

CPacketStream *CReflector::GetStream(char module)
{
    CPacketStream *stream = NULL;
    int i = GetModuleIndex(module);
    if ( i >= 0 )
    {
        stream = &(m_Streams[i]);
    }
    return stream;
}

// Walk the (already-locked) Clients list and assemble the set of
// codecs that any client on this module would consume. AMBE+ and
// AMBE+2 are unconditionally included — they are operator-mandated
// "core" codecs (D-Star and DMR/YSF/NXDN families respectively),
// and OpenStream's gap-fill logic relies on them always being
// present in the needed set so any source that doesn't provide
// them triggers a local transcode. Codec2 and IMBE are added
// on demand based on which client protocols are actually present.
// Mid-stream client joins are explicitly out of scope per the
// operator's policy — a client joining after stream-open hears
// silence on a needed-but-not-decided codec until the next TX.
uint32 CReflector::GetNeededCodecsOnModule(char module) const
{
    uint32 needed = (1U << CODEC_AMBEPLUS) | (1U << CODEC_AMBE2PLUS);

    for ( int i = 0; i < m_Clients.GetSize(); i++ )
    {
        // const_cast is the lesser evil here: CClients::GetClient is
        // not const-qualified (pre-existing API limitation), but we
        // only read from the returned client and we already hold the
        // Clients lock (caller contract). If GetClient gains a const
        // overload later, drop this cast.
        const CClient *cl = const_cast<CClients &>(m_Clients).GetClient(i);
        if ( cl == NULL )
            continue;
        if ( cl->GetReflectorModule() != module )
            continue;
        switch ( cl->GetProtocol() )
        {
            case PROTOCOL_M17:
                needed |= (1U << CODEC_CODEC2);
                break;
            case PROTOCOL_P25:
                needed |= (1U << CODEC_IMBE);
                break;
            default:
                // D-Star (DEXTRA/DPLUS/DCS/G3), DMR (MMDVM/PLUS),
                // YSF, NXDN, XLX, IMRS — covered by the always-on
                // AMBE+/AMBE+2 baseline.
                break;
        }
    }
    return needed;
}

void CReflector::ReleaseStreamOwner(CClient *client)
{
    // Null out the owner pointer on any stream owned by this client.
    // Must be called BEFORE deleting the client object to prevent
    // dangling pointer dereference in CloseStream/RouterThread.
    // Caller must hold the Clients lock.
    for ( int i = 0; i < m_Streams.size(); i++ )
    {
        m_Streams[i].Lock();
        if ( m_Streams[i].GetOwnerClient() == client )
        {
            client->NotAMaster();
            m_Streams[i].SetOwnerClient(NULL);
        }
        m_Streams[i].Unlock();
    }
}

bool CReflector::IsStreamOpen(const CDvHeaderPacket *DvHeader)
{
    bool open = false;
    for ( int i = 0; (i < m_Streams.size()) && !open; i++  )
    {
        open =  ( (m_Streams[i].GetStreamId() == DvHeader->GetStreamId()) &&
                  (m_Streams[i].IsOpen()));
    }
    return open;
}

char CReflector::GetStreamModule(CPacketStream *stream)
{
    char module = ' ';
    for ( int i = 0; (i < m_Streams.size()) && (module == ' '); i++ )
    {
        if ( &(m_Streams[i]) == stream )
        {
            module = GetModuleLetter(i);
        }
    }
    return module;
}

////////////////////////////////////////////////////////////////////////////////////////
// xml helpers

void CReflector::WriteXmlFile(std::ofstream &xmlFile)
{
    // write header
    xmlFile << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;
    
    // software version
    char sz[64];
    ::sprintf(sz, "%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION);
    xmlFile << "<Version>" << sz << "</Version>" << std::endl;
    
    // linked peers
    xmlFile << "<" << m_Callsign << "linked peers>" << std::endl;
    // lock
    CPeers *peers = GetPeers();
    // iterate on peers
    for ( int i = 0; i < peers->GetSize(); i++ )
    {
        peers->GetPeer(i)->WriteXml(xmlFile);
    }
    // unlock
    ReleasePeers();
    xmlFile << "</" << m_Callsign << "linked peers>" << std::endl;
    
    // linked nodes
    xmlFile << "<" << m_Callsign << "linked nodes>" << std::endl;
    // lock
    CClients *clients = GetClients();
    // iterate on clients
    for ( int i = 0; i < clients->GetSize(); i++ )
    {
        if ( clients->GetClient(i)->IsNode() )
        {
            clients->GetClient(i)->WriteXml(xmlFile);
        }
    }
    // unlock
    ReleaseClients();
    xmlFile << "</" << m_Callsign << "linked nodes>" << std::endl;
    
    // last heard users
    xmlFile << "<" << m_Callsign << "heard users>" << std::endl;
    // lock
    CUsers *users = GetUsers();
    // iterate on users
    for ( int i = 0; i < users->GetSize(); i++ )
    {
        users->GetUser(i)->WriteXml(xmlFile);
    }
    // unlock
    ReleaseUsers();
    xmlFile << "</" << m_Callsign << "heard users>" << std::endl;
}

////////////////////////////////////////////////////////////////////////////////////////
// json helpers

void CReflector::SendJsonReflectorObject(CUdpSocket &Socket, CIp &Ip)
{
	char Buffer[1024];
 	char cs[CALLSIGN_LEN+1];
 	char mod[8] = "\"A\"";

 	// get reflector callsign
    m_Callsign.GetCallsign((uint8 *)cs);
    cs[CALLSIGN_LEN] = 0;

	// build string
	::sprintf(Buffer, "{\"reflector\":\"%s\",\"modules\":[", cs);
    for ( int i = 0; i < NB_OF_MODULES; i++ )
    {
    	::strcat(Buffer, mod);
    	mod[1]++;
        if ( i < NB_OF_MODULES-1 )
        {
        	::strcat(Buffer, ",");
        }
    }
    ::strcat(Buffer, "]}");

    // and send
    Socket.Send(Buffer, Ip);
}

#define JSON_NBMAX_NODES	250

void CReflector::SendJsonNodesObject(CUdpSocket &Socket, CIp &Ip)
{
	char Buffer[12+(JSON_NBMAX_NODES*94)];

    // nodes object table
    ::sprintf(Buffer, "{\"nodes\":[");
    // lock
    CClients *clients = GetClients();
    // iterate on clients
    for ( int i = 0; (i < clients->GetSize()) && (i < JSON_NBMAX_NODES); i++ )
    {
        clients->GetClient(i)->GetJsonObject(Buffer);
        if ( i < clients->GetSize()-1 )
        {
        	::strcat(Buffer, ",");
        }
    }
    // unlock
    ReleaseClients();
    ::strcat(Buffer, "]}");

    // and send
    //std::cout << Buffer << std::endl;
    Socket.Send(Buffer, Ip);
}

void CReflector::SendJsonStationsObject(CUdpSocket &Socket, CIp &Ip)
{
	char Buffer[15+(LASTHEARD_USERS_MAX_SIZE*94)];

    // stations object table
    ::sprintf(Buffer, "{\"stations\":[");

    // lock
    CUsers *users = GetUsers();
    // iterate on users
    for ( int i = 0; i < users->GetSize(); i++ )
    {
        users->GetUser(i)->GetJsonObject(Buffer);
        if ( i < users->GetSize()-1 )
        {
        	::strcat(Buffer, ",");
        }
    }
    // unlock
    ReleaseUsers();

    ::strcat(Buffer, "]}");

    // and send
    //std::cout << Buffer << std::endl;
    Socket.Send(Buffer, Ip);
}

void CReflector::SendJsonOnairObject(CUdpSocket &Socket, CIp &Ip, const CCallsign &Callsign)
{
    char Buffer[128];
    char sz[CALLSIGN_LEN+1];

    // onair object
    Callsign.GetCallsignString(sz);
    ::sprintf(Buffer, "{\"onair\":\"%s\"}", sz);

    // and send
    //std::cout << Buffer << std::endl;
    Socket.Send(Buffer, Ip);
}

void CReflector::SendJsonOffairObject(CUdpSocket &Socket, CIp &Ip, const CCallsign &Callsign)
{
    char Buffer[128];
    char sz[CALLSIGN_LEN+1];

    // offair object
    Callsign.GetCallsignString(sz);
    ::sprintf(Buffer, "{\"offair\":\"%s\"}", sz);

    // and send
    //std::cout << Buffer << std::endl;
    Socket.Send(Buffer, Ip);
}

////////////////////////////////////////////////////////////////////////////////////////
// MAC address helpers

#ifdef __linux__
#include <netpacket/packet.h>
bool CReflector::UpdateListenMac(void)
{
    struct ifaddrs *ifap, *ifaptr;
    char host[NI_MAXHOST];
    char *ifname = NULL;
    bool found = false;
    
    // iterate through all our AF_INET interface to find the one
    // of our listening ip
    if ( getifaddrs(&ifap) == 0 )
    {
        for ( ifaptr = ifap; (ifaptr != NULL) && !found; ifaptr = (ifaptr)->ifa_next )
        {
            // is it an AF_INET?
            if ( ifaptr->ifa_addr && ifaptr->ifa_addr->sa_family == AF_INET )
            {
                // get the IP
                if ( getnameinfo(ifaptr->ifa_addr,
                        sizeof(struct sockaddr_in),
                        host, NI_MAXHOST,
                        NULL, 0, NI_NUMERICHOST) == 0 )
                {
                    if ( CIp(host) == m_Ip )
                    {
                        // yes, found it
                        found = true;
                        ifname = new char[strlen(ifaptr->ifa_name)+1];
                        strcpy(ifname, ifaptr->ifa_name);
                    }
                }
            }
           
        }
        freeifaddrs(ifap);
    }
    
    // if listening interface name found, iterate again
    // to find the corresponding AF_PACKET interface
    if ( found )
    {
        found = false;
        if ( getifaddrs(&ifap) == 0 )
        {
            for ( ifaptr = ifap; (ifaptr != NULL) && !found; ifaptr = (ifaptr)->ifa_next )
            {
                if ( !strcmp((ifaptr)->ifa_name, ifname) && (ifaptr->ifa_addr->sa_family == AF_PACKET) )
                {
                    found = true;
                    struct sockaddr_ll *s = (struct sockaddr_ll *)(ifaptr->ifa_addr);
                    for ( int i = 0; i < 6; i++ )
                    {
                        m_Mac[i] = s->sll_addr[i];
                    }
                }
            }
        }
        freeifaddrs(ifap);
    }
    
    // done
    return found;
}
#endif

#if defined(__APPLE__)  && defined(__MACH__)
#include <net/if_dl.h>
bool CReflector::UpdateListenMac(void)
{
    struct ifaddrs *ifaddr;
    int  s;
    char host[NI_MAXHOST];
    char *ifname = NULL;
    bool found = false;
    bool ok = false;

    if ( getifaddrs(&ifaddr) != -1)
    {
        // Walk through linked list, maintaining head pointer so we can free list later.
        // until finding our listening AF_INET interface
        for (struct ifaddrs *ifa = ifaddr; (ifa != NULL) && !found; ifa = ifa->ifa_next)
        {
            if (ifa->ifa_addr == NULL)
                continue;

            // is it an AF_INET?
            if (ifa->ifa_addr->sa_family == AF_INET)
            {
                // get IP
                s = getnameinfo(ifa->ifa_addr,
                        sizeof(struct sockaddr_in),
                        host, NI_MAXHOST,
                        NULL, 0, NI_NUMERICHOST);
                if (s != 0)
                {
                   return false;
                }
                // is it our listening ip ?
                if ( CIp(host) == m_Ip )
                {
                    // yes, found it
                    found = true;
                    ifname = new char[strlen(ifa->ifa_name)+1];
                    strcpy(ifname, ifa->ifa_name);
                }
            }
        }
        freeifaddrs(ifaddr);

        // found our interface ?
        if ( found )
        {
            // yes
            //std::cout << ifname << " : " << host << std::endl;
            
            // Walk again through linked list
            // until finding our listening AF_LINK interface
            if ( getifaddrs(&ifaddr) != -1 )
            {
                found = false;
                for (struct ifaddrs *ifa = ifaddr; (ifa != NULL) && !found; ifa = ifa->ifa_next)
                {
                    if (ifa->ifa_addr == NULL)
                        continue;
                    
                    if ( !strcmp(ifa->ifa_name, ifname) && (ifa->ifa_addr->sa_family == AF_LINK))
                    {
                        ::memcpy((void *)m_Mac, (void *)LLADDR((struct sockaddr_dl *)(ifa)->ifa_addr), sizeof(m_Mac));
                        ok = true;
                        found = true;
                    }
                }
                freeifaddrs(ifaddr);
            }
        }

        delete [] ifname;
    }
    return ok;
}
#endif
