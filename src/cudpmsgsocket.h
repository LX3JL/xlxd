//
//  cudpmsgsocket.h
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
//    Extension of the CUdpSocket class to allow retrieving
//    the local target IP for a G3 Terminal mode config request

#ifndef cudpmsgsocket_h
#define cudpmsgsocket_h

#include "cudpsocket.h"

#define UDP_MSG_BUFFER_LENMAX       1024

////////////////////////////////////////////////////////////////////////////////////////
// class

class CUdpMsgSocket : public CUdpSocket
{
public:
    // open
    bool Open(uint16);

    // read
    int Receive(CBuffer *, CIp *, int);

    struct in_addr *GetLocalAddr(void)   { return &m_LocalAddr; }

protected:
    // data
    struct in_addr      m_LocalAddr;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cudpmsgsocket_h */
