//
//  cip.h
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

#ifndef cip_h
#define cip_h


////////////////////////////////////////////////////////////////////////////////////////
// class

class CIp
{
public:
    // constructors
    CIp();
    //CIp(uint8, uint8, uint8, uint8);
    CIp(const struct sockaddr_in *);
    CIp(const char *);
    CIp(const CIp &);
    
    // destructor
    virtual ~CIp() {};
    
    // sockaddr
    void SetSockAddr(struct sockaddr_in *);
    struct sockaddr_in *GetSockAddr(void)     { return &m_Addr; }
    
    // convertor
    uint32 GetAddr(void) const                { return m_Addr.sin_addr.s_addr; }
    uint16 GetPort(void) const                { return m_Addr.sin_port; }
    
    // operator
    bool operator ==(const CIp &) const;
    operator const char *() const;
    
protected:
    // data
    struct sockaddr_in  m_Addr;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cip_h */
