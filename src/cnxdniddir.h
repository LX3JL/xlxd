//
//  cnxdniddir.h
//  xlxd
//
//  Created for NXDN Reflector peering support
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

#ifndef cnxdniddir_h
#define cnxdniddir_h

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "cbuffer.h"
#include "ccallsign.h"

// compare function for std::map::find

struct CNxdnIdDirCallsignCompare
{
    bool operator() (const CCallsign &cs1, const CCallsign &cs2) const
    { return cs1.HasLowerCallsign(cs2);}
};


////////////////////////////////////////////////////////////////////////////////////////

class CNxdnIdDir
{
public:
    // constructor
    CNxdnIdDir();

    // destructor
    virtual ~CNxdnIdDir();

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
    const CCallsign *FindCallsign(uint16_t);
    uint16_t FindNxdnId(const CCallsign &);

protected:
    // thread
    static void Thread(CNxdnIdDir *);

    // reload helpers
    bool Reload(void);
    virtual bool NeedReload(void)                    { return false; }
    bool IsValidNxdnId(const char *);

protected:
    // data
    std::map <uint16_t, CCallsign> m_CallsignMap;
    std::map <CCallsign, uint16_t, CNxdnIdDirCallsignCompare> m_NxdnIdMap;

    // Lock()
    std::mutex          m_Mutex;

    // thread
    bool                m_bStopThread;
    std::thread         *m_pThread;

};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cnxdniddir_h */
