//
//  cnxdniddirhttp.cpp
//  xlxd
//
//  Created for NXDN Reflector peering support
//  XLX API source and parser rework by Luc (LX1IQ)
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
// ----------------------------------------------------------------------------

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
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
// Helpers

static char *NxdnTrim(char *str)
{
    if ( str == NULL )
        return NULL;

    while ( *str && isspace((unsigned char)*str) )
        ++str;

    char *end = str + strlen(str);

    while ( end > str && isspace((unsigned char)*(end - 1)) )
        --end;

    *end = '\0';

    return str;
}

////////////////////////////////////////////////////////////////////////////////////////
// Load database

bool CNxdnIdDirHttp::LoadContent(CBuffer *buffer)
{
    // Primary source: XLX API server
    if ( HttpGet("xlxapi.rlx.lu", "api/exportnxdn.php", 80, buffer) )
    {
        m_LastSource = "xlxapi.rlx.lu";
        std::cout << "NXDN database source: XLX API server" << std::endl;
        return true;
    }

    // Fallback: radioid.net
    std::cout << "XLX API NXDN database failed, trying radioid.net..."
              << std::endl;

    if ( HttpGet("database.radioid.net", "static/nxdn.csv", 80, buffer) )
    {
        m_LastSource = "radioid.net";
        std::cout << "NXDN database source: radioid.net" << std::endl;
        return true;
    }

    std::cout << "ERROR: All NXDN database sources failed" << std::endl;
    return false;
}

////////////////////////////////////////////////////////////////////////////////////////
// Parse database
//
// Supported formats:
//
// XLX API:
//   2;W2FLY;
//   3;WB2OOM;
//
// CSV:
//   RADIO_ID,CALLSIGN,FIRST_NAME,LAST_NAME,CITY,STATE,COUNTRY
//   1234567,AB1CD,First,Last,City,State,Country
//
// Both formats are parsed using comma and semicolon delimiters.

bool CNxdnIdDirHttp::RefreshContent(const CBuffer &buffer)
{
    if ( buffer.size() == 0 )
    {
        std::cout << "ERROR: Empty NXDN database received from "
                  << m_LastSource << std::endl;
        return false;
    }

    // Create a writable copy. CBuffer may contain a terminating zero.
    char *workbuf = new char[buffer.size() + 1];

    ::memcpy(workbuf, buffer.data(), buffer.size());
    workbuf[buffer.size()] = '\0';

    char *body = workbuf;

    // Skip HTTP response headers, if present.
    char *bodyStart = ::strstr(body, "\r\n\r\n");

    if ( bodyStart != NULL )
    {
        body = bodyStart + 4;
    }
    else
    {
        bodyStart = ::strstr(body, "\n\n");

        if ( bodyStart != NULL )
            body = bodyStart + 2;
    }

    // Build temporary maps. Keep the existing database if parsing fails.
    decltype(m_CallsignMap) newCallsignMap;
    decltype(m_NxdnIdMap) newNxdnIdMap;

    unsigned int count = 0;
    char *line = body;

    while ( *line != '\0' )
    {
        // Find the end of this line.
        char *nextLine = ::strchr(line, '\n');

        if ( nextLine != NULL )
        {
            *nextLine = '\0';
            ++nextLine;
        }

        // Remove whitespace and carriage returns.
        char *record = NxdnTrim(line);

        if ( *record != '\0' )
        {
            // Both the XLX API and CSV use the ID as the first field
            // and the callsign as the second field.
            char *idStr = ::strtok(record, ";,");
            char *callsignStr = ::strtok(NULL, ";,");

            idStr = NxdnTrim(idStr);
            callsignStr = NxdnTrim(callsignStr);

            // CSV header lines are ignored because RADIO_ID is not
            // a valid numeric NXDN ID.
            if ( idStr != NULL &&
                 callsignStr != NULL &&
                 IsValidNxdnId(idStr) )
            {
                char *endptr = NULL;
                long parsedId = ::strtol(idStr, &endptr, 10);

                if ( endptr != idStr &&
                     *endptr == '\0' &&
                     parsedId > 0 &&
                     parsedId <= 65535 )
                {
                    uint16_t nxdnid = (uint16_t)parsedId;
                    CCallsign cs(callsignStr);

                    if ( cs.IsValid() )
                    {
                        // Every ID gets its callsign, the first row wins for a
                        // duplicate ID. A callsign with several IDs (one per
                        // radio) maps back to the first of them.
                        auto idResult = newCallsignMap.insert(
                            std::pair<uint16_t, CCallsign>(nxdnid, cs));

                        if ( idResult.second )
                        {
                            newNxdnIdMap.insert(
                                std::pair<CCallsign, uint16_t>(cs, nxdnid));

                            ++count;
                        }
                    }
                }
            }
        }

        if ( nextLine == NULL )
            break;

        line = nextLine;
    }

    delete[] workbuf;

    // Do not replace the current database with an empty or invalid result.
    if ( count == 0 )
    {
        std::cout << "ERROR: No valid NXDN IDs found in database from "
                  << m_LastSource << std::endl;
        return false;
    }

    // Replace the existing maps only after successful parsing.
    m_CallsignMap.swap(newCallsignMap);
    m_NxdnIdMap.swap(newNxdnIdMap);

    std::cout << "Read " << count << " NXDN IDs from "
              << m_LastSource << " database" << std::endl;

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
// HTTP helpers

#define NXDNID_HTTPGET_SIZEMAX         (512)
#define HTTPGET_CONNECT_TIMEOUT_SEC    (10)
#define HTTPGET_RW_TIMEOUT_SEC         (10)
#define HTTPGET_TOTAL_TIMEOUT_SEC      (60)

// Non-blocking connect with a bounded timeout.
static bool NxdnHttpConnectWithTimeout(
    int sock,
    const struct sockaddr *addr,
    socklen_t addrlen,
    int timeoutSec)
{
    int flags = ::fcntl(sock, F_GETFL, 0);

    if ( flags < 0 )
        return false;

    if ( ::fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0 )
        return false;

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

    if ( ::getsockopt(sock, SOL_SOCKET, SO_ERROR, &soerr, &slen) < 0 ||
         soerr != 0 )
    {
        ::fcntl(sock, F_SETFL, flags);
        return false;
    }

    ::fcntl(sock, F_SETFL, flags);
    return true;
}

bool CNxdnIdDirHttp::HttpGet(
    const char *hostname,
    const char *filename,
    int port,
    CBuffer *buffer)
{
    int sock_id = ::socket(AF_INET, SOCK_STREAM, 0);

    if ( sock_id < 0 )
    {
        std::cout << "Failed to open HTTP socket" << std::endl;
        return false;
    }

    // Bound individual read and write operations.
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

    ::memcpy(
        (char *)&servaddr.sin_addr.s_addr,
        (char *)hp->h_addr,
        hp->h_length);

    servaddr.sin_port = htons(port);
    servaddr.sin_family = AF_INET;

    if ( !NxdnHttpConnectWithTimeout(
             sock_id,
             (struct sockaddr *)&servaddr,
             sizeof(servaddr),
             HTTPGET_CONNECT_TIMEOUT_SEC) )
    {
        std::cout << "Cannot establish connection with host "
                  << hostname << std::endl;
        ::close(sock_id);
        return false;
    }

    char request[NXDNID_HTTPGET_SIZEMAX];

    int requestLen = ::snprintf(
        request,
        sizeof(request),
        "GET /%s HTTP/1.0\r\n"
        "Host: %s\r\n"
        "From: %s\r\n"
        "User-Agent: XLX %d.%d.%d NXDN Client Updater\r\n"
        "\r\n",
        filename,
        hostname,
        (const char *)g_Reflector.GetCallsign(),
        VERSION_MAJOR,
        VERSION_MINOR,
        VERSION_REVISION);

    if ( requestLen <= 0 || requestLen >= (int)sizeof(request) )
    {
        std::cout << "HTTP request is too long" << std::endl;
        ::close(sock_id);
        return false;
    }

    // Send the complete request.
    int sent = 0;

    while ( sent < requestLen )
    {
        ssize_t written = ::write(
            sock_id,
            request + sent,
            requestLen - sent);

        if ( written <= 0 )
        {
            std::cout << "Failed to send HTTP request to "
                      << hostname << std::endl;
            ::close(sock_id);
            return false;
        }

        sent += (int)written;
    }

    buffer->clear();

    bool ok = false;

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
            break;

        // Interrupted reads can be retried.
        if ( errno == EINTR )
            continue;

        break;
    }

    // Preserve the existing CBuffer convention.
    buffer->Append((uint8)0);

    ::close(sock_id);

    // Only a 200 is a database, a redirect or an error page tries the next source
    if ( ok )
    {
        int status = 0;

        if ( ::sscanf((const char *)buffer->data(), "HTTP/%*d.%*d %d", &status) != 1 ||
             status != 200 )
        {
            std::cout << "HTTP status " << status << " from host "
                      << hostname << std::endl;
            ok = false;
        }
    }

    return ok;
}
