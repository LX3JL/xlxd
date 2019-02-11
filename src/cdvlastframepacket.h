//
//  cdvlastframepacket.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 03/11/2015.
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

#ifndef cdvlastframepacket_h
#define cdvlastframepacket_h


#include "cdvframepacket.h"

////////////////////////////////////////////////////////////////////////////////////////
// defines


////////////////////////////////////////////////////////////////////////////////////////
// class

class CDvLastFramePacket : public CDvFramePacket
{
public:
    // constructor
    CDvLastFramePacket();
    CDvLastFramePacket(const struct dstar_dvframe *, uint16, uint8);
    CDvLastFramePacket(const uint8 *, const uint8 *, uint16, uint8, uint8);
    CDvLastFramePacket(uint16, uint8, const uint8 *, const uint8 *, const uint8 *, uint8, uint8, const uint8 *, const uint8 *);
    CDvLastFramePacket(const CDvLastFramePacket &);
    
    // destructor
    virtual ~CDvLastFramePacket() {};
    
    // virtual duplication
    CPacket *Duplicate(void) const;
    
    // identity
    bool IsLastPacket(void) const           { return true; }
    bool HaveTranscodableAmbe(void) const   { return false; }
};


////////////////////////////////////////////////////////////////////////////////////////
#endif /* cdvlastframepacket_h */
