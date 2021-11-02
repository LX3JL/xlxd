//
//  cip.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 31/10/2015.
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

#include "main.h"
#include <string.h>
#include "cip.h"

#include <netdb.h>

////////////////////////////////////////////////////////////////////////////////////////
// constructors

CIp::CIp(const int af)
{
    ::memset(&m_Addr, 0, m_AddrLen = sizeof(struct sockaddr_in));
    m_Addr.ss_family = af;
}

CIp::CIp(const char *sz)
{
    struct addrinfo hints, *res;
    
    ::memset(&m_Addr, 0, m_AddrLen = sizeof(struct sockaddr_in));
    m_Addr.ss_family = AF_INET;
    
    ::memset(&hints, 0, sizeof(hints));
    if ( getaddrinfo(sz, NULL, &hints, &res) == 0 )
    {
        ::memcpy(&m_Addr, res->ai_addr, m_AddrLen = res->ai_addrlen);
        freeaddrinfo(res);
    }
}

CIp::CIp(const struct sockaddr_storage *ss, socklen_t len)
{
    len = ( len < sizeof(m_Addr) ) ? len : sizeof(m_Addr);
    ::memcpy(&m_Addr, ss, m_AddrLen = len);
}

CIp::CIp(const CIp &ip)
{
    ::memcpy(&m_Addr, &ip.m_Addr, m_AddrLen = ip.m_AddrLen);
}

CIp::CIp(const CIp &ip, uint16 port)
{
    ::memcpy(&m_Addr, &ip.m_Addr, m_AddrLen = ip.m_AddrLen);
    
    switch (m_Addr.ss_family)
    {
        case AF_INET:
            ((struct sockaddr_in *)&m_Addr)->sin_port = htons(port);
            break;
        case AF_INET6:
            ((struct sockaddr_in6 *)&m_Addr)->sin6_port = htons(port);
            break;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// sockaddr

void CIp::SetSockAddr(struct sockaddr_storage *ss, socklen_t len)
{
    len = ( len < sizeof(m_Addr) ) ? len : sizeof(m_Addr);
    ::memcpy(&m_Addr, ss, m_AddrLen = len);
}

struct sockaddr_storage *CIp::GetSockAddr(socklen_t &len)
{
    len = m_AddrLen;
    return &m_Addr;
}

////////////////////////////////////////////////////////////////////////////////////////
// operator

bool CIp::operator ==(const CIp &ip) const
{
    if ( ip.m_Addr.ss_family != m_Addr.ss_family )
    {
        return false;
    }
    
    switch (m_Addr.ss_family)
    {
        case AF_INET:
            struct sockaddr_in *pi4, *pm4;
            pi4 = (struct sockaddr_in *)&ip.m_Addr;
            pm4 = (struct sockaddr_in *)&m_Addr;
            return ( (pi4->sin_addr.s_addr == pm4->sin_addr.s_addr) &&
                     (pi4->sin_port == pm4->sin_port) );
        case AF_INET6:
            struct sockaddr_in6 *pi6, *pm6;
            pi6 = (struct sockaddr_in6 *)&ip.m_Addr;
            pm6 = (struct sockaddr_in6 *)&m_Addr;
            return ( IN6_ARE_ADDR_EQUAL(&pi6->sin6_addr, &pm6->sin6_addr) &&
                     (pi6->sin6_port == pm6->sin6_port) );
        default:
            return false;
    }
}

CIp::operator const char *() const
{
    switch (m_Addr.ss_family)
    {
        case AF_INET:
            return ::inet_ntop(m_Addr.ss_family,
                               &((struct sockaddr_in *)&m_Addr)->sin_addr.s_addr,
                               m_AddrStr, sizeof(m_AddrStr));
        case AF_INET6:
            return ::inet_ntop(m_Addr.ss_family,
                               ((struct sockaddr_in6 *)&m_Addr)->sin6_addr.s6_addr,
                               m_AddrStr, sizeof(m_AddrStr));
        default:
            return ::strncpy(m_AddrStr, "unknown", sizeof(m_AddrStr));
    }
}


