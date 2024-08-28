//
//  crawsocket.cpp
//  xlxd
//
//  Created by Marius Petrescu (YO2LOJ) on 22/02/2020.
//  Copyright © 2020 Marius Petrescu (YO2LOJ). All rights reserved.
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
#include "crawsocket.h"


////////////////////////////////////////////////////////////////////////////////////////
// constructor

CRawSocket::CRawSocket()
{
    m_Socket = -1;
}

////////////////////////////////////////////////////////////////////////////////////////
// destructor

CRawSocket::~CRawSocket()
{
    if ( m_Socket != -1 )
    {
        Close();
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// open & close

bool CRawSocket::Open(uint16 uiProto)
{
    bool open = false;
    int on = 1;
    
    // create socket
    m_Socket = socket(AF_INET,SOCK_RAW,uiProto);
    if ( m_Socket != -1 )
    {
        fcntl(m_Socket, F_SETFL, O_NONBLOCK);
        open = true;
        m_Proto = uiProto;
    }
    
    // done
    return open;
}

void CRawSocket::Close(void)
{
    if ( m_Socket != -1 )
    {
        close(m_Socket);
        m_Socket = -1;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// read

int CRawSocket::Receive(CBuffer *Buffer, CIp *Ip, int timeout)
{
    struct sockaddr_in Sin;
    fd_set FdSet;
    unsigned int uiFromLen = sizeof(struct sockaddr_in);
    int iRecvLen = -1;
    struct timeval tv;

    // socket valid ?
    if ( m_Socket != -1 )
    {
        // allocate buffer
        Buffer->resize(RAW_BUFFER_LENMAX);

        // control socket
        FD_ZERO(&FdSet);
        FD_SET(m_Socket, &FdSet);
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
        select(m_Socket + 1, &FdSet, 0, 0, &tv);

        // read
        iRecvLen = (int)recvfrom(m_Socket,
            (void *)Buffer->data(), RAW_BUFFER_LENMAX,
            0, (struct sockaddr *)&Sin, &uiFromLen);

        // handle
        if ( iRecvLen != -1 )
        {
            // adjust buffer size
            Buffer->resize(iRecvLen);

            // get IP
            Ip->SetSockAddr(&Sin);
        }
    }

    // done
    return iRecvLen;
}

// protocol specific

// ICMP

int CRawSocket::IcmpReceive(CBuffer *Buffer, CIp *Ip, int timeout)
{
    int iIcmpType = -1;
    int iRecv;

    if (m_Proto == IPPROTO_ICMP)
    {
        iRecv = Receive(Buffer, Ip, timeout);

        if (iRecv >= (int)(sizeof(struct ip) + sizeof(struct icmp)))
        {
            struct ip *iph = (struct ip *)Buffer->data();
            int iphdrlen = iph->ip_hl * 4;
            struct icmp *icmph = (struct icmp *)((unsigned char *)iph + iphdrlen);
            struct ip *remote_iph = (struct ip *)((unsigned char *)icmph + 8);

            iIcmpType = icmph->icmp_type;

            struct sockaddr_in Sin;
            bzero(&Sin, sizeof(Sin));
            Sin.sin_family = AF_INET;
            Sin.sin_addr.s_addr = remote_iph->ip_dst.s_addr;

            Ip->SetSockAddr(&Sin);

        }
    }
    return iIcmpType;
}
