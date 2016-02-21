//
//  cclients.cpp
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
#include "creflector.h"
#include "cclients.h"


////////////////////////////////////////////////////////////////////////////////////////
// constructor


CClients::CClients()
{
    m_Clients.reserve(100);
}

////////////////////////////////////////////////////////////////////////////////////////
// destructors

CClients::~CClients()
{
    m_Mutex.lock();
    {
        for ( int i = 0; i < m_Clients.size(); i++ )
        {
            delete m_Clients[i];
        }
        m_Clients.clear();
            
    }
    m_Mutex.unlock();
}

////////////////////////////////////////////////////////////////////////////////////////
// manage Clients

void CClients::AddClient(CClient *client)
{
    // first check if client already exists
    bool found = false;
    for ( int i = 0; (i < m_Clients.size()) && !found; i++ )
    {
        found = (*client == *m_Clients[i]);
        // if found, just do nothing
        // so *client keep pointing on a valid object
        // on function return
        if ( found )
        {
            // delete new one
            delete client;
            //std::cout << "Adding existing client " << client->GetCallsign() << " at " << client->GetIp() << std::endl;
        }
    }
    
    // if not, append to the vector
    if ( !found )
    {
        // grow vector capacity if needed
        if ( m_Clients.capacity() == m_Clients.size() )
        {
            m_Clients.reserve(m_Clients.capacity()+10);
        }
        // and append
        m_Clients.push_back(client);
        std::cout << "New client " << client->GetCallsign() << " at " << client->GetIp() 
        	<< " added with protocol " << client->GetProtocol()  << std::endl;
        // notify
        g_Reflector.OnClientsChanged();
    }
 }

void CClients::RemoveClient(CClient *client)
{    
    // look for the client
    bool found = false;
    for ( int i = 0; (i < m_Clients.size()) && !found; i++ )
    {
        // compare objetc pointers
        if ( (m_Clients[i]) ==  client )
        {
            // found it !
            if ( !m_Clients[i]->IsAMaster() )
            {
                // remove it
                std::cout << "Client " << m_Clients[i]->GetCallsign() << " at " << m_Clients[i]->GetIp()
                          << " removed" << std::endl;
                delete m_Clients[i];
                m_Clients.erase(m_Clients.begin()+i);
                found = true;
                // notify
                g_Reflector.OnClientsChanged();
            }
        }
    }
}

CClient *CClients::GetClient(int i)
{
    if ( (i >= 0) && (i < m_Clients.size()) )
    {
        return m_Clients[i];
    }
    else
    {
        return NULL;
    }
}

bool CClients::IsClient(CClient *client) const
{
    bool found = false;
    for ( int i = 0; (i < m_Clients.size()) && !found; i++ )
    {
        found = (m_Clients[i] == client);
    }
    return found;
}

////////////////////////////////////////////////////////////////////////////////////////
// find Clients

CClient *CClients::FindClient(const CIp &Ip)
{
    CClient *client = NULL;
    
    // find client
    for ( int i = 0; (i < m_Clients.size()) && (client == NULL); i++ )
    {
        if ( m_Clients[i]->GetIp() == Ip )
        {
            client = m_Clients[i];
        }
    }
        
    // done
    return client;
}

CClient *CClients::FindClient(const CIp &Ip, int Protocol)
{
    CClient *client = NULL;
    
    // find client
    for ( int i = 0; (i < m_Clients.size()) && (client == NULL); i++ )
    {
        if ( (m_Clients[i]->GetIp() == Ip)  && (m_Clients[i]->GetProtocol() == Protocol))
        {
            client = m_Clients[i];
        }
    }
    
    // done
    return client;
}

CClient *CClients::FindClient(const CCallsign &Callsign, const CIp &Ip, int Protocol)
{
    CClient *client = NULL;
    
    // find client
    for ( int i = 0; (i < m_Clients.size()) && (client == NULL); i++ )
    {
        if ( m_Clients[i]->GetCallsign().HasSameCallsign(Callsign) &&
             (m_Clients[i]->GetIp() == Ip)  &&
             (m_Clients[i]->GetProtocol() == Protocol) )
        {
            client = m_Clients[i];
        }
    }
    
    // done
    return client;
}

CClient *CClients::FindClient(const CCallsign &Callsign, char module, const CIp &Ip, int Protocol)
{
    CClient *client = NULL;
    
    // find client
    for ( int i = 0; (i < m_Clients.size()) && (client == NULL); i++ )
    {
        if ( m_Clients[i]->GetCallsign().HasSameCallsign(Callsign) &&
             (m_Clients[i]->GetModule() == module) &&
             (m_Clients[i]->GetIp() == Ip)  &&
             (m_Clients[i]->GetProtocol() == Protocol) )
        {
            client = m_Clients[i];
        }
    }
    
    // done
    return client;
}

CClient *CClients::FindClient(const CCallsign &Callsign, int Protocol)
{
    CClient *client = NULL;
    
    // find client
    for ( int i = 0; (i < m_Clients.size()) && (client == NULL); i++ )
    {
        if ( (m_Clients[i]->GetProtocol() == Protocol) &&
             m_Clients[i]->GetCallsign().HasSameCallsign(Callsign) )
        {
            client = m_Clients[i];
        }
    }
    
    // done
    return client;
}


////////////////////////////////////////////////////////////////////////////////////////
// iterate on clients

CClient *CClients::FindNextClient(int Protocol, int *index)
{
    CClient *client = NULL;
    
    // find next client
    bool found = false;
    for ( int i = *index+1; (i < m_Clients.size()) && !found; i++ )
    {
        if ( m_Clients[i]->GetProtocol() == Protocol )
        {
            found = true;
            client = m_Clients[i];
            *index = i;
        }
    }
    return client;
}

CClient *CClients::FindNextClient(const CIp &Ip, int Protocol, int *index)
{
    CClient *client = NULL;
    
    // find next client
    bool found = false;
    for ( int i = *index+1; (i < m_Clients.size()) && !found; i++ )
    {
        if ( (m_Clients[i]->GetProtocol() == Protocol) &&
             (m_Clients[i]->GetIp() == Ip) )
        {
            found = true;
            client = m_Clients[i];
            *index = i;
        }
    }
    return client;
}

CClient *CClients::FindNextClient(const CCallsign &Callsign, const CIp &Ip, int Protocol, int *index)
{
    CClient *client = NULL;
    
    // find next client
    bool found = false;
    for ( int i = *index+1; (i < m_Clients.size()) && !found; i++ )
    {
        if ( (m_Clients[i]->GetProtocol() == Protocol) &&
            (m_Clients[i]->GetIp() == Ip) &&
            m_Clients[i]->GetCallsign().HasSameCallsign(Callsign) )
        {
            found = true;
            client = m_Clients[i];
            *index = i;
        }
    }
    return client;
}

