//
//  cdmriddirhttp.cpp
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
#include "main.h"
#include "creflector.h"
#include "cdmriddirhttp.h"

#if (DMRIDDB_USE_RLX_SERVER == 1)
CDmridDirHttp g_DmridDir;
#endif




////////////////////////////////////////////////////////////////////////////////////////
// refresh

bool CDmridDirHttp::LoadContent(CBuffer *buffer)
{
    // get file from xlxapi server
    return HttpGet("xlxapi.rlx.lu", "api/exportdmr.php", 80, buffer);
}

bool CDmridDirHttp::RefreshContent(const CBuffer &buffer)
{
    bool ok = false;

    // clear directory
    m_CallsignMap.clear();
    m_DmridMap.clear();
    
    // scan file
    if ( buffer.size() > 0 )
    {
        char *ptr1 = (char *)buffer.data();
        char *ptr2;
        // get next line
        while ( (ptr2 = ::strchr(ptr1, '\n')) != NULL )
        {
            *ptr2 = 0;
            // get items
            char *dmrid;
            char *callsign;
            if ( ((dmrid = ::strtok(ptr1, ";")) != NULL) && IsValidDmrid(dmrid) )
            {
                if ( ((callsign = ::strtok(NULL, ";")) != NULL) )
                {
                    // new entry
                    uint32 ui = atoi(dmrid);
                    CCallsign cs(callsign, ui);
                    if ( cs.IsValid() )
                    {
                        m_CallsignMap.insert(std::pair<uint32,CCallsign>(ui, cs));
                        m_DmridMap.insert(std::pair<CCallsign,uint32>(cs,ui));
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
    std::cout << "Read " << m_DmridMap.size() << " DMR id from xlxapi.rlx.lu database " << std::endl;
    
    // done
    return ok;
}


////////////////////////////////////////////////////////////////////////////////////////
// httpd helpers

#define DMRID_HTTPGET_SIZEMAX       (256)
#define DMRID_TEXTFILE_SIZEMAX      (10*1024*1024)

bool CDmridDirHttp::HttpGet(const char *hostname, const char *filename, int port, CBuffer *buffer)
{
    bool ok = false;
    int sock_id;
    
    // open socket
    if ( (sock_id = ::socket(AF_INET, SOCK_STREAM, 0)) >= 0 )
    {
        // get hostname address
        struct sockaddr_in servaddr;
        struct hostent *hp;
        ::memset(&servaddr,0,sizeof(servaddr));
        if( (hp = gethostbyname(hostname)) != NULL )
        {
            // dns resolved
            ::memcpy((char *)&servaddr.sin_addr.s_addr, (char *)hp->h_addr, hp->h_length);
            servaddr.sin_port = htons(port);
            servaddr.sin_family = AF_INET;
            
            // connect
            if ( ::connect(sock_id, (struct sockaddr *)&servaddr, sizeof(servaddr)) == 0)
            {
                // send the GET request
                char request[DMRID_HTTPGET_SIZEMAX];
                ::sprintf(request, "GET /%s HTTP/1.0\r\nFrom: %s\r\nUser-Agent: xlxd\r\n\r\n",
                          filename, (const char *)g_Reflector.GetCallsign());
                ::write(sock_id, request, strlen(request));
                
                // config receive timeouts
                fd_set read_set;
                struct timeval timeout;
                timeout.tv_sec = 5;
                timeout.tv_usec = 0;
                FD_ZERO(&read_set);
                FD_SET(sock_id, &read_set);
                
                // get the reply back
                buffer->clear();
                bool done = false;
                do
                {
                    char buf[1440];
                    ssize_t len = 0;
                    select(sock_id+1, &read_set, NULL, NULL, &timeout);
                    //if ( (ret > 0) || ((ret < 0) && (errno == EINPROGRESS)) )
                    //if ( ret >= 0 )
                    //{
                    usleep(5000);
                    len = read(sock_id, buf, 1440);
                    if ( len > 0 )
                    {
                        buffer->Append((uint8 *)buf, (int)len);
                        ok = true;
                    }
                    //}
                    done = (len <= 0);
                    
                } while (!done);
                buffer->Append((uint8)0);
                
                // and disconnect
                close(sock_id);
            }
            else
            {
                std::cout << "Cannot establish connection with host " << hostname << std::endl;
            }
        }
        else
        {
            std::cout << "Host " << hostname << " not found" << std::endl;
        }
        
    }
    else
    {
        std::cout << "Failed to open wget socket" << std::endl;
    }
    
    // done
    return ok;
}
