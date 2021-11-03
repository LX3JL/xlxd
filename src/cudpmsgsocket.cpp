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

////////////////////////////////////////////////////////////////////////////////////////
// open
bool CUdpMsgSocket::Open(uint16 uiPort)
{
    int on = 1, err;
    struct sockaddr_storage *ss;
    socklen_t ss_len;
    
    // IPv6 not supported
    if ( !CUdpSocket::Open(uiPort, AF_INET) )
    {
        return false;
    }
    
    for ( int i = 0; i < UDP_SOCKET_MAX; i++ )
    {
        if ( m_Socket[i] != -1 )
        {
            ss = m_Ip[i].GetSockAddr(ss_len);
            switch ( ss->ss_family )
            {
                case AF_INET:
#if defined(IP_PKTINFO)
                    err = setsockopt(m_Socket[i], IPPROTO_IP, IP_PKTINFO, (char *)&on, sizeof(on));
#elif defined(IP_RECVDSTADDR)
                    err = setsockopt(m_Socket[i], IPPROTO_IP, IP_RECVDSTADDR, (char *)&on, sizeof(on));
#endif
                    break;
                default:
                    err = -1;
                    break;
            }
            
            if ( err < 0 )
            {
                CUdpSocket::Close();
                return false;
            }
        }
    }
    
    return true;
}


////////////////////////////////////////////////////////////////////////////////////////
// read

int CUdpMsgSocket::Receive(CBuffer *Buffer, CIp *Ip, int timeout)
{
    struct sockaddr_in Sin;
    struct pollfd pfd[UDP_SOCKET_MAX];
    int i, socks, index, iRecvLen = -1;
    
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
                Buffer->resize(UDP_MSG_BUFFER_LENMAX);
                
                // prepare msghdr
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
                
                // read
                iRecvLen = (int)recvmsg(pfd[index].fd, &Msg, 0);
                
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
                    
                    m_Counter++;
                    break;
                }
            }
        }
    }
    
    // done
    return iRecvLen;
}

