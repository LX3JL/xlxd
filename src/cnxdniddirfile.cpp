//
//  cnxdniddirfile.cpp
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
#include <fcntl.h>
#include <sys/stat.h>
#include "main.h"
#include "cnxdniddirfile.h"


#if (NXDNIDDB_USE_RLX_SERVER == 0)
CNxdnIdDirFile g_NxdnIdDir;
#endif

////////////////////////////////////////////////////////////////////////////////////////
// constructor & destructor

CNxdnIdDirFile::CNxdnIdDirFile()
{
    ::memset(&m_LastModTime, 0, sizeof(time_t));
}


////////////////////////////////////////////////////////////////////////////////////////
// init & close

bool CNxdnIdDirFile::Init(void)
{
    return CNxdnIdDir::Init();
}

////////////////////////////////////////////////////////////////////////////////////////
// refresh

bool CNxdnIdDirFile::NeedReload(void)
{
    bool needReload = false;

    time_t time;
    if ( GetLastModTime(&time) )
    {
        needReload = time != m_LastModTime;
    }
    return needReload;
}

bool CNxdnIdDirFile::LoadContent(CBuffer *buffer)
{
    bool ok = false;
    std::ifstream file;
    std::streampos size;

    // open file
    file.open(NXDNIDDB_PATH, std::ios::in | std::ios::binary | std::ios::ate);
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

bool CNxdnIdDirFile::RefreshContent(const CBuffer &buffer)
{
    bool ok = false;
    bool firstLine = true;

    // clear directory
    m_CallsignMap.clear();
    m_NxdnIdMap.clear();

    // scan buffer
    // CSV format: RADIO_ID,CALLSIGN,FIRST_NAME,LAST_NAME,CITY,STATE,COUNTRY
    if ( buffer.size() > 0 )
    {
        // Make a working copy since strtok modifies the string
        char *workbuf = new char[buffer.size() + 1];
        ::memcpy(workbuf, buffer.data(), buffer.size());
        workbuf[buffer.size()] = '\0';

        char *ptr1 = workbuf;
        char *ptr2;

        // get next line
        while ( (ptr2 = ::strchr(ptr1, '\n')) != NULL )
        {
            *ptr2 = 0;

            // Remove carriage return if present
            if ( ptr2 > ptr1 && *(ptr2-1) == '\r' )
            {
                *(ptr2-1) = 0;
            }

            // Skip header line
            if ( firstLine )
            {
                firstLine = false;
                ptr1 = ptr2 + 1;
                continue;
            }

            // Parse CSV line: RADIO_ID,CALLSIGN,...
            // Make a copy of the line for parsing
            char linebuf[256];
            ::strncpy(linebuf, ptr1, sizeof(linebuf)-1);
            linebuf[sizeof(linebuf)-1] = '\0';

            char *nxdnid_str = ::strtok(linebuf, ",");
            char *callsign_str = ::strtok(NULL, ",");

            if ( nxdnid_str != NULL && callsign_str != NULL && IsValidNxdnId(nxdnid_str) )
            {
                uint16_t nxdnid = (uint16_t)::atoi(nxdnid_str);
                CCallsign cs(callsign_str);

                if ( cs.IsValid() && nxdnid > 0 )
                {
                    m_CallsignMap.insert(std::pair<uint16_t,CCallsign>(nxdnid, cs));
                    m_NxdnIdMap.insert(std::pair<CCallsign,uint16_t>(cs, nxdnid));
                }
            }

            // next line
            ptr1 = ptr2 + 1;
        }

        delete[] workbuf;

        // done
        ok = true;
    }

    // report
    std::cout << "Read " << m_NxdnIdMap.size() << " NXDN ids from file " << NXDNIDDB_PATH << std::endl;

    // done
    return ok;
}


bool CNxdnIdDirFile::GetLastModTime(time_t *time)
{
    bool ok = false;

    struct stat fileStat;
    if( ::stat(NXDNIDDB_PATH, &fileStat) != -1 )
    {
        *time = fileStat.st_mtime;
        ok = true;
    }
    return ok;
}
