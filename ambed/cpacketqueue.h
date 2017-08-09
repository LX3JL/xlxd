//
//  cpacketqueue.h
//  ambed
//
//  Created by Jean-Luc Deltombe (LX3JL) on 24/04/2017.
//  Copyright © 2015 Jean-Luc Deltombe (LX3JL). All rights reserved.
//
// ----------------------------------------------------------------------------
//    This file is part of ambed.
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

#ifndef cpacketqueue_h
#define cpacketqueue_h

#include "cpacket.h"

////////////////////////////////////////////////////////////////////////////////////////
// class

class CPacketQueue : public std::queue<CPacket *>
{
public:
    // constructor
    CPacketQueue();
    
    // destructor
    ~CPacketQueue();
    
    // lock
    void Lock()                 { m_Mutex.lock(); }
    void Unlock()               { m_Mutex.unlock(); }
    
    // purge
    void Purge(void);
    
protected:
    // status
    std::mutex  m_Mutex;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cpacketqueue_h */
