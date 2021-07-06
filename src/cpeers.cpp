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
        // compare object pointers
        if ( (m_Peers[i]) ==  peer )
        {
            // found it !
            if ( !m_Peers[i]->IsAMaster() )
            {
                // remove all clients from reflector client list
                // it is double lock safe to lock Clients list after Peers list
                CClients *clients = g_Reflector.GetClients();
                for ( int i = 0; i < peer->GetNbClients(); i++ )
                {
                    // this also delete the client object
                    clients->RemoveClient(peer->GetClient(i));
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
    
    // find peer
    for ( int i = 0; (i < m_Peers.size()) && (peer == NULL); i++ )
    {
        if ( (m_Peers[i]->GetIp() == Ip)  && (m_Peers[i]->GetProtocol() == Protocol))
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
    
    // find peer
    for ( int i = 0; (i < m_Peers.size()) && (peer == NULL); i++ )
    {
        if ( m_Peers[i]->GetCallsign().HasSameCallsign(Callsign) &&
            (m_Peers[i]->GetIp() == Ip)  &&
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

