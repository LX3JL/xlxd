//
//  cprotocol.cpp
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
#include <chrono>
#include <future>
#include <random>
#include "cprotocol.h"
#include "cclients.h"
#include "creflector.h"

////////////////////////////////////////////////////////////////////////////////////////
// thread join helper

// Join a thread with a wall-clock timeout. On success, deletes the thread
// and nulls the pointer. On timeout, logs a warning and calls std::terminate()
// — a thread that cannot be stopped at shutdown means something is
// deadlocked, and silently hanging the process is the worst possible outcome
// (requires SIGKILL, prevents log flushing, prevents any post-mortem).
// We deliberately do NOT detach: the thread still holds `this` and would
// cause use-after-free after the destructor returns.
static void JoinWithTimeout(std::thread *&t, int timeoutMs, const char *name)
{
    if ( t == NULL ) return;
    if ( !t->joinable() )
    {
        delete t;
        t = NULL;
        return;
    }

    // Run the blocking join on a helper thread so we can bound the wait.
    // `future` captures `t` by value; once this function returns (whether
    // by completion or terminate), the helper either finishes or dies with
    // the process.
    std::thread *threadRef = t;
    auto future = std::async(std::launch::async, [threadRef]() {
        threadRef->join();
    });

    if ( future.wait_for(std::chrono::milliseconds(timeoutMs)) == std::future_status::ready )
    {
        delete t;
        t = NULL;
        return;
    }

    std::cerr << "FATAL: " << name << " thread did not exit within "
              << timeoutMs << "ms — likely deadlocked; aborting" << std::endl;
    std::terminate();
}


////////////////////////////////////////////////////////////////////////////////////////
// constructor


CProtocol::CProtocol()
{
    m_bStopThread = false;
    m_bStopTxThread = false;
    m_pThread = NULL;
    m_pTxThread = NULL;
    m_Streams.reserve(NB_OF_MODULES);
}


////////////////////////////////////////////////////////////////////////////////////////
// destructor

CProtocol::~CProtocol()
{
    // kill RX thread (bounded join — see JoinWithTimeout rationale)
    m_bStopThread = true;
    JoinWithTimeout(m_pThread, 5000, "RX");

    // kill TX thread (must notify CV to unblock a waiting TxTask)
    m_bStopTxThread = true;
    m_QueueCondVar.notify_one();
    JoinWithTimeout(m_pTxThread, 5000, "TX");

    // empty queue — delete any remaining packets to prevent leak on shutdown
    m_Queue.Lock();
    while ( !m_Queue.empty() )
    {
        delete m_Queue.front();
        m_Queue.pop();
    }
    m_Queue.Unlock();
}

////////////////////////////////////////////////////////////////////////////////////////
// initialization

bool CProtocol::Init(void)
{
    // init reflector apparent callsign
    m_ReflectorCallsign = g_Reflector.GetCallsign();

    // reset stop flags
    m_bStopThread = false;
    m_bStopTxThread = false;

    // start RX thread (or legacy single thread for unconverted protocols)
    m_pThread = new std::thread(CProtocol::RxThread, this);

    // start TX thread if protocol uses split-thread mode
    if ( UsesSplitThreads() )
    {
        m_pTxThread = new std::thread(CProtocol::TxThread, this);
    }

    // done
    return true;
}

void CProtocol::Close(void)
{
    // stop RX first — no new data enters the system (bounded join)
    m_bStopThread = true;
    JoinWithTimeout(m_pThread, 5000, "RX");

    // then stop TX — drain remaining packets and exit (bounded join)
    m_bStopTxThread = true;
    m_QueueCondVar.notify_one();
    JoinWithTimeout(m_pTxThread, 5000, "TX");
}


////////////////////////////////////////////////////////////////////////////////////////
// threads

void CProtocol::RxThread(CProtocol *This)
{
    if ( This->UsesSplitThreads() )
    {
        while ( !This->m_bStopThread )
        {
            This->RxTask();
        }
    }
    else
    {
        // legacy single-thread mode — run Task() which does both RX and TX
        while ( !This->m_bStopThread )
        {
            This->Task();
        }
    }
}

void CProtocol::TxThread(CProtocol *This)
{
    while ( !This->m_bStopTxThread )
    {
        This->TxTask();
    }

    // final drain — process any packets remaining after stop
    bool drained = false;
    while ( !drained )
    {
        This->HandleQueue();
        This->m_Queue.Lock();
        drained = This->m_Queue.empty();
        This->m_Queue.Unlock();
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// default TX task — condition variable wait + HandleQueue

void CProtocol::TxTask(void)
{
    // wait for queue notification or 20ms timeout
    {
        std::unique_lock<std::mutex> lk(m_QueueCondMutex);
        m_QueueCondVar.wait_for(lk, std::chrono::milliseconds(20));
    }

    HandleQueue();
}

////////////////////////////////////////////////////////////////////////////////////////
// packet encoding helpers

bool CProtocol::EncodeDvPacket(const CPacket &packet, CBuffer *buffer) const
{
    bool ok = false;
    if ( packet.IsDvFrame() )
    {
        if ( packet.IsLastPacket() )
        {
            ok = EncodeDvLastFramePacket((const CDvLastFramePacket &)packet, buffer);
        }
        else
        {
            ok = EncodeDvFramePacket((const CDvFramePacket &)packet, buffer);
        }
    }
    else if ( packet.IsDvHeader() )
    {
        ok = EncodeDvHeaderPacket((const CDvHeaderPacket &)packet, buffer);
    }
    else
    {
        buffer->clear();
    }
    return ok;
}

////////////////////////////////////////////////////////////////////////////////////////
// streams helpers

void CProtocol::OnDvFramePacketIn(CDvFramePacket *Frame, const CIp *Ip)
{
    // find the stream
    CPacketStream *stream = GetStream(Frame->GetStreamId(), Ip);
    if ( stream == NULL )
    {
        // try late entry buffering
        if ( !g_Reflector.BufferPendingFrame(Frame->GetStreamId(), Frame) )
        {
            delete Frame;
        }
    }
    else
    {
        // and push
        stream->Lock();
        stream->Push(Frame);
        stream->Unlock();
    }
}

void CProtocol::OnDvLastFramePacketIn(CDvLastFramePacket *Frame, const CIp *Ip)
{
    // find the stream
    CPacketStream *stream = GetStream(Frame->GetStreamId(), Ip);
    if ( stream == NULL )
    {
        // check if this is for a promoted late-entry stream
        stream = g_Reflector.GetPromotedStream(Frame->GetStreamId());
        if ( stream != NULL )
        {
            // handle last frame and close the promoted stream
            HandleStreamLastFrame(stream, Frame);
            g_Reflector.CloseStream(stream);
        }
        else
        {
            // check if pending (not yet promoted) — cancel it
            g_Reflector.CancelPendingEntry(Frame->GetStreamId());
            delete Frame;
        }
    }
    else
    {
        HandleStreamLastFrame(stream, Frame);
        g_Reflector.CloseStream(stream);

        // remove from our local stream cache to prevent stale entries
        // that could cause false Tickle matches on future streams
        for ( int i = 0; i < m_Streams.size(); i++ )
        {
            if ( m_Streams[i] == stream )
            {
                m_Streams.erase(m_Streams.begin()+i);
                break;
            }
        }
    }
}

void CProtocol::HandleStreamLastFrame(CPacketStream *stream, CDvLastFramePacket *Frame)
{
    // Push the last frame into the stream for transcoding and routing.
    // The drain wait is handled by CloseStream (called by the caller after this
    // returns) to avoid an ABBA deadlock: the drain loop would hold the stream
    // lock while calling CCodecStream::IsEmpty (which acquires the codec lock),
    // while the codec thread holds its lock and tries to acquire the stream lock.
    stream->Lock();
    stream->Push(Frame);
    stream->Unlock();
}

////////////////////////////////////////////////////////////////////////////////////////
// stream handle helpers

CPacketStream *CProtocol::GetStream(uint16 uiStreamId, const CIp *Ip)
{
    CPacketStream *stream = NULL;

    // find if we have a stream with same streamid in our cache
    for ( int i = 0; (i < m_Streams.size()) && (stream == NULL); i++ )
    {
        if ( m_Streams[i]->GetStreamId() == uiStreamId )
        {
            // if Ip is NULL, accept any stream with matching streamId
            // otherwise, also check if IP address matches (ignore port, as some
            // protocols like NXDN may send packets from different source ports)
            if ( Ip == NULL )
            {
                stream = m_Streams[i];
            }
            else if ( m_Streams[i]->GetOwnerIp() != NULL )
            {
                // Compare only the address part, not the port
                if ( Ip->GetAddr() == m_Streams[i]->GetOwnerIp()->GetAddr() )
                {
                    stream = m_Streams[i];
                }
            }
        }
    }
    // done
    return stream;
}

uint16 CProtocol::AllocateGlobalStreamIdNonce(void)
{
    // One process-wide counter shared across every CProtocol-derived
    // class. Seeded from std::random_device the first time this runs
    // (Meyers-singleton style — guaranteed thread-safe initialisation
    // under C++11). Per-protocol counters were rejected because two
    // protocols starting from low values (e.g. NXDN sid 1, 2, 3 and
    // P25 sid 1, 2, 3) can hand the SAME wire DCS sid to two different
    // sources back-to-back if the gateway routes both onto the same
    // egress module — exactly the cross-source-rapid-rekey bug we just
    // fixed for same-source rapid rekeys, but inter-protocol.
    //
    // Random seed also avoids a "ghost match" against a gateway that
    // cached the previous xlxd run's last-used sid: a fresh process
    // starts somewhere in the 16-bit space chosen by the OS RNG, not
    // at a fixed offset.
    //
    // Returns 1..65535 (skips 0 because CReflector::OpenStream rejects
    // a zero stream-id at creflector.cpp:212).
    static std::atomic<uint32_t> counter{
        []()
        {
            std::random_device rd;
            // Seed in [1, 65535]. Excluding 0 avoids the edge case
            // where the very first ++counter dispenses sid 1 — the
            // same value the pre-fix per-protocol counters always
            // started at. Picking 0 would defeat the randomised-start
            // benefit for the first allocation after each xlxd
            // restart and re-create the original cross-protocol clash
            // that motivated this whole design.
            uint32_t seed = rd() & 0xFFFFu;
            if (seed == 0) seed = 1;
            return seed;
        }()
    };

    uint32_t v = ++counter;
    // Wrap to 16 bits and re-skip zero. Two ++counter calls handle the
    // wraparound case where v lands exactly on 0x10000, 0x20000, ... —
    // we read the low 16 bits, and if that's zero we increment again.
    uint16_t out = (uint16_t)(v & 0xFFFFu);
    if (out == 0)
    {
        out = (uint16_t)((++counter) & 0xFFFFu);
        if (out == 0) out = 1;
    }
    return out;
}

void CProtocol::CheckStreamsTimeout(void)
{
    for ( int i = 0; i < m_Streams.size(); i++ )
    {
        // time out ?
        m_Streams[i]->Lock();
        if ( m_Streams[i]->IsExpired() )
        {
            // yes, close it
            m_Streams[i]->Unlock();
            g_Reflector.CloseStream(m_Streams[i]);
            // and remove it
            m_Streams.erase(m_Streams.begin()+i);
            i--;
        }
        else
        {
            m_Streams[i]->Unlock();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// queue helper

void CProtocol::HandleQueue(void)
{
    // the default protocol just keep queue empty
    m_Queue.Lock();
    while ( !m_Queue.empty() )
    {
        delete m_Queue.front();
        m_Queue.pop();
    }
    m_Queue.Unlock();
}

////////////////////////////////////////////////////////////////////////////////////////
// syntax helper

bool CProtocol::IsNumber(char c) const
{
    return ((c >= '0') && (c <= '9'));
}

bool CProtocol::IsLetter(char c) const
{
    return ((c >= 'A') && (c <= 'Z'));
}

bool CProtocol::IsSpace(char c) const
{
    return (c == ' ');
}

////////////////////////////////////////////////////////////////////////////////////////
// DestId to Module helper

char CProtocol::DmrDstIdToModule(uint32 tg) const
{
    if ( tg >= 1 && tg <= (uint32)NB_OF_MODULES )
    {
        return ((char)(tg - 1) + 'A');
    }
    return ' ';
}

uint32 CProtocol::ModuleToDmrDestId(char m) const
{
    return (uint32)(m - 'A')+1;
}

