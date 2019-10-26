//
//  cysfnode.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 08/10/2019.
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

#ifndef cysfnode_h
#define cysfnode_h

#include "main.h"
#include "ccallsign.h"

////////////////////////////////////////////////////////////////////////////////////////
// define


////////////////////////////////////////////////////////////////////////////////////////
// class

class CYsfNode
{
public:
    // constructor
    CYsfNode();
    CYsfNode(const CCallsign &, uint32, uint32);
    CYsfNode(const CYsfNode &);
    
    // destructor
    virtual ~CYsfNode() {}
    
    // get
    uint32  GetTxFrequency(void) const          { return m_uiTxFreq; }
    uint32  GetRxFrequency(void) const          { return m_uiRxFreq; }
    bool       IsValid(void) const;
    
protected:
    // data
    CCallsign   m_Callsign;
    uint32      m_uiTxFreq;
    uint32      m_uiRxFreq;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cysfnode_h */
