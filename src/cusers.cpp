//
//  cusers.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 13/11/2015.
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
#include "cusers.h"
#include "creflector.h"

////////////////////////////////////////////////////////////////////////////////////////
// constructor

CUsers::CUsers()
{
    m_Users.reserve(LASTHEARD_USERS_MAX_SIZE);
}

////////////////////////////////////////////////////////////////////////////////////////
// users management

void CUsers::AddUser(const CUser &user)
{
    // add
    m_Users.push_back(user);
   
    // sort list by descending time (fisrt is youngest)
    std::sort(m_Users.begin(), m_Users.end());
    
    // if list size too big, remove oldest
    if ( m_Users.size() >= (LASTHEARD_USERS_MAX_SIZE-1) )
    {
        m_Users.resize(m_Users.size()-1);
    }

    // notify
    g_Reflector.OnUsersChanged();
}

////////////////////////////////////////////////////////////////////////////////////////
// operation

void CUsers::Hearing(const CCallsign &my, const CCallsign &rpt1, const CCallsign &rpt2)
{
    Hearing(my, rpt1, rpt2, g_Reflector.GetCallsign());
}

void CUsers::Hearing(const CCallsign &my, const CCallsign &rpt1, const CCallsign &rpt2, const CCallsign &xlx)
{
    CUser heard(my, rpt1, rpt2, xlx);
    
    // first check if we have this user listed yet
    bool found = false;
    for ( int i = 0; (i < m_Users.size()) && !found; i++ )
    {
        found = (m_Users[i] == heard);
        if ( found )
        {
            m_Users[i].HeardNow();
        }
    }
    
    // if not found, add user to list
    // otherwise just re-sort the list
    if ( !found )
    {
        AddUser(heard);
    }
    else
    {
        std::sort(m_Users.begin(), m_Users.end());
    }
}

