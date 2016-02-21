//
//  cclients.h
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

#ifndef cclients_h
#define cclients_h

#include "cclient.h"


////////////////////////////////////////////////////////////////////////////////////////
// define


////////////////////////////////////////////////////////////////////////////////////////
// class

class CClients
{
public:
    // constructors
    CClients();
    
    // destructors
    virtual ~CClients();
    
    // locks
    void Lock(void)                     { m_Mutex.lock(); }
    void Unlock(void)                   { m_Mutex.unlock(); }
    
    // manage Clients
    int     GetSize(void) const         { return (int)m_Clients.size(); }
    void    AddClient(CClient *);
    void    RemoveClient(CClient *);
    CClient *GetClient(int);
    bool    IsClient(CClient *) const;
    
    // find clients
    CClient *FindClient(const CIp &);
    CClient *FindClient(const CIp &, int);
    CClient *FindClient(const CCallsign &, const CIp &, int);
    CClient *FindClient(const CCallsign &, char, const CIp &, int);
    CClient *FindClient(const CCallsign &, int);
    
    // iterate on clients
    CClient *FindNextClient(int, int*);
    CClient *FindNextClient(const CIp &, int, int *);
    CClient *FindNextClient(const CCallsign &, const CIp &, int, int *);

protected:
    // data
    std::mutex               m_Mutex;
    std::vector<CClient *>   m_Clients;
};


////////////////////////////////////////////////////////////////////////////////////////
#endif /* cclients_h */
