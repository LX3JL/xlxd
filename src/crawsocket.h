//
//  crawsocket.h
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

// Description:
//    Raw socket access class with protocol specific 


#ifndef crawsocket_h
#define crawsocket_h

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>

#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include "cip.h"
#include "cbuffer.h"

////////////////////////////////////////////////////////////////////////////////////////
// define

#define RAW_BUFFER_LENMAX       65536


////////////////////////////////////////////////////////////////////////////////////////
// class

class CRawSocket
{
public:
    // constructor
    CRawSocket();
    
    // destructor
    ~CRawSocket();
    
    // open & close
    bool Open(uint16);
    void Close(void);
    int  GetSocket(void)        { return m_Socket; }
    
    // read

    // if ETH_P_ALL is used, the received data buffer will hold
    // the ethernet header (struct ethhdr) followed by the IP header (struct iphdr),
    // the protocol header (e.g tcp, udp, icmp) and the data.
    // For specific protocols, the data content may vary depending on the protocol
    // Returns the number of received bytes in buffer

    int Receive(CBuffer *, CIp *, int);

    // ICMP receive helper
    // parameters:
    //   buffer - packet receive buffer (starting with ip header)
    //   ip - remote address (filled in on receive)
    //   timeout - receive timeout in msec
    // return value:
    //   ICMP type, -1 if nothing was received

    int IcmpReceive(CBuffer *, CIp *, int);

    // write
    // no write support - complexity makes it out of scope for now
    // to be added if needed

protected:
    // data
    int                 m_Socket;
    int                 m_Proto;
    struct sockaddr_in  m_SocketAddr;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* crawsocket_h */
