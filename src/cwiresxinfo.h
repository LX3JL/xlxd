//
//  cwiresxinfo.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 29/09/2019.
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

#ifndef cwiresxinfo_h
#define cwiresxinfo_h

#include "ysfdefines.h"
#include "ccallsign.h"

class CWiresxInfo
{
public:
    // constructor
    CWiresxInfo();
    
    // destructor
    virtual ~CWiresxInfo() {}
    
    // get
    const uint8 *GetCallsign(void) const    { return m_callsign; }
    const uint8 *GetNode(void) const        { return m_node; }
    const uint8 *GetName(void) const        { return m_name; }
    const uint8 *GetId(void) const          { return m_id; }
    uint        GetTxFrequency(void) const  { return m_txFrequency; }
    uint        GetRxFrequency(void) const  { return m_rxFrequency; }
    const uint8 *GetCsd1(void) const        { return m_csd1; }
    const uint8 *GetCsd2(void) const        { return m_csd2; }
    const uint8 *GetCsd3(void) const        { return m_csd3; }
    
    // set
    void SetCallsign(const CCallsign &);
    void SetNode(const char *);
    void SetName(const char *);
    void SetFrequencies(uint, uint);
    
    // uodates
    void UpdateCsds(void);
    void UpdateId(void);
    
protected:
    // data
    uint8 m_callsign[YSF_CALLSIGN_LENGTH];
    uint8 m_node[YSF_CALLSIGN_LENGTH];
    uint8 m_name[14];
    uint8 m_id[6];
    uint  m_txFrequency;
    uint  m_rxFrequency;

    uint8 m_csd1[20];
    uint8 m_csd2[20];
    uint8 m_csd3[20];
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cwiresxinfo_h */
