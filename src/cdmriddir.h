//
//  cdmriddir.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 08/10/2016.
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

#ifndef cdmriddir_h
#define cdmriddir_h

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "cbuffer.h"
#include "ccallsign.h"

// compare function for std::map::find

struct CallsignCompare
{
    bool operator() (const CCallsign &cs1, const CCallsign &cs2) const
    { return cs1.HasLowerCallsign(cs2);}
};


////////////////////////////////////////////////////////////////////////////////////////

class CDmridDir
{
public:
    // constructor
    CDmridDir();
    
    // destructor
    ~CDmridDir();
    
    // init & close
    virtual bool Init(void);
    virtual void Close(void);
    
    // locks
    void Lock(void)                                 { m_Mutex.lock(); }
    void Unlock(void)                               { m_Mutex.unlock(); }
    
    // refresh
    virtual bool LoadContent(CBuffer *)             { return false; }
    virtual bool RefreshContent(const CBuffer &)    { return false; }
    
    // find
    const CCallsign *FindCallsign(uint32);
    uint32 FindDmrid(const CCallsign &);
    
protected:
    // thread
    static void Thread(CDmridDir *);

    // reload helpers
    bool Reload(void);
    virtual bool NeedReload(void)                    { return false; }
    bool IsValidDmrid(const char *);
    
protected:
	// data
    std::map <uint32, CCallsign> m_CallsignMap;
    std::map <CCallsign, uint32, CallsignCompare> m_DmridMap;
    
    // Lock()
    std::mutex          m_Mutex;
           
    // thread
    bool                m_bStopThread;
    std::thread         *m_pThread;

};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cdmriddir_h */
