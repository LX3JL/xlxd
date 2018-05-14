//
//  cdmriddir.cpp
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

#include <string.h>
#include "main.h"
#include "creflector.h"
#include "cdmriddir.h"
#include "cdmriddirfile.h"
#include "cdmriddirhttp.h"

////////////////////////////////////////////////////////////////////////////////////////
// constructor & destructor

CDmridDir::CDmridDir()
{
    m_bStopThread = false;
    m_pThread = NULL;
}

CDmridDir::~CDmridDir()
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

bool CDmridDir::Init(void)
{
	// load content
	Reload();
	
    // reset stop flag
    m_bStopThread = false;
    
    // start  thread;
    m_pThread = new std::thread(CDmridDir::Thread, this);

    return true;
}

void CDmridDir::Close(void)
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

void CDmridDir::Thread(CDmridDir *This)
{
    while ( !This->m_bStopThread )
    {
        // Wait 30 seconds
        CTimePoint::TaskSleepFor(DMRIDDB_REFRESH_RATE * 60000);

        // have lists files changed ?
        if ( This->NeedReload() )
        {
           	This->Reload();
        }
     }
}

////////////////////////////////////////////////////////////////////////////////////////
// Reload

bool CDmridDir::Reload(void)
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

const CCallsign *CDmridDir::FindCallsign(uint32 dmrid)
{
    auto found = m_CallsignMap.find(dmrid);
    if ( found != m_CallsignMap.end() )
    {
        return &(found->second);
    }
    return NULL;
}

uint32 CDmridDir::FindDmrid(const CCallsign &callsign)
{
    auto found = m_DmridMap.find(callsign);
    if ( found != m_DmridMap.end() )
    {
        return (found->second);
    }
    return 0;
}


////////////////////////////////////////////////////////////////////////////////////////
// syntax helpers

bool CDmridDir::IsValidDmrid(const char *sz)
{
    bool ok = false;
    size_t n = ::strlen(sz);
    if ( (n > 0) && (n <= 8) )
    {
        ok = true;
        for ( size_t i = 0; (i < n) && ok; i++ )
        {
            ok &= ::isdigit(sz[i]);
        }
    }
    return ok;
}

