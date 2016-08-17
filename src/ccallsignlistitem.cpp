//
//  ccallsignlistitem.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 31/01/2016.
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
#include "ccallsignlistitem.h"

////////////////////////////////////////////////////////////////////////////////////////
// constructor

CCallsignListItem::CCallsignListItem()
{
    ::memset(m_Modules, 0, sizeof(m_Modules));
    ::memset(m_szUrl, 0, sizeof(m_szUrl));
}

CCallsignListItem::CCallsignListItem(const CCallsign &callsign, const CIp &ip, const char *modules)
{
    m_Callsign = callsign;
    ::memset(m_szUrl, 0, sizeof(m_szUrl));
    m_Ip = ip;
    if ( modules != NULL )
    {
        :: memset(m_Modules, 0, sizeof(m_Modules));
        ::memcpy(m_Modules, modules, MIN(strlen(modules), sizeof(m_Modules)-1));
    }
}

CCallsignListItem::CCallsignListItem(const CCallsign &callsign, const char *url, const char *modules)
{
    m_Callsign = callsign;
    ::strncpy(m_szUrl, url, URL_MAXLEN);
    m_Ip = CIp(m_szUrl);
    if ( modules != NULL )
    {
        :: memset(m_Modules, 0, sizeof(m_Modules));
        ::memcpy(m_Modules, modules, MIN(strlen(modules), sizeof(m_Modules)-1));
    }
}

CCallsignListItem::CCallsignListItem(const CCallsignListItem &item)
{
    m_Callsign = item.m_Callsign;
    ::memcpy(m_szUrl, item.m_szUrl, sizeof(m_szUrl));
    m_Ip = item.m_Ip;
    ::memcpy(m_Modules, item.m_Modules, sizeof(m_Modules));
}


////////////////////////////////////////////////////////////////////////////////////////
// compare

bool CCallsignListItem::HasSameCallsign(const CCallsign &callsign) const
{
    return m_Callsign.HasSameCallsign(callsign);
}

bool CCallsignListItem::HasSameCallsignWithWidlcard(const CCallsign &callsign) const
{
    return m_Callsign.HasSameCallsignWithWidlcard(callsign);
}

bool CCallsignListItem::HasModuleListed(char module) const
{
    return (::strchr(m_Modules, (int)module) != NULL);
}

bool CCallsignListItem::CheckListedModules(char *Modules) const
{
    bool listed = false;
    
    if ( Modules != NULL )
    {
        // build a list of common modules
        char list[NB_MODULES_MAX+1];
        list[0] = 0;
        //
        for ( int i = 0; i < ::strlen(Modules); i++ )
        {
            if ( HasModuleListed(Modules[i]) )
            {
                ::strncat(list, &(Modules[i]), 1);
                listed = true;
            }
        }
        ::strcpy(Modules, list);
    }
    return listed;
}

