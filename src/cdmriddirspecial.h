//
//  cdmrididirspecial.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 29/12/2017.
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

#ifndef cdmrididirspecial_h
#define cdmrididirspecial_h

#include "cdmriddir.h"

////////////////////////////////////////////////////////////////////////////////////////

class CDmridDirSpecial
{
public:
    // constructor
    CDmridDirSpecial();
    
    // destructor
    ~CDmridDirSpecial();
    
    // init & close
    bool Init(void);

    // locks
    void Lock(void)                                 { m_Mutex.lock(); }
    void Unlock(void)                               { m_Mutex.unlock(); }

    // refresh
    bool LoadContent(CBuffer *);
    bool RefreshContent(const CBuffer &);
    
protected:
    // reload helpers
    bool Reload(void);
    bool GetLastModTime(time_t *);
    
protected:
    // data
    time_t      m_LastModTime;

    // Lock()
    std::mutex          m_Mutex;
 };

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cdmrididirspecial_h */
