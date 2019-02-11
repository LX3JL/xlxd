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
    m_uiStreamId = 0;
    m_uiPacketCntr = 0;
    m_OwnerClient = NULL;
    m_CodecIn = CODEC_NONE;
    m_CodecStream = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////
// open / close

bool CPacketStream::Open(const CDvHeaderPacket &DvHeader, CClient *client, uint8 CodecIn)
{
    bool ok = false;
    
    // not already open?
    if ( !m_bOpen )
    {
        // update status
        m_bOpen = true;
        m_uiStreamId = DvHeader.GetStreamId();
        m_uiPacketCntr = 0;
        m_DvHeader = DvHeader;
        m_OwnerClient = client;
        m_LastPacketTime.Now();
        m_CodecIn = CodecIn;
        m_CodecStream = g_Transcoder.GetStream(this, CodecIn);
        ok = true;
    }
    return ok;
}

void CPacketStream::Close(void)
{
    // update status
    m_bOpen = false;
    m_uiStreamId = 0;
    m_OwnerClient = NULL;
    g_Transcoder.ReleaseStream(m_CodecStream);
    m_CodecIn = CODEC_NONE;
    m_CodecStream = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////
// push & pop

void CPacketStream::Push(CPacket *Packet)
{
    // update stream dependent packet data
    m_LastPacketTime.Now();
    Packet->UpdatePids(m_uiPacketCntr++);
    // transcoder avaliable ?
    if ( m_CodecStream != NULL )
    {
        // todo: verify no possibilty of double lock here
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
                // no, just bypass tarnscoder
                push(Packet);
            }
        }
        m_CodecStream->Unlock();
    }
    else
    {
        // otherwise, push direct push
        push(Packet);
    }
}

bool CPacketStream::IsEmpty(void) const
{
    bool bEmpty = empty();
    
    // also check no packets still in Codec stream's queue
    if ( bEmpty && (m_CodecStream != NULL) )
    {
        bEmpty &= m_CodecStream->IsEmpty();
    }

    // done
    return bEmpty;
}

////////////////////////////////////////////////////////////////////////////////////////
// get

const CIp *CPacketStream::GetOwnerIp(void)
{
    if ( m_OwnerClient != NULL )
    {
        return &(m_OwnerClient->GetIp());
    }
    return NULL;
}

