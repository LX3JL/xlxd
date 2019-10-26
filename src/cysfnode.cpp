//
//  cysfnode.cpp
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

#include <string.h>
#include "main.h"
#include "cysfnode.h"


////////////////////////////////////////////////////////////////////////////////////////
///// constructor

CYsfNode::CYsfNode()
{
    m_uiTxFreq = 0;
    m_uiRxFreq = 0;
}

CYsfNode::CYsfNode(const CCallsign &callsign, uint32 txfreq, uint32 rxfreq)
{
    m_Callsign = callsign;
    m_uiTxFreq = txfreq;
    m_uiRxFreq = rxfreq;
}

CYsfNode::CYsfNode(const CYsfNode &node)
{
    m_Callsign = node.m_Callsign;
    m_uiTxFreq = node.m_uiTxFreq;
    m_uiRxFreq = node.m_uiRxFreq;
}


////////////////////////////////////////////////////////////////////////////////////////
// get

bool CYsfNode::IsValid(void) const
{
    return m_Callsign.IsValid();
}
