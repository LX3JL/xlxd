//
//  cudpsocket.cpp
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
#include "creflector.h"
#include "cudpsocket.h"


////////////////////////////////////////////////////////////////////////////////////////
// constructor

CUdpSocket::CUdpSocket()
{
    for ( int i = 0; i < UDP_SOCKET_MAX; i++ )
    {
        m_Socket[i] = -1;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// destructor

CUdpSocket::~CUdpSocket()
{
    Close();
}

////////////////////////////////////////////////////////////////////////////////////////
// open & close

bool CUdpSocket::Open(uint16 uiPort)
{
    bool open = false;
    struct sockaddr_storage *ss;
    socklen_t ss_len;
    
    for ( int i = 0; i < UDP_SOCKET_MAX; i++ )
    {
        m_Ip[i] = CIp(g_Reflector.GetListenIp(i), uiPort);
        ss = m_Ip[i].GetSockAddr(ss_len);
        
        // create socket
        // (avoid INADDR_ANY on secondary and later IP address)
        m_Socket[i] = ( i != 0 && m_Ip[i] == CIp() ) ?
            -1 : socket(ss->ss_family, SOCK_DGRAM, 0);
        if ( m_Socket[i] != -1 )
        {
            if ( bind(m_Socket[i], (struct sockaddr *)ss, ss_len) == 0 )
            {
                fcntl(m_Socket[i], F_SETFL, O_NONBLOCK);
                open |= true;
            }
            else
            {
                close(m_Socket[i]);
                m_Socket[i] = -1;
            }
        }
    }
    
    // done
    return open;
}

void CUdpSocket::Close(void)
{
    for ( int i = 0; i < UDP_SOCKET_MAX; i++ )
    {
        if ( m_Socket[i] != -1 )
        {
            close(m_Socket[i]);
            m_Socket[i] = -1;
        }
    }
}

int CUdpSocket::GetSocket(const CIp &Ip)
{
    CIp temp(Ip);
    socklen_t ss_len;
    struct sockaddr_storage *ss = temp.GetSockAddr(ss_len);
    sa_family_t ss_family = ss->ss_family;
    
    for ( int i = 0; i < UDP_SOCKET_MAX; i++ )
    {
        ss = m_Ip[i].GetSockAddr(ss_len);
        if ( ss_family == ss->ss_family )
        {
            return m_Socket[i];
        }
    }
    
    return -1;
}

////////////////////////////////////////////////////////////////////////////////////////
// read

int CUdpSocket::Receive(CBuffer *Buffer, CIp *Ip, int timeout)
{
    struct sockaddr_storage Sin;
    struct pollfd pfd[UDP_SOCKET_MAX];
    socklen_t uiFromLen = sizeof(Sin);
    int i, socks, index, iRecvLen = -1;
    
    // socket valid ?
    for ( i = socks = 0; i < UDP_SOCKET_MAX; i++ )
    {
        if ( m_Socket[i] != -1 )
        {
            pfd[socks].fd = m_Socket[i];
            pfd[socks].events = POLLIN;
            socks++;
        }
    }
    
    if ( socks != 0 )
    {
        // control socket
        poll(pfd, socks, timeout);
        
        for ( i = 0; i < socks; i++ )
        {
            // round robin
            index = (i + m_Counter) % socks;
            
            if ( pfd[index].revents & POLLIN )
            {
                // allocate buffer
                Buffer->resize(UDP_BUFFER_LENMAX);
                
                // read
                iRecvLen = (int)recvfrom(pfd[index].fd,
                    (void *)Buffer->data(), UDP_BUFFER_LENMAX,
                    0, (struct sockaddr *)&Sin, &uiFromLen);
                
                // handle
                if ( iRecvLen != -1 )
                {
                    // adjust buffer size
                    Buffer->resize(iRecvLen);
                    
                    // get IP
                    Ip->SetSockAddr(&Sin, uiFromLen);
                    
                    m_Counter++;
                    break;
                }
            }
        }
    }
 
    // done
    return iRecvLen;
}

////////////////////////////////////////////////////////////////////////////////////////
// write

int CUdpSocket::Send(const CBuffer &Buffer, const CIp &Ip)
{
    CIp temp(Ip);
    socklen_t ss_len;
    struct sockaddr_storage *ss = temp.GetSockAddr(ss_len);
    return (int)::sendto(GetSocket(Ip),
           (void *)Buffer.data(), Buffer.size(),
           0, (struct sockaddr *)ss, ss_len);
}

int CUdpSocket::Send(const char *Buffer, const CIp &Ip)
{
    CIp temp(Ip);
    socklen_t ss_len;
    struct sockaddr_storage *ss = temp.GetSockAddr(ss_len);
    return (int)::sendto(GetSocket(Ip),
           (void *)Buffer, ::strlen(Buffer),
           0, (struct sockaddr *)ss, ss_len);
}

int CUdpSocket::Send(const CBuffer &Buffer, const CIp &Ip, uint16 destport)
{
    CIp temp(Ip, destport);
    socklen_t ss_len;
    struct sockaddr_storage *ss = temp.GetSockAddr(ss_len);
    return (int)::sendto(GetSocket(Ip),
                         (void *)Buffer.data(), Buffer.size(),
                         0, (struct sockaddr *)ss, ss_len);
}

int CUdpSocket::Send(const char *Buffer, const CIp &Ip, uint16 destport)
{
    CIp temp(Ip, destport);
    socklen_t ss_len;
    struct sockaddr_storage *ss = temp.GetSockAddr(ss_len);
    return (int)::sendto(GetSocket(Ip),
                         (void *)Buffer, ::strlen(Buffer),
                         0, (struct sockaddr *)ss, ss_len);
}


