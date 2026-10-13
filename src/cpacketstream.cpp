//
//  cpacketstream.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 06/11/2015.
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
#include "cpacketstream.h"

////////////////////////////////////////////////////////////////////////////////////////
// constructor

CPacketStream::CPacketStream()
{
    m_bOpen = false;
    m_bClosing = false;
    m_bCodecPending = false;
    m_bHasPendingHeader = false;
    m_uiStreamId = 0;
    m_uiPacketCntr = 0;
    m_uiInputCodec = CODEC_NONE;
    m_OwnerClient = NULL;
    m_bHasCachedOwnerIp = false;
    m_CodecStream = NULL;
    m_uiPreCodecDropped = 0;
}

////////////////////////////////////////////////////////////////////////////////////////
// open / close

bool CPacketStream::Open(const CDvHeaderPacket &DvHeader, CClient *client)
{
    bool ok = false;

    // not already open?
    if ( !m_bOpen )
    {
        // Phase 1 of a two-phase open: reserve the stream and record the
        // owner, but do NOT negotiate with the transcoder yet. The caller
        // (CReflector::OpenStream) finishes the handshake asynchronously
        // via AttachCodecStream(), releasing reflector locks during the
        // potentially slow AMBEd negotiation.
        m_bOpen = true;
        m_bCodecPending = true;
        m_uiStreamId = DvHeader.GetStreamId();
        m_uiPacketCntr = 0;
        m_DvHeader = DvHeader;
        // Mark the header as pending emission. AttachCodecStream() pushes a
        // fresh copy of m_DvHeader onto the base queue at the start of
        // Phase 3, just before draining the pre-codec buffer — so on the
        // wire the header arrives ~one transcoder-roundtrip before the
        // first voice frame instead of the ~1-1.3s ahead that the old
        // Phase-1 push produced. See header comment on Open() for the
        // rationale.
        m_bHasPendingHeader = true;
        m_OwnerClient = client;
        m_CachedOwnerIp = client->GetIp();
        m_bHasCachedOwnerIp = true;
        m_LastPacketTime.Now();
        m_uiInputCodec = client->GetCodec();
        m_CodecStream = NULL;
        m_uiPreCodecDropped = 0;
        ok = true;
    }
    return ok;
}

void CPacketStream::AttachCodecStream(CCodecStream *codecStream, bool transcodeRequested)
{
    // Caller holds the CPacketStream lock.
    // Phase 3 of the two-phase open: attach the negotiated CCodecStream
    // (or NULL if the transcoder was unavailable) and replay any voice
    // frames that were buffered during Phase 2.

    // If the stream was closed concurrently (CloseStream ran while we were
    // negotiating the transcoder), release the codec stream right back to
    // the transcoder and drop any buffered frames AND the pending header.
    // A header was never emitted in this scenario, so receivers on the
    // egress protocols simply never see the aborted transmission — this
    // is a mild behavioural change from the old "immediate push at Phase
    // 1" design, where a very short keyup would leave an orphan header on
    // listeners' last-heard displays even though no voice played.
    if ( !m_bOpen || m_bClosing )
    {
        if ( codecStream != NULL )
        {
            g_Transcoder.ReleaseStream(codecStream);
        }
        while ( !m_PreCodecBuffer.empty() )
        {
            delete m_PreCodecBuffer.front();
            m_PreCodecBuffer.pop();
        }
        m_bHasPendingHeader = false;
        m_bCodecPending = false;
        return;
    }

    m_CodecStream = codecStream;
    // Only warn when the caller actually asked for transcoding and didn't
    // get any (e.g. ambed unavailable). Paths that intentionally skip the
    // transcoder (late-entry promotion) pass transcodeRequested=false.
    if ( m_CodecStream == NULL && transcodeRequested && m_uiInputCodec != CODEC_NONE )
    {
        std::cout << "Warning: stream opened without transcoding (ambed unavailable) — same-mode only" << std::endl;
    }

    // Emit the deferred header to the base stream queue BEFORE draining
    // the voice frames. The router thread will pick it up and fan it out
    // to egress protocols one tick later — then the voice frames follow
    // in FIFO order. This pairs the header with the first voice frame on
    // the wire within a couple of router ticks (< 50ms), matching native
    // D-Star header → first-voice timing.
    //
    // We emit even when the transcoder failed (codecStream == NULL): the
    // bypass drain below will ship raw source-codec bytes (which will be
    // silent on D-Star receivers unless the source was also D-Star), but
    // listeners still see who attempted to transmit. This matches the
    // old behaviour's semantics for the ambed-unavailable scenario.
    if ( m_bHasPendingHeader )
    {
        push(new CDvHeaderPacket(m_DvHeader));
        m_bHasPendingHeader = false;
    }

    // Drain the buffered frames in FIFO order through whichever path is
    // now live (transcoder if attached and connected, otherwise bypass).
    while ( !m_PreCodecBuffer.empty() )
    {
        CPacket *pkt = m_PreCodecBuffer.front();
        m_PreCodecBuffer.pop();
        if ( m_CodecStream != NULL && m_CodecStream->IsConnected() && pkt->HaveTranscodableAmbe() )
        {
            m_CodecStream->Lock();
            m_CodecStream->push(pkt);
            m_CodecStream->Unlock();
        }
        else
        {
            push(pkt);
        }
    }

    if ( m_uiPreCodecDropped > 0 )
    {
        std::cout << "Warning: pre-codec buffer overflowed during transcoder open — "
                  << m_uiPreCodecDropped << " voice frame(s) dropped" << std::endl;
    }

    // After this point, Push() routes new arrivals through the normal path.
    m_bCodecPending = false;
}

void CPacketStream::Close(void)
{
    // update status
    m_bOpen = false;
    m_bCodecPending = false;
    // Clear the pending-header flag for hygiene. If Close() runs before
    // AttachCodecStream(), the header was never emitted — we deliberately
    // do NOT push it here, matching the early-close branch in
    // AttachCodecStream (see comment there for rationale).
    m_bHasPendingHeader = false;
    m_uiStreamId = 0;
    m_OwnerClient = NULL;
    m_bHasCachedOwnerIp = false;
    g_Transcoder.ReleaseStream(m_CodecStream);
    m_CodecStream = NULL;

    // If the stream is being closed before AttachCodecStream() ran (e.g.
    // the transcoder negotiation failed in some exceptional way and
    // CloseStream is about to clean up), drain any leftover buffered
    // voice frames. In normal flow this queue is already empty.
    while ( !m_PreCodecBuffer.empty() )
    {
        delete m_PreCodecBuffer.front();
        m_PreCodecBuffer.pop();
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// push & pop

void CPacketStream::Push(CPacket *Packet)
{
    // update stream dependent packet data
    m_LastPacketTime.Now();
    Packet->UpdatePids(m_uiPacketCntr);
    if (Packet->IsDvFrame()) { m_uiPacketCntr++; }

    // If the transcoder stream is still being opened (Phase 2 of the
    // two-phase open), buffer transcodable voice frames so they can be
    // replayed through the transcoder once it attaches. Non-transcodable
    // packets (DV header, DV last-frame) go straight to the stream queue
    // so they reach clients without added latency.
    if ( m_bCodecPending && Packet->HaveTranscodableAmbe() )
    {
        if ( m_PreCodecBuffer.size() < PRE_CODEC_BUFFER_MAX )
        {
            m_PreCodecBuffer.push(Packet);
        }
        else
        {
            // Buffer full — this should be rare (100 frames = 2s of voice,
            // longer than any AMBEd handshake). Count the drop so
            // AttachCodecStream() can log it. We drop the NEW frame to
            // preserve the continuous run of already-buffered audio.
            m_uiPreCodecDropped++;
            delete Packet;
        }
        return;
    }

    // Use transcoder for cross-codec conversion (D-Star, DMR, M17)
    // Each input codec transcodes to both other codecs in a combined response
    if ( m_CodecStream != NULL )
    {
        m_CodecStream->Lock();
        {
            // transcoder ready & frame need transcoding ?
            if ( m_CodecStream->IsConnected() && Packet->HaveTranscodableAmbe() )
            {
                // yes, push packet to trancoder queue
                // trancoder will push it after transcoding
                // is completed
                m_CodecStream->push(Packet);
            }
            else
            {
                // no, just bypass transcoder
                push(Packet);
            }
        }
        m_CodecStream->Unlock();
    }
    else
    {
        // No transcoder, push directly
        push(Packet);
    }
}

bool CPacketStream::IsEmpty(void)
{
    // Snapshot m_CodecStream under the stream lock. It is nulled in
    // Close(); without the snapshot, the drain loop (which calls us
    // without holding the stream lock) could read a freed pointer after
    // Close() runs. The snapshot is safe to dereference outside the
    // lock because the CloseStream serialisation guarantee (see
    // TryBeginClose) means the object is not freed while a drain is in
    // progress on the same stream.
    CCodecStream *codec;
    Lock();
    codec = m_CodecStream;
    bool preCodecEmpty = m_PreCodecBuffer.empty();
    Unlock();

    // While the transcoder is still opening (Phase 2), voice frames are
    // held in the pre-codec buffer. Treat the stream as non-empty until
    // AttachCodecStream() drains that buffer.
    if ( !preCodecEmpty )
    {
        return false;
    }

    if ( codec != NULL && !codec->IsEmpty() )
    {
        return false;
    }

    Lock();
    bool bEmpty = empty();
    Unlock();

    return bEmpty;
}

// Sum of packets everywhere in this stream's delivery pipeline: base
// queue, pre-codec ring buffer, and the codec's internal queues + in-
// flight count. Same field set IsEmpty() inspects, returning a count
// rather than a boolean. Used by CReflector::CloseStream for drain
// stall detection — see the rationale in CloseStream.
//
// Lock discipline mirrors IsEmpty(): take the stream lock briefly to
// snapshot the base queue size, the pre-codec buffer size, and the
// codec pointer, then read the codec's depth under its own internal
// lock. Safe against concurrent Close() because CReflector::CloseStream
// holds TryBeginClose and calls Close() only AFTER the drain loop
// finishes, so m_CodecStream is not freed while a drain is in progress.
size_t CPacketStream::GetPipelineDepth(void)
{
    size_t depth;
    CCodecStream *codec;
    Lock();
    depth = size() + m_PreCodecBuffer.size();
    codec = m_CodecStream;
    Unlock();

    if ( codec != NULL )
    {
        depth += codec->GetDepth();
    }
    return depth;
}

////////////////////////////////////////////////////////////////////////////////////////
// get

const CIp *CPacketStream::GetOwnerIp(void)
{
    if ( m_bHasCachedOwnerIp )
    {
        return &m_CachedOwnerIp;
    }
    return NULL;
}

