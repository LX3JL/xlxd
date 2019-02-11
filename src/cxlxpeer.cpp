//
//  cxlxpeer.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 10/12/2016.
//  Copyright © 2016 Jean-Luc Deltombe (LX3JL). All rights reserved.
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
#include "cxlxpeer.h"
#include "cxlxclient.h"


////////////////////////////////////////////////////////////////////////////////////////
// constructor


CXlxPeer::CXlxPeer()
{
}

CXlxPeer::CXlxPeer(const CCallsign &callsign, const CIp &ip, char *modules, const CVersion &version)
: CPeer(callsign, ip, modules, version)
{
    // get protocol revision
    int protrev = GetProtocolRevision(version);
    //std::cout << "Adding XLX peer with protocol revision " << protrev << std::endl;
    
    // and construct all xlx clients
    for ( int i = 0; i < ::strlen(modules); i++ )
    {
        // create
        CXlxClient *client = new CXlxClient(callsign, ip, modules[i], protrev);
        // and append to vector
        m_Clients.push_back(client);
    }
}

CXlxPeer::CXlxPeer(const CXlxPeer &peer)
: CPeer(peer)
{
    for ( int i = 0; i < peer.m_Clients.size(); i++ )
    {
        CXlxClient *client = new CXlxClient((const CXlxClient &)*(peer.m_Clients[i]));
        // grow vector capacity if needed
        if ( m_Clients.capacity() == m_Clients.size() )
        {
            m_Clients.reserve(m_Clients.capacity()+10);
        }
        // and append
        m_Clients.push_back(client);
        
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// destructors

CXlxPeer::~CXlxPeer()
{
}

////////////////////////////////////////////////////////////////////////////////////////
// status

bool CXlxPeer::IsAlive(void) const
{
    return (m_LastKeepaliveTime.DurationSinceNow() < XLX_KEEPALIVE_TIMEOUT);
}

////////////////////////////////////////////////////////////////////////////////////////
// revision helper

int CXlxPeer::GetProtocolRevision(const CVersion &version)
{
    int protrev = XLX_PROTOCOL_REVISION_0;
    
    if ( version.IsEqualOrHigherTo(CVersion(3,0,0)) )
    {
        protrev = XLX_PROTOCOL_REVISION_3;
    }
    else if ( version.IsEqualOrHigherTo(CVersion(2,2,0)) )
    {
        protrev = XLX_PROTOCOL_REVISION_2;
    }
    else if ( version.IsEqualOrHigherTo(CVersion(1,4,0)) )
    {
        protrev = XLX_PROTOCOL_REVISION_1;
    }
    return protrev;
}

