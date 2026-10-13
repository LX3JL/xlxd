//
//  cnxdniddir.cpp
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

#include <string.h>
#include "main.h"
#include "creflector.h"
#include "cnxdniddir.h"

////////////////////////////////////////////////////////////////////////////////////////
// constructor & destructor

CNxdnIdDir::CNxdnIdDir()
{
    m_bStopThread = false;
    m_pThread = NULL;
}

CNxdnIdDir::~CNxdnIdDir()
{
    // kill threads
    m_bStopThread = true;
    if ( m_pThread != NULL )
    {
        m_pThread->join();
        delete m_pThread;
    }
}


////////////////////////////////////////////////////////////////////////////////////////
// init & close

bool CNxdnIdDir::Init(void)
{
    // load content
    Reload();

    // reset stop flag
    m_bStopThread = false;

    // start  thread;
    m_pThread = new std::thread(CNxdnIdDir::Thread, this);

    return true;
}

void CNxdnIdDir::Close(void)
{
    m_bStopThread = true;
    if ( m_pThread != NULL )
    {
        m_pThread->join();
        delete m_pThread;
        m_pThread = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// thread

void CNxdnIdDir::Thread(CNxdnIdDir *This)
{
    while ( !This->m_bStopThread )
    {
        // Wait for refresh period (in minutes, converted to ms)
        CTimePoint::TaskSleepFor(NXDNIDDB_REFRESH_RATE * 60000);

        // have lists files changed ?
        if ( This->NeedReload() )
        {
            This->Reload();
        }
     }
}

////////////////////////////////////////////////////////////////////////////////////////
// Reload

bool CNxdnIdDir::Reload(void)
{
    CBuffer buffer;
    bool ok = false;

    if ( LoadContent(&buffer) )
    {
        Lock();
        {
            ok = RefreshContent(buffer);
        }
        Unlock();
    }
    return ok;
}


////////////////////////////////////////////////////////////////////////////////////////
// find

const CCallsign *CNxdnIdDir::FindCallsign(uint16_t nxdnid)
{
    auto found = m_CallsignMap.find(nxdnid);
    if ( found != m_CallsignMap.end() )
    {
        return &(found->second);
    }
    return NULL;
}

uint16_t CNxdnIdDir::FindNxdnId(const CCallsign &callsign)
{
    auto found = m_NxdnIdMap.find(callsign);
    if ( found != m_NxdnIdMap.end() )
    {
        return (found->second);
    }
    return 0;
}


////////////////////////////////////////////////////////////////////////////////////////
// syntax helpers

bool CNxdnIdDir::IsValidNxdnId(const char *sz)
{
    bool ok = false;
    size_t n = ::strlen(sz);
    // NXDN IDs are 1-65535 (max 5 digits)
    if ( (n > 0) && (n <= 5) )
    {
        ok = true;
        for ( size_t i = 0; (i < n) && ok; i++ )
        {
            ok = ::isdigit(sz[i]);
        }
        // Also validate range
        if ( ok )
        {
            int val = ::atoi(sz);
            ok = (val >= 1) && (val <= 65535);
        }
    }
    return ok;
}
