//
//  cwiresxpacket.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 09/10/2019.
//  Copyright © 2019 Jean-Luc Deltombe (LX3JL). All rights reserved.
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

#ifndef cwiresxpacket_h
#define cwiresxpacket_h

#include "cbuffer.h"
#include "cip.h"

////////////////////////////////////////////////////////////////////////////////////////
// class

class CWiresxPacket
{
public:
    // constructor
    CWiresxPacket() {}
    CWiresxPacket(const CBuffer &Buffer, const CIp &Ip) { m_Buffer = Buffer; m_Ip = Ip; }
    
    // destructor
    virtual ~CWiresxPacket() {}
    
    // get
    const CBuffer &GetBuffer(void) const        { return m_Buffer; }
    const CIp &GetIp(void) const                { return m_Ip; }
    
protected:
    // data
    CBuffer     m_Buffer;
    CIp         m_Ip;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cwiresxpacket_h */
