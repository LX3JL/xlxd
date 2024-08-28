//
//  cwiresxpacketqueue.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 09/10/2019.
//  Copyright © 2019 Jean-Luc Deltombe (LX3JL). All rights reserved.
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

#ifndef cwiresxpacketqueue_h
#define cwiresxpacketqueue_h

#include "cwiresxpacket.h"

////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////
// class

class CWiresxPacketQueue : public std::queue<CWiresxPacket>
{
public:
    // constructor
    CWiresxPacketQueue() {};
    
    // destructor
    ~CWiresxPacketQueue() {};
    
    // lock
    void Lock()                 { m_Mutex.lock(); }
    void Unlock()               { m_Mutex.unlock(); }
    
protected:
    // status
    std::mutex  m_Mutex;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cwiresxpacketqueue_h */
