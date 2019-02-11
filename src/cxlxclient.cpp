//
//  cxlxclient.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 28/01/2016.
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

#include <string.h>
#include "main.h"
#include "cxlxclient.h"


////////////////////////////////////////////////////////////////////////////////////////
// constructors

CXlxClient::CXlxClient()
{
    m_ProtRev = XLX_PROTOCOL_REVISION_0;
}

CXlxClient::CXlxClient(const CCallsign &callsign, const CIp &ip, char reflectorModule, int protRev)
: CClient(callsign, ip, reflectorModule)
{
    m_ProtRev = protRev;
}

CXlxClient::CXlxClient(const CXlxClient &client)
: CClient(client)
{
    m_ProtRev = client.m_ProtRev;
}

////////////////////////////////////////////////////////////////////////////////////////
// identity

int CXlxClient::GetCodec(void) const
{
    int codec;
    
    switch ( GetProtocolRevision() )
    {
        case XLX_PROTOCOL_REVISION_0:
        case XLX_PROTOCOL_REVISION_1:
        default:
            codec = CODEC_AMBEPLUS;
            break;
        case XLX_PROTOCOL_REVISION_2:
            codec = CODEC_AMBEPLUS; // transcode again anyway
            break;
        case XLX_PROTOCOL_REVISION_3:
            codec = CODEC_NONE;
            break;
    }
    return codec;
}

////////////////////////////////////////////////////////////////////////////////////////
// status

bool CXlxClient::IsAlive(void) const
{
    return (m_LastKeepaliveTime.DurationSinceNow() < XLX_KEEPALIVE_TIMEOUT);
}

