//
//  cysfnodedirfile.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 08/10/2019.
//  Copyright © 2019 Jean-Luc Deltombe (LX3JL). All rights reserved.
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
#include "cysfnodedirfile.h"


#if (YSFNODEDB_USE_RLX_SERVER == 0)
CYsfNodeDirFile   g_YsfNodeDir;
#endif

////////////////////////////////////////////////////////////////////////////////////////
// constructor & destructor

CYsfNodeDirFile::CYsfNodeDirFile()
{
    ::memset(&m_LastModTime, 0, sizeof(time_t));
}

////////////////////////////////////////////////////////////////////////////////////////
// init & close

bool CYsfNodeDirFile::Init(void)
{
    return CYsfNodeDir::Init();
}

////////////////////////////////////////////////////////////////////////////////////////
// refresh

bool CYsfNodeDirFile::NeedReload(void)
{
    bool needReload = false;
    
    time_t time;
    if ( GetLastModTime(&time) )
    {
        needReload = time != m_LastModTime;
    }
    return needReload;
}

bool CYsfNodeDirFile::LoadContent(CBuffer *buffer)
{
    bool ok = false;
    std::ifstream file;
    std::streampos size;
    
    // open file
    file.open(YSFNODEDB_PATH, std::ios::in | std::ios::binary | std::ios::ate);
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

bool CYsfNodeDirFile::RefreshContent(const CBuffer &buffer)
{
    bool ok = false;
    
    // clear directory
    clear();
    
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
            char *callsign;
            char *txfreq;
            char *rxfreq;
            if ( ((callsign = ::strtok(ptr1, ";")) != NULL) )
            {
                if ( ((txfreq = ::strtok(NULL, ";")) != NULL) )
                {
                    if ( ((rxfreq = ::strtok(NULL, ";")) != NULL) )
                    {
                        // new entry
                        CCallsign cs(callsign);
                        CYsfNode node(cs, atoi(txfreq), atoi(rxfreq));
                        if ( cs.IsValid() && node.IsValid() )
                        {
                            insert(std::pair<CCallsign, CYsfNode>(cs, node));
                        }
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
    std::cout << "Read " << size() << " YSF nodes from file " << YSFNODEDB_PATH << std::endl;
    
    // done
    return ok;
}


bool CYsfNodeDirFile::GetLastModTime(time_t *time)
{
    bool ok = false;
    
    struct stat fileStat;
    if( ::stat(YSFNODEDB_PATH, &fileStat) != -1 )
    {
        *time = fileStat.st_mtime;
        ok = true;
    }
    return ok;
}
