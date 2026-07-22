//
//  cnxdniddirhttp.cpp
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
#include <sys/time.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include <chrono>
#include "main.h"
#include "creflector.h"
#include "cnxdniddirhttp.h"

#if (NXDNIDDB_USE_RLX_SERVER == 1)
CNxdnIdDirHttp g_NxdnIdDir;
#endif


////////////////////////////////////////////////////////////////////////////////////////
// refresh

bool CNxdnIdDirHttp::LoadContent(CBuffer *buffer)
{
    // Try primary source: radioid.net
    if ( HttpGet("database.radioid.net", "static/nxdn.csv", 80, buffer) )
    {
        m_LastSource = "radioid.net";
        return true;
    }

    // Fallback to pistar.uk mirror
    std::cout << "Primary NXDN database source failed, trying pistar.uk fallback..." << std::endl;
    if ( HttpGet("www.pistar.uk", "downloads/NXDN.csv", 80, buffer) )
    {
        m_LastSource = "pistar.uk";
        return true;
    }

    return false;
}

bool CNxdnIdDirHttp::RefreshContent(const CBuffer &buffer)
{
    bool ok = false;
    bool firstLine = true;

    // clear directory
    m_CallsignMap.clear();
    m_NxdnIdMap.clear();

    // scan file
    // CSV format: RADIO_ID,CALLSIGN,FIRST_NAME,LAST_NAME,CITY,STATE,COUNTRY
    if ( buffer.size() > 0 )
    {
        // Make a working copy since strtok modifies the string
        char *workbuf = new char[buffer.size() + 1];
        ::memcpy(workbuf, buffer.data(), buffer.size());
        workbuf[buffer.size()] = '\0';

        char *ptr1 = workbuf;
        char *ptr2;

        // Skip HTTP headers if present (find double newline)
        char *bodyStart = ::strstr(ptr1, "\r\n\r\n");
        if ( bodyStart != NULL )
        {
            ptr1 = bodyStart + 4;
        }

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
    std::cout << "Read " << m_NxdnIdMap.size() << " NXDN ids from " << m_LastSource << " database " << std::endl;

    // done
    return ok;
}


////////////////////////////////////////////////////////////////////////////////////////
// httpd helpers

#define NXDNID_HTTPGET_SIZEMAX         (512)
#define HTTPGET_CONNECT_TIMEOUT_SEC    (10)
#define HTTPGET_RW_TIMEOUT_SEC         (10)
#define HTTPGET_TOTAL_TIMEOUT_SEC      (60)

// Non-blocking connect with a bounded timeout. See cdmriddirhttp.cpp for
// rationale — same helper duplicated here to keep this translation unit
// self-contained.
static bool NxdnHttpConnectWithTimeout(int sock, const struct sockaddr *addr, socklen_t addrlen, int timeoutSec)
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

bool CNxdnIdDirHttp::HttpGet(const char *hostname, const char *filename, int port, CBuffer *buffer)
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

    if ( !NxdnHttpConnectWithTimeout(sock_id, (struct sockaddr *)&servaddr, sizeof(servaddr), HTTPGET_CONNECT_TIMEOUT_SEC) )
    {
        std::cout << "Cannot establish connection with host " << hostname << std::endl;
        ::close(sock_id);
        return false;
    }

    char request[NXDNID_HTTPGET_SIZEMAX];
    int requestLen = ::snprintf(request, sizeof(request),
        "GET /%s HTTP/1.0\r\nHost: %s\r\nFrom: %s\r\nUser-Agent: XLX %d.%d.%d NXDN Client Updater\r\n\r\n",
        filename, hostname, (const char *)g_Reflector.GetCallsign(),
        VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION);
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
