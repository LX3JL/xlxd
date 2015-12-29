//
//  cdvframepacket.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 01/11/2015.
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

#ifndef cdvframepacket_h
#define cdvframepacket_h

#include "cpacket.h"

////////////////////////////////////////////////////////////////////////////////////////
// defines

#define AMBE_SIZE       9
#define DVDATA_SIZE     3

// typedef & structures

struct __attribute__ ((__packed__))dstar_dvframe
{
    uint8	AMBE[AMBE_SIZE];
    uint8	DVDATA[DVDATA_SIZE];
};

////////////////////////////////////////////////////////////////////////////////////////
// class

class CDvFramePacket : public CPacket
{
public:
    // constructor
    CDvFramePacket();
    CDvFramePacket(const struct dstar_dvframe *, uint16 = 0, uint8 = 0);
    CDvFramePacket(const CDvFramePacket &);
    
    // destructor
    virtual ~CDvFramePacket() {};
    
    // virtual duplication
    CPacket *Duplicate(void) const;
    
    // identity
    bool IsDvFrame(void) const          { return true; }
    
    // get
    const uint8 *GetAmbe(void) const    { return m_uiAmbe; }
    const uint8 *GetDvData(void) const  { return m_uiDvData; }

    // operators
    bool operator ==(const CDvFramePacket &) const;
    
protected:
    // data
    uint8       m_uiAmbe[AMBE_SIZE];
    uint8       m_uiDvData[DVDATA_SIZE];
};


////////////////////////////////////////////////////////////////////////////////////////
#endif /* cdvframepacket_h */
