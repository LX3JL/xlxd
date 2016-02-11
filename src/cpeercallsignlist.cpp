//
//  cxlxcallsignlist.cpp
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
#include "cpeercallsignlist.h"


////////////////////////////////////////////////////////////////////////////////////////
// file io

bool CPeerCallsignList::LoadFromFile(const char *filename)
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
            
            // crack it
            if ( (::strlen(szt) > 0) && (szt[0] != '#') )
            {
                // 1st token is callsign
                if ( (szt = ::strtok(szt, " ,\t")) != NULL )
                {
                    CCallsign callsign(szt);
                    // 2nd token is ip
                    if ( (szt = ::strtok(NULL, " ,\t")) != NULL )
                    {
                        CIp ip(szt);
                        // 3rd token is modules list
                        if ( (szt = ::strtok(NULL, " ,\t")) != NULL )
                        {
                            // and load
                            push_back(CCallsignListItem(callsign, ip, szt));
                        }
                    }
                }
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


