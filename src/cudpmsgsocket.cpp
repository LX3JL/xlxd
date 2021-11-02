//
//  cudpmsgsocket.cpp
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
#include "cudpmsgsocket.h"

// neither multiple socket nor IPv6 supported
#define MsgSocket m_Socket[0]
#define MsgIp     m_Ip[0]

////////////////////////////////////////////////////////////////////////////////////////
// open
bool CUdpMsgSocket::Open(uint16 uiPort)
{
    int on = 1, err = -1;
    struct sockaddr_storage *ss;
    socklen_t ss_len;
    
    if ( !CUdpSocket::Open(uiPort, AF_INET) )
    {
        return false;
    }
    
    ss = MsgIp.GetSockAddr(ss_len);
    if ( ss->ss_family == AF_INET )
    {
#if defined(IP_PKTINFO)
        err = setsockopt(MsgSocket, IPPROTO_IP, IP_PKTINFO, (char *)&on, sizeof(on));
#elif defined(IP_RECVDSTADDR)
        err = setsockopt(MsgSocket, IPPROTO_IP, IP_RECVDSTADDR, (char *)&on, sizeof(on));
#endif
    }
    
    if ( err < 0 )
    {
        CUdpSocket::Close();
        return false;
    }
    else
    {
        return true;
    }
}


////////////////////////////////////////////////////////////////////////////////////////
// read

int CUdpMsgSocket::Receive(CBuffer *Buffer, CIp *Ip, int timeout)
{
    struct sockaddr_in Sin;
    fd_set FdSet;
    unsigned int uiFromLen = sizeof(struct sockaddr_in);
    int iRecvLen = -1;
    struct timeval tv;

    struct msghdr Msg;
    struct iovec Iov[1];

    union {
         struct cmsghdr cm;
#if defined(IP_PKTINFO)
         unsigned char pktinfo_sizer[sizeof(struct cmsghdr) + sizeof(struct in_pktinfo)];
#elif defined(IP_RECVDSTADDR)
         unsigned char pktinfo_sizer[sizeof(struct cmsghdr) + sizeof(struct sockaddr_in)];
#endif
    } Control;

    // socket valid ?
    if ( MsgSocket != -1 )
    {
        // allocate buffer
        Buffer->resize(UDP_MSG_BUFFER_LENMAX);

        //prepare msghdr
        bzero(&Msg, sizeof(Msg));
        Iov[0].iov_base = Buffer->data();
        Iov[0].iov_len = UDP_MSG_BUFFER_LENMAX;

        bzero(&Sin, sizeof(Sin));
        Msg.msg_name = &Sin;
        Msg.msg_namelen = sizeof(Sin);
        Msg.msg_iov = Iov;
        Msg.msg_iovlen = 1;
        Msg.msg_control = &Control;
        Msg.msg_controllen = sizeof(Control);

        // control socket
        FD_ZERO(&FdSet);
        FD_SET(MsgSocket, &FdSet);
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
        select(MsgSocket + 1, &FdSet, 0, 0, &tv);
        
        // read
        iRecvLen = (int)recvmsg(MsgSocket, &Msg, 0);
        
        // handle
        if ( iRecvLen != -1 )
        {
            // adjust buffer size
            Buffer->resize(iRecvLen);
            
            // get IP
            Ip->SetSockAddr((struct sockaddr_storage *)&Sin, sizeof(Sin));

            // get local IP
            struct cmsghdr *Cmsg;
            for (Cmsg = CMSG_FIRSTHDR(&Msg); Cmsg != NULL; Cmsg = CMSG_NXTHDR(&Msg, Cmsg))
            {
#if defined(IP_PKTINFO)
                if (Cmsg->cmsg_level == IPPROTO_IP && Cmsg->cmsg_type == IP_PKTINFO)
                {
                    struct in_pktinfo *PktInfo = (struct in_pktinfo *)CMSG_DATA(Cmsg);
                    m_LocalAddr.s_addr = PktInfo->ipi_spec_dst.s_addr;
                }
#elif defined(IP_RECVDSTADDR)
                if (Cmsg->cmsg_level == IPPROTO_IP && Cmsg->cmsg_type == IP_RECVDSTADDR)
                {
                    struct sockaddr_in *DestAddr = (struct sockaddr_in *)CMSG_DATA(Cmsg);
                    m_LocalAddr.s_addr = DestAddr->sin_addr.s_addr;
                }
#endif
            }
        }
    }
 
    // done
    return iRecvLen;
}

