//
//  cwiresxcmd.cpp
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

#include "main.h"
#include "cwiresxcmd.h"


////////////////////////////////////////////////////////////////////////////////////////
// constructor

CWiresxCmd::CWiresxCmd()
{
    m_iCmd = WIRESX_CMD_UNKNOWN;
    m_Time.Now();
}

CWiresxCmd::CWiresxCmd(const CIp &Ip, const CCallsign &Callsign, int iCmd, int iArg)
{
    m_Ip = Ip;
    m_Callsign = Callsign;
    m_iCmd = iCmd;
    m_iArg = iArg;
    m_Time.Now();
}

CWiresxCmd::CWiresxCmd(const CWiresxCmd &Cmd)
{
    m_Ip = Cmd.m_Ip;
    m_Callsign = Cmd.m_Callsign;
    m_iCmd = Cmd.m_iCmd;
    m_iArg = Cmd.m_iArg;
    m_Time = Cmd.m_Time;
}
