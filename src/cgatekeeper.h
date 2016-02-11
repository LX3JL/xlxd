//
//  cgatekeeper.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 07/11/2015.
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

#ifndef cgatekeeper_h
#define cgatekeeper_h

#include "main.h"
#include "ccallsign.h"
#include "cip.h"
#include "ccallsignlist.h"
#include "cpeercallsignlist.h"

////////////////////////////////////////////////////////////////////////////////////////
// class

class CGateKeeper
{
public:
    // constructor
    CGateKeeper();
    
    // destructor
    virtual ~CGateKeeper();
    
    // init & clode
    bool Init(void);
    void Close(void);
    
    // authorizations
    bool MayLink(const CCallsign &, const CIp &, int, char * = NULL) const;
    bool MayTransmit(const CCallsign &, const CIp &, int = PROTOCOL_ANY, char = ' ') const;
    
    // peer list handeling
    CPeerCallsignList *GetPeerList(void)    { m_PeerList.Lock(); return &m_PeerList; }
    void ReleasePeerList(void)              { m_PeerList.Unlock(); }
    
protected:
    // thread
    static void Thread(CGateKeeper *);

    // operation helpers
    bool IsNodeListedOk(const CCallsign &, const CIp &) const;
    bool IsPeerListedOk(const CCallsign &, const CIp &, char) const;
    bool IsPeerListedOk(const CCallsign &, const CIp &, char *) const;
    
protected:
    // data
    CCallsignList       m_NodeWhiteList;
    CCallsignList       m_NodeBlackList;
    CPeerCallsignList   m_PeerList;
    
    // thread
    bool                m_bStopThread;
    std::thread         *m_pThread;
};


////////////////////////////////////////////////////////////////////////////////////////
#endif /* cgatekeeper_h */
