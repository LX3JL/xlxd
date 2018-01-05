//
//  cambeserver.cpp
//  ambed
//
//  Created by Jean-Luc Deltombe (LX3JL) on 15/04/2017.
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

#include "main.h"
#include "ctimepoint.h"
#include "ccontroller.h"
#include "cvocodecs.h"
#include "cambeserver.h"

////////////////////////////////////////////////////////////////////////////////////////

CAmbeServer g_AmbeServer;


////////////////////////////////////////////////////////////////////////////////////////
// constructor

CAmbeServer::CAmbeServer()
{
    m_bStopThreads = false;
    m_pThread = NULL;
#ifdef DEBUG_DUMPFILE
    m_DebugFile.open("/Users/jeanluc/Desktop/ambed.txt");
#endif
}

////////////////////////////////////////////////////////////////////////////////////////
// destructor

CAmbeServer::~CAmbeServer()
{
    m_bStopThreads = true;
    if ( m_pThread != NULL )
    {
        m_pThread->join();
        delete m_pThread;
    }
#ifdef DEBUG_DUMPFILE
    m_DebugFile.close();
#endif
}

////////////////////////////////////////////////////////////////////////////////////////
// operation

bool CAmbeServer::Start(void)
{
    bool ok = true;
    
    // init interfaces & controller
    std::cout << "Initializing vocodecs:" << std::endl;
    ok &= g_Vocodecs.Init();
    std::cout << std::endl;
    std::cout << "Initializing controller" << std::endl;
    ok &= m_Controller.Init();
    std::cout << std::endl;
    
    // if ok, start threads
    if ( ok )
    {
        //
        m_pThread = new std::thread(CAmbeServer::Thread, this);
    }
    
    // done
    return ok;
}

void CAmbeServer::Stop(void)
{
    // stop controller
    m_Controller.Close();
    
    // stop & delete all threads
    m_bStopThreads = true;
    
    // stop & delete report threads
    if ( m_pThread != NULL )
    {
        m_pThread->join();
        delete m_pThread;
        m_pThread = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// thread

void CAmbeServer::Thread(CAmbeServer *This)
{
    while ( !This->m_bStopThreads )
    {
        This->Task();
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// task

void CAmbeServer::Task(void)
{
    // and wait a bit
    CTimePoint::TaskSleepFor(10000);
}
