//
//  ccallsignlist.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 30/12/2015.
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
#include <fcntl.h>
#include <sys/stat.h>
#include "main.h"
#include "ccallsignlist.h"

////////////////////////////////////////////////////////////////////////////////////////
// constructor

CCallsignList::CCallsignList()
{
    m_Filename = NULL;
    ::memset(&m_LastModTime, 0, sizeof(CCallsignList));
}

////////////////////////////////////////////////////////////////////////////////////////
// file io

bool CCallsignList::LoadFromFile(const char *filename)
{
    bool ok = false;
    char sz[256];

    // and load
    std::ifstream file (filename);
    if ( file.is_open() )
    {
        Lock();

        // empty list
        clear();
        // fill with file content
        while ( file.getline(sz, sizeof(sz)).good()  )
        {
            // remove leading & trailing spaces
            char *szt = TrimWhiteSpaces(sz);
            // and load if not comment
            if ( (::strlen(szt) > 0) && (szt[0] != '#') )
            {
                push_back(CCallsignListItem(CCallsign(szt), CIp(), NULL));
            }
        }
        // close file
        file.close();

        // keep file path
        m_Filename = filename;

        // update time
        GetLastModTime(&m_LastModTime);

        // and done
        Unlock();
        ok = true;
        std::cout << "Gatekeeper loaded " << size() << " lines from " << filename <<  std::endl;
    }
    else
    {
        std::cout << "Gatekeeper cannot find " << filename <<  std::endl;
    }

    return ok;
}

bool CCallsignList::ReloadFromFile(void)
{
    bool ok = false;

    if ( m_Filename !=  NULL )
    {
        ok = LoadFromFile(m_Filename);
    }
    return ok;
}

bool CCallsignList::NeedReload(void)
{
    bool needReload = false;

    time_t time;
    if ( GetLastModTime(&time) )
    {
        needReload = time != m_LastModTime;
    }
    return needReload;
}

////////////////////////////////////////////////////////////////////////////////////////
// compare

bool CCallsignList::IsCallsignListed(const CCallsign &callsign) const
{
    bool listed = false;

    for ( int i =  0; (i < size()) && !listed; i++ )
    {
        listed = (data()[i]).HasSameCallsignWithWidlcard(callsign);
    }

    return listed;
}

bool CCallsignList::IsCallsignListed(const CCallsign &callsign, char module) const
{
    bool listed = false;
    
    for ( int i =  0; (i < size()) && !listed; i++ )
    {
        const CCallsignListItem *item = &(data()[i]);
        listed = (item->HasSameCallsign(callsign) && item->HasModuleListed(module));

    }
    
    return listed;
}

bool CCallsignList::IsCallsignListed(const CCallsign &callsign, char *modules) const
{
    bool listed = false;
    
    for ( int i =  0; (i < size()) && !listed; i++ )
    {
        const CCallsignListItem *item = &(data()[i]);
        listed = (item->HasSameCallsign(callsign) && item->CheckListedModules(modules));
        
    }
    
    return listed;
}

////////////////////////////////////////////////////////////////////////////////////////
// find

CCallsignListItem *CCallsignList::FindListItem(const CCallsign &Callsign)
{
    CCallsignListItem *item = NULL;
    
    // find client
    for ( int i = 0; (i < size()) && (item == NULL); i++ )
    {
        if ( (data()[i]).GetCallsign().HasSameCallsign(Callsign) )
        {
            item = &(data()[i]);
        }
    }
    
    // done
    return item;
    
}

////////////////////////////////////////////////////////////////////////////////////////
// helpers

char *CCallsignList::TrimWhiteSpaces(char *str)
{
  char *end;

  // Trim leading space & tabs
  while((*str == ' ') || (*str == '\t')) str++;

  // All spaces?
  if(*str == 0)
    return str;

  // Trim trailing space, tab or lf
  end = str + ::strlen(str) - 1;
  while((end > str) && ((*end == ' ') || (*end == '\t') || (*end == '\r'))) end--;

  // Write new null terminator
  *(end+1) = 0;

  return str;
}

bool CCallsignList::GetLastModTime(time_t *time)
{
    bool ok = false;

    if ( m_Filename != NULL )
    {
        struct stat fileStat;
        if( ::stat(m_Filename, &fileStat) != -1 )
        {
            *time = fileStat.st_mtime;
            ok = true;
        }
    }
    return ok;
}
