//
//  cwiresxinfo.cpp
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

#include <string.h>
#include "main.h"
#include "ccallsign.h"
#include "cwiresxinfo.h"


////////////////////////////////////////////////////////////////////////////////////////
// constructor

CWiresxInfo::CWiresxInfo()
{
    ::memset(m_callsign, ' ', YSF_CALLSIGN_LENGTH);
    ::memset(m_node, ' ', YSF_CALLSIGN_LENGTH);
    ::memset(m_name, ' ', 14);
    ::memset(m_id, ' ', 6);
    m_txFrequency = 0U;
    m_rxFrequency = 0U;
 
    ::memset(m_csd1, '*', 20U);
    ::memset(m_csd2, ' ', 20U);
    ::memset(m_csd3, ' ', 20U);
}


////////////////////////////////////////////////////////////////////////////////////////
///// set

void CWiresxInfo::SetCallsign(const CCallsign &callsign)
{
    ::memset(m_callsign, ' ', YSF_CALLSIGN_LENGTH);
    callsign.GetCallsign(m_callsign);
    UpdateCsds();
}

void CWiresxInfo::SetNode(const char *node)
{
    ::memset(m_node, ' ', YSF_CALLSIGN_LENGTH);
    ::memcpy(m_node, node, MIN(::strlen(node), YSF_CALLSIGN_LENGTH));
    UpdateCsds();
}

void CWiresxInfo::SetName(const char *name)
{
    ::memset(m_name, ' ', 14);
    ::memcpy(m_name, name, MIN(::strlen(name), 14));
    UpdateId();
}

void CWiresxInfo::SetFrequencies(uint txFreq, uint rxFreq)
{
    m_txFrequency = txFreq;
    m_rxFrequency = rxFreq;
}

////////////////////////////////////////////////////////////////////////////////////////
// updates

void CWiresxInfo::UpdateCsds(void)
{
    ::memset(m_csd1, '*', 20U);
    ::memset(m_csd2, ' ', 20U);
    ::memset(m_csd3, ' ', 20U);
    ::memcpy(m_csd1 + 10U, m_node, 10U);
    ::memcpy(m_csd2 +  0U, m_callsign, 10U);
    ::memcpy(m_csd3 +  0U, m_id, 5U);
    ::memcpy(m_csd3 + 15U, m_id, 5U);
}

void CWiresxInfo::UpdateId(void)
{
    uint hash = 0U;
    for (uint i = 0U; i < 14; i++)
    {
        hash += m_name[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    ::sprintf((char *)m_id, "%05u", hash % 100000U);
    UpdateCsds();
}
