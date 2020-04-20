//
//  cdmriddirspecial.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 29/12/2017.
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
#include "cdmriddirfile.h"
#include "cdmriddirhttp.h"
#include "cdmriddirspecial.h"


#if (EXTENDED_DMRID_CHECKS == 1)
CDmridDirSpecial g_DmridDirSpecial;
#endif

////////////////////////////////////////////////////////////////////////////////////////
// constructor & destructor

CDmridDirSpecial::CDmridDirSpecial()
{
}

CDmridDirSpecial::~CDmridDirSpecial()
{
}

////////////////////////////////////////////////////////////////////////////////////////
// init & close

bool CDmridDirSpecial::Init(void)
{
    Reload();
    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////
// Reload

bool CDmridDirSpecial::Reload(void)
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
// refresh

bool CDmridDirSpecial::LoadContent(CBuffer *buffer)
{
    bool ok = false;
    std::ifstream file;
    std::streampos size;
    
    // open file
    file.open(SPECIALIDDB_PATH, std::ios::in | std::ios::binary | std::ios::ate);
    if ( file.is_open() )
    {
        // read file
        size = file.tellg();
        if ( size > 0 )
        {
            // read file into buffer
            buffer->resize((int)size+1);
            file.seekg (0, std::ios::beg);
            file.read((char *)buffer->data(), (int)size);
            
            // close file
            file.close();
            
            // update time
            GetLastModTime(&m_LastModTime);
            
            // done
            ok = true;
        }
    }
    
    // done
    return ok;
}

bool CDmridDirSpecial::RefreshContent(const CBuffer &buffer)
{
    bool ok = false;
    int count = 0;
    
    // scan buffer
    if ( buffer.size() > 0 )
    {
        // crack it
        char *ptr1 = (char *)buffer.data();
        char *ptr2;
        
        // get next line
        while ( (ptr2 = ::strchr(ptr1, '\n')) != NULL )
        {
            *ptr2 = 0;
            // get items
            char *dmrid;
            char *callsign;
            //std::cout << "Read callsign: " << &callsign << " ID: " << &dmrid << std::endl;
            if ( ((dmrid = ::strtok(ptr1, ";")) != NULL) && g_DmridDir.IsValidDmrid(dmrid) )
            {
                if ( ((callsign = ::strtok(NULL, ";")) != NULL) )
                {
                    // new entry
                    uint32 ui = atoi(dmrid);
                    CCallsign cs(callsign, ui);
                    if ( cs.IsValid() )
                    {
                        g_DmridDir.m_CallsignMap.insert(std::pair<uint32,CCallsign>(ui, cs));
                        g_DmridDir.m_DmridMap.insert(std::pair<CCallsign,uint32>(cs,ui));
                        count++;
                    }
                }
            }
            // next line
            ptr1 = ptr2+1;
        }
        
        // done
        ok = true;
    }
    
    // report
    std::cout << "Read " << count << " special ids from file " << SPECIALIDDB_PATH << std::endl;
    
    // done
    return ok;
}


bool CDmridDirSpecial::GetLastModTime(time_t *time)
{
    bool ok = false;
    
    struct stat fileStat;
    if( ::stat(SPECIALIDDB_PATH, &fileStat) != -1 )
    {
        *time = fileStat.st_mtime;
        ok = true;
    }
    return ok;
}
