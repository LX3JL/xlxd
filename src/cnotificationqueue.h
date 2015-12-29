//
//  cnotificationqueue.h
//  xlxd
//
//  Created by Jean-Luc on 05/12/2015.
//  Copyright © 2015 Jean-Luc. All rights reserved.
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



#ifndef cnotificationqueue_h
#define cnotificationqueue_h

#include "cnotification.h"


////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////
// class

class CNotificationQueue : public std::queue<CNotification>
{
public:
    // constructor
    CNotificationQueue() {};
    
    // destructor
    ~CNotificationQueue() {};
    
    // lock
    void Lock()                 { m_Mutex.lock(); }
    void Unlock()               { m_Mutex.unlock(); }
    
protected:
    // data
    std::mutex  m_Mutex;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cnotificationqueue_h */
