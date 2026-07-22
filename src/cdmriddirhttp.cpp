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
#include <sys/time.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include <chrono>
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
    std::cout << "Read " << m_DmridMap.size() << " DMR ids from xlxapi.rlx.lu database " << std::endl;
    
    // done
    return ok;
}


////////////////////////////////////////////////////////////////////////////////////////
// httpd helpers

#define DMRID_HTTPGET_SIZEMAX       (256)
#define HTTPGET_CONNECT_TIMEOUT_SEC (10)
#define HTTPGET_RW_TIMEOUT_SEC      (5)
#define HTTPGET_TOTAL_TIMEOUT_SEC   (60)

// Non-blocking connect with a bounded timeout. Returns true on success.
// Leaves the socket in blocking mode on return.
static bool HttpConnectWithTimeout(int sock, const struct sockaddr *addr, socklen_t addrlen, int timeoutSec)
{
    // Switch socket to non-blocking for the duration of connect()
    int flags = ::fcntl(sock, F_GETFL, 0);
    if ( flags < 0 ) return false;
    if ( ::fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0 ) return false;

    int rc = ::connect(sock, addr, addrlen);
    if ( rc == 0 )
    {
        // Connected immediately (unlikely for TCP but valid)
        ::fcntl(sock, F_SETFL, flags);
        return true;
    }
    if ( errno != EINPROGRESS )
    {
        ::fcntl(sock, F_SETFL, flags);
        return false;
    }

    // Wait for writability with our own timeout
    fd_set wset;
    FD_ZERO(&wset);
    FD_SET(sock, &wset);
    struct timeval tv;
    tv.tv_sec = timeoutSec;
    tv.tv_usec = 0;
    rc = ::select(sock + 1, NULL, &wset, NULL, &tv);
    if ( rc <= 0 )
    {
        // timeout or error
        ::fcntl(sock, F_SETFL, flags);
        return false;
    }

    // Check SO_ERROR to distinguish connected-successfully from async error
    int soerr = 0;
    socklen_t slen = sizeof(soerr);
    if ( ::getsockopt(sock, SOL_SOCKET, SO_ERROR, &soerr, &slen) < 0 || soerr != 0 )
    {
        ::fcntl(sock, F_SETFL, flags);
        return false;
    }

    // Restore blocking mode
    ::fcntl(sock, F_SETFL, flags);
    return true;
}

bool CDmridDirHttp::HttpGet(const char *hostname, const char *filename, int port, CBuffer *buffer)
{
    bool ok = false;
    int sock_id;

    if ( (sock_id = ::socket(AF_INET, SOCK_STREAM, 0)) < 0 )
    {
        std::cout << "Failed to open wget socket" << std::endl;
        return false;
    }

    // Apply per-operation send/receive timeouts so a peer that half-dies
    // mid-transfer cannot wedge this thread forever. Without these, both
    // ::write() and ::read() block indefinitely on a stalled socket.
    struct timeval rwto;
    rwto.tv_sec = HTTPGET_RW_TIMEOUT_SEC;
    rwto.tv_usec = 0;
    ::setsockopt(sock_id, SOL_SOCKET, SO_RCVTIMEO, &rwto, sizeof(rwto));
    ::setsockopt(sock_id, SOL_SOCKET, SO_SNDTIMEO, &rwto, sizeof(rwto));

    // Resolve hostname. gethostbyname has no configurable timeout; on Linux
    // it respects /etc/resolv.conf's `timeout` and `attempts` options, so
    // a misconfigured resolver can block ~15s. That is ugly but still
    // bounded, and this runs on the background refresh thread so it does
    // not affect voice routing.
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

    // Bounded connect (avoids up-to-127s kernel TCP SYN retry block)
    if ( !HttpConnectWithTimeout(sock_id, (struct sockaddr *)&servaddr, sizeof(servaddr), HTTPGET_CONNECT_TIMEOUT_SEC) )
    {
        std::cout << "Cannot establish connection with host " << hostname << std::endl;
        ::close(sock_id);
        return false;
    }

    // Send the GET request. SO_SNDTIMEO bounds the write() if the socket
    // is wedged.
    char request[DMRID_HTTPGET_SIZEMAX];
    int requestLen = ::snprintf(request, sizeof(request),
        "GET /%s HTTP/1.0\r\nHost: %s\r\nFrom: %s\r\nUser-Agent: xlxd\r\n\r\n",
        filename, hostname, (const char *)g_Reflector.GetCallsign());
    if ( requestLen > 0 && requestLen < (int)sizeof(request) )
    {
        ::write(sock_id, request, requestLen);
    }

    // Read response with a wall-clock overall deadline so a peer that
    // dribbles bytes forever cannot stall us indefinitely either.
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
            // clean EOF from peer
            break;
        }
        // len < 0 — error. EAGAIN/EWOULDBLOCK means the SO_RCVTIMEO fired
        // mid-transfer; we bail rather than risk looping forever.
        break;
    }
    buffer->Append((uint8)0);

    ::close(sock_id);
    return ok;
}
