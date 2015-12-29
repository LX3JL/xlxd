//
//  cprotocols.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 01/11/2015.
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

#include "main.h"
#include "cdextraprotocol.h"
#include "cdplusprotocol.h"
#include "cdcsprotocol.h"
#include "cprotocols.h"


////////////////////////////////////////////////////////////////////////////////////////
// constructor

CProtocols::CProtocols()
{
    for ( int i = 0; i < m_Protocols.size(); i++ )
    {
        m_Protocols[i] = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// destructor

CProtocols::~CProtocols()
{
    m_Mutex.lock();
    {
        for ( int i = 0; i < m_Protocols.size(); i++ )
        {
           delete m_Protocols[i];
        }
    }
    m_Mutex.unlock();
}

////////////////////////////////////////////////////////////////////////////////////////
// initialization

bool CProtocols::Init(void)
{
    bool ok = true;
    
    m_Mutex.lock();
    {
        // create and initialize DEXTRA
        delete m_Protocols[0];
        m_Protocols[0] = new CDextraProtocol;
        ok &= m_Protocols[0]->Init();
        
        // create and initialize DPLUS
        delete m_Protocols[1];
        m_Protocols[1] = new CDplusProtocol;
        ok &= m_Protocols[1]->Init();
        
        // create and initialize DCS
        delete m_Protocols[2];
        m_Protocols[2] = new CDcsProtocol;
        ok &= m_Protocols[2]->Init();

    }
    m_Mutex.unlock();
   
    // done
    return ok;
}

void CProtocols::Close(void)
{
    m_Mutex.lock();
    {
        for ( int i = 0; i < m_Protocols.size(); i++ )
        {
            m_Protocols[i]->Close();
        }
    }
    m_Mutex.unlock();
}
