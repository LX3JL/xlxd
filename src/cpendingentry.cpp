//
//  cpendingentry.cpp
//  xlxd
//
//  Created for late-entry stream support.
//  Copyright © 2025. All rights reserved.
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
#include "cpendingentry.h"
#include "cpacketstream.h"

////////////////////////////////////////////////////////////////////////////////////////
// constructor / destructor

CPendingEntry::CPendingEntry()
{
    m_State = INACTIVE;
    m_Header = NULL;
    m_Client = NULL;
    m_uiStreamId = 0;
    m_PromotedStream = NULL;
}

CPendingEntry::~CPendingEntry()
{
    Clear();
}

////////////////////////////////////////////////////////////////////////////////////////
// lifecycle

void CPendingEntry::Set(CDvHeaderPacket *header, CClient *client)
{
    // clear any existing state
    Clear();

    m_State = BUFFERING;
    m_Header = header;              // take ownership
    m_Client = client;              // borrow pointer
    m_uiStreamId = header->GetStreamId();
    m_PromotedStream = NULL;
    m_CreatedTime.Now();
}

void CPendingEntry::Clear(void)
{
    if ( m_Header != NULL )
    {
        delete m_Header;
        m_Header = NULL;
    }
    DiscardFrames();
    m_State = INACTIVE;
    m_Client = NULL;
    m_uiStreamId = 0;
    m_PromotedStream = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////
// state queries

bool CPendingEntry::IsExpired(void) const
{
    return (m_CreatedTime.DurationSinceNow() > PENDING_TIMEOUT);
}

////////////////////////////////////////////////////////////////////////////////////////
// frame buffering

bool CPendingEntry::BufferFrame(CPacket *frame)
{
    if ( m_State != BUFFERING )
    {
        return false;
    }

    if ( IsBufferFull() )
    {
        TransitionToHeaderOnly();
        return false;
    }

    m_Frames.push_back(frame);     // take ownership
    return true;
}

void CPendingEntry::TransitionToHeaderOnly(void)
{
    DiscardFrames();
    m_State = HEADER_ONLY;
}

////////////////////////////////////////////////////////////////////////////////////////
// promotion

void CPendingEntry::Promote(CPacketStream *stream)
{
    m_PromotedStream = stream;
    m_State = PROMOTED;
}

void CPendingEntry::ReplayInto(CPacketStream *stream)
{
    // Replay at half normal spacing (10ms instead of 20ms) so we catch up
    // gradually rather than dumping all frames at once
    static const int REPLAY_SPACING_MS = 10;

    while ( !m_Frames.empty() )
    {
        CPacket *frame = m_Frames.front();
        m_Frames.pop_front();

        stream->Lock();
        stream->Push(frame);       // ownership transferred to stream
        stream->Unlock();

        // pace the replay if more frames remain
        if ( !m_Frames.empty() )
        {
            CTimePoint::TaskSleepFor(REPLAY_SPACING_MS);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// helpers

void CPendingEntry::DiscardFrames(void)
{
    while ( !m_Frames.empty() )
    {
        delete m_Frames.front();
        m_Frames.pop_front();
    }
}
