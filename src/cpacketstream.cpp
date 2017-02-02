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
    m_OwnerClient = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////
// open / close

bool CPacketStream::Open(const CDvHeaderPacket &DvHeader, CClient *client)
{
    bool ok = false;
    
    // not already open?
    if ( !m_bOpen )
    {
        // update status
        m_bOpen = true;
        m_uiStreamId = DvHeader.GetStreamId();
        m_DvHeader = DvHeader;
        m_OwnerClient = client;
        m_LastPacketTime.Now();
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
}

////////////////////////////////////////////////////////////////////////////////////////
// push & pop

void CPacketStream::Push(CPacket *Packet)
{
    m_LastPacketTime.Now();
    push(Packet);
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

