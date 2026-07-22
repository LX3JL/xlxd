//
//  cysfnodedirhttp.cpp
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
#include <sys/time.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include <chrono>
#include "main.h"
#include "creflector.h"
#include "cysfnodedirhttp.h"

#if (YSFNODEDB_USE_RLX_SERVER == 1)
CYsfNodeDirHttp g_YsfNodeDir;
#endif


////////////////////////////////////////////////////////////////////////////////////////
// refresh

bool CYsfNodeDirHttp::LoadContent(CBuffer *buffer)
{
    // get file from xlxapi server
    return HttpGet("xlxapi.rlx.lu", "api/exportysfrepeaters.php", 80, buffer);
}

bool CYsfNodeDirHttp::RefreshContent(const CBuffer &buffer)
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
    std::cout << "Read " << size() << " YSF nodes from xlxapi.rlx.lu database " << std::endl;
    
    // done
    return ok;
}


////////////////////////////////////////////////////////////////////////////////////////
// httpd helpers

#define YSFNODE_HTTPGET_SIZEMAX        (256)
#define HTTPGET_CONNECT_TIMEOUT_SEC    (10)
#define HTTPGET_RW_TIMEOUT_SEC         (5)
#define HTTPGET_TOTAL_TIMEOUT_SEC      (60)

// Non-blocking connect with a bounded timeout. See cdmriddirhttp.cpp for
// rationale — same helper duplicated here to keep this translation unit
// self-contained.
static bool YsfHttpConnectWithTimeout(int sock, const struct sockaddr *addr, socklen_t addrlen, int timeoutSec)
{
    int flags = ::fcntl(sock, F_GETFL, 0);
    if ( flags < 0 ) return false;
    if ( ::fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0 ) return false;

    int rc = ::connect(sock, addr, addrlen);
    if ( rc == 0 )
    {
        ::fcntl(sock, F_SETFL, flags);
        return true;
    }
    if ( errno != EINPROGRESS )
    {
        ::fcntl(sock, F_SETFL, flags);
        return false;
    }

    fd_set wset;
    FD_ZERO(&wset);
    FD_SET(sock, &wset);
    struct timeval tv;
    tv.tv_sec = timeoutSec;
    tv.tv_usec = 0;
    rc = ::select(sock + 1, NULL, &wset, NULL, &tv);
    if ( rc <= 0 )
    {
        ::fcntl(sock, F_SETFL, flags);
        return false;
    }

    int soerr = 0;
    socklen_t slen = sizeof(soerr);
    if ( ::getsockopt(sock, SOL_SOCKET, SO_ERROR, &soerr, &slen) < 0 || soerr != 0 )
    {
        ::fcntl(sock, F_SETFL, flags);
        return false;
    }

    ::fcntl(sock, F_SETFL, flags);
    return true;
}

bool CYsfNodeDirHttp::HttpGet(const char *hostname, const char *filename, int port, CBuffer *buffer)
{
    bool ok = false;
    int sock_id;

    if ( (sock_id = ::socket(AF_INET, SOCK_STREAM, 0)) < 0 )
    {
        std::cout << "Failed to open wget socket" << std::endl;
        return false;
    }

    // Per-operation send/receive timeouts bound any single read()/write()
    // call so a stalled peer cannot wedge this thread forever.
    struct timeval rwto;
    rwto.tv_sec = HTTPGET_RW_TIMEOUT_SEC;
    rwto.tv_usec = 0;
    ::setsockopt(sock_id, SOL_SOCKET, SO_RCVTIMEO, &rwto, sizeof(rwto));
    ::setsockopt(sock_id, SOL_SOCKET, SO_SNDTIMEO, &rwto, sizeof(rwto));

    struct sockaddr_in servaddr;
    struct hostent *hp;
    ::memset(&servaddr, 0, sizeof(servaddr));
    if ( (hp = ::gethostbyname(hostname)) == NULL )
    {
        std::cout << "Host " << hostname << " not found" << std::endl;
        ::close(sock_id);
        return false;
    }
    ::memcpy((char *)&servaddr.sin_addr.s_addr, (char *)hp->h_addr, hp->h_length);
    servaddr.sin_port = htons(port);
    servaddr.sin_family = AF_INET;

    if ( !YsfHttpConnectWithTimeout(sock_id, (struct sockaddr *)&servaddr, sizeof(servaddr), HTTPGET_CONNECT_TIMEOUT_SEC) )
    {
        std::cout << "Cannot establish connection with host " << hostname << std::endl;
        ::close(sock_id);
        return false;
    }

    char request[YSFNODE_HTTPGET_SIZEMAX];
    int requestLen = ::snprintf(request, sizeof(request),
        "GET /%s HTTP/1.0\r\nHost: %s\r\nFrom: %s\r\nUser-Agent: xlxd\r\n\r\n",
        filename, hostname, (const char *)g_Reflector.GetCallsign());
    if ( requestLen > 0 && requestLen < (int)sizeof(request) )
    {
        ::write(sock_id, request, requestLen);
    }

    buffer->clear();
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(HTTPGET_TOTAL_TIMEOUT_SEC);
    while ( std::chrono::steady_clock::now() < deadline )
    {
        char buf[1440];
        ssize_t len = ::read(sock_id, buf, sizeof(buf));
        if ( len > 0 )
        {
            buffer->Append((uint8 *)buf, (int)len);
            ok = true;
            continue;
        }
        if ( len == 0 )
        {
            break;
        }
        break;
    }
    buffer->Append((uint8)0);

    ::close(sock_id);
    return ok;
}
