//
//  cpeers.cpp
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
#include "creflector.h"
#include "cpeers.h"


////////////////////////////////////////////////////////////////////////////////////////
// constructor


CPeers::CPeers()
{
    m_Peers.reserve(100);
}

////////////////////////////////////////////////////////////////////////////////////////
// destructors

CPeers::~CPeers()
{
    m_Mutex.lock();
    {
        for ( int i = 0; i < m_Peers.size(); i++ )
        {
            delete m_Peers[i];
        }
        m_Peers.clear();
        
    }
    m_Mutex.unlock();
}

////////////////////////////////////////////////////////////////////////////////////////
// manage peers

void CPeers::AddPeer(CPeer *peer)
{
    // first check if peer already exists
    bool found = false;
    for ( int i = 0; (i < m_Peers.size()) && !found; i++ )
    {
        found = (*peer == *m_Peers[i]);
        // if found, just do nothing
        // so *peer keep pointing on a valid object
        // on function return
        if ( found )
        {
            // delete new one
            delete peer;
            //std::cout << "Adding existing peer " << peer->GetCallsign() << " at " << peer->GetIp() << std::endl;
        }
    }
    
    // if not, append to the vector
    if ( !found )
    {
        // grow vector capacity if needed
        if ( m_Peers.capacity() == m_Peers.size() )
        {
            m_Peers.reserve(m_Peers.capacity()+10);
        }
        // append peer to reflector peer list
        m_Peers.push_back(peer);

        // Warn if multiple peers share the same FULL ENDPOINT (IP+port)
        // and protocol — those would conflict regardless of which
        // FindPeer variant is used. Two peers on the same IP but
        // different ports are legitimate for YSF/NXDN/P25 (handled by
        // FindPeerByIpPort); two peers on the same IP for an IP-only
        // protocol (DExtra/DPlus/DCS/XLX/M17) would conflict in
        // FindPeer(Ip, Protocol) but CPeer::operator== should have
        // deduped them already at the top of AddPeer. So the only
        // remaining same-endpoint case here is a genuine bug — keep
        // the warning.
        for ( int j = 0; j < (int)m_Peers.size() - 1; j++ )
        {
            if ( (m_Peers[j]->GetIp() == peer->GetIp()) &&
                 (m_Peers[j]->GetProtocol() == peer->GetProtocol()) )
            {
                std::cout << "WARNING: Multiple " << peer->GetProtocolName() << " peers at "
                          << peer->GetIp() << " - "
                          << m_Peers[j]->GetCallsign() << " and " << peer->GetCallsign()
                          << " will conflict in peer lookups" << std::endl;
            }
        }

        std::cout << "New peer " << peer->GetCallsign() << " at " << peer->GetIp()
                  << " added with protocol " << peer->GetProtocolName()  << std::endl;
        // and append all peer's client to reflector client list
        // it is double lock safe to lock Clients list after Peers list
        CClients *clients = g_Reflector.GetClients();
        for ( int i = 0; i < peer->GetNbClients(); i++ )
        {
            clients->AddClient(peer->GetClient(i));
        }
        g_Reflector.ReleaseClients();
        
        // notify
        g_Reflector.OnPeersChanged();
    }
}

void CPeers::RemovePeer(CPeer *peer)
{
    // look for the client
    bool found = false;
    for ( int i = 0; (i < m_Peers.size()) && !found; i++ )
    {
        // compare objetc pointers
        if ( (m_Peers[i]) ==  peer )
        {
            // found it !
            if ( !m_Peers[i]->IsAMaster() )
            {
                // remove all clients from reflector client list
                // it is double lock safe to lock Clients list after Peers list
                CClients *clients = g_Reflector.GetClients();
                for ( int j = 0; j < peer->GetNbClients(); j++ )
                {
                    // null out stream owner pointer before deleting the client,
                    // preventing dangling pointer dereference in CloseStream
                    g_Reflector.ReleaseStreamOwner(peer->GetClient(j));
                    // this also delete the client object
                    clients->RemoveClient(peer->GetClient(j));
                }
                // so clear it then
                m_Peers[i]->ClearClients();
                g_Reflector.ReleaseClients();
                
                // remove it
                std::cout << "Peer " << m_Peers[i]->GetCallsign() << " at " << m_Peers[i]->GetIp()
                         << " removed" << std::endl;
                delete m_Peers[i];
                m_Peers.erase(m_Peers.begin()+i);
                found = true;
                // notify
                g_Reflector.OnPeersChanged();
            }
        }
    }
}

CPeer *CPeers::GetPeer(int i)
{
    if ( (i >= 0) && (i < m_Peers.size()) )
    {
        return m_Peers[i];
    }
    else
    {
        return NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// find peers

CPeer *CPeers::FindPeer(const CIp &Ip, int Protocol)
{
    CPeer *peer = NULL;

    // Find peer by address only (ignore port — ports may differ due to NAT
    // or stored-port vs response-port differences in peer connection
    // handshakes). Appropriate for protocols where a single peer per IP
    // per protocol is the operational model (DExtra/DPlus/DCS/XLX/M17).
    // For protocols where two peers can legitimately share an IP on
    // different listening ports (YSF/NXDN/P25), use FindPeerByIpPort()
    // instead.
    for ( int i = 0; (i < m_Peers.size()) && (peer == NULL); i++ )
    {
        if ( (m_Peers[i]->GetIp().GetAddr() == Ip.GetAddr())  && (m_Peers[i]->GetProtocol() == Protocol))
        {
            peer = m_Peers[i];
        }
    }

    // done
    return peer;
}

CPeer *CPeers::FindPeerByIpPort(const CIp &Ip, int Protocol)
{
    CPeer *peer = NULL;

    // Find peer by full endpoint (address AND port) plus protocol. Used
    // by YSF / NXDN / P25 where peers listen on well-known ports
    // declared in the interlink file and the protocols' server-style
    // semantics guarantee inbound source-port == peer listening port.
    // CIp::operator== compares family + address + port — exactly the
    // full-endpoint match we want.
    for ( int i = 0; (i < m_Peers.size()) && (peer == NULL); i++ )
    {
        if ( (m_Peers[i]->GetIp() == Ip) && (m_Peers[i]->GetProtocol() == Protocol) )
        {
            peer = m_Peers[i];
        }
    }

    // done
    return peer;
}

CPeer *CPeers::FindPeer(const CCallsign &Callsign, const CIp &Ip, int Protocol)
{
    CPeer *peer = NULL;

    // find peer by callsign and address (ignore port)
    for ( int i = 0; (i < m_Peers.size()) && (peer == NULL); i++ )
    {
        if ( m_Peers[i]->GetCallsign().HasSameCallsign(Callsign) &&
            (m_Peers[i]->GetIp().GetAddr() == Ip.GetAddr())  &&
            (m_Peers[i]->GetProtocol() == Protocol) )
        {
            peer = m_Peers[i];
        }
    }

    // done
    return peer;
}

CPeer *CPeers::FindPeer(const CCallsign &Callsign, int Protocol)
{
    CPeer *peer = NULL;
    
    // find peer
    for ( int i = 0; (i < m_Peers.size()) && (peer == NULL); i++ )
    {
        if ( (m_Peers[i]->GetProtocol() == Protocol) &&
            m_Peers[i]->GetCallsign().HasSameCallsign(Callsign) )
        {
            peer = m_Peers[i];
        }
    }
    
    // done
    return peer;
}


////////////////////////////////////////////////////////////////////////////////////////
// iterate on peers

CPeer *CPeers::FindNextPeer(int Protocol, int *index)
{
    CPeer *peer = NULL;
    
    // find next peer
    bool found = false;
    for ( int i = *index+1; (i < m_Peers.size()) && !found; i++ )
    {
        if ( m_Peers[i]->GetProtocol() == Protocol )
        {
            found = true;
            peer = m_Peers[i];
            *index = i;
        }
    }
    return peer;
}

