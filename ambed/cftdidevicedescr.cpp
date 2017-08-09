//
//  cftdidevicedescr.cpp
//  ambed
//
//  Created by Jean-Luc Deltombe (LX3JL) on 02/06/2017.
//  Copyright © 2015 Jean-Luc Deltombe (LX3JL). All rights reserved.
//
// ----------------------------------------------------------------------------
//    This file is part of ambed.
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
#include <string.h>
#include "cftdidevicedescr.h"


////////////////////////////////////////////////////////////////////////////////////////
// constructor

CFtdiDeviceDescr::CFtdiDeviceDescr(void)
{
    m_bUsed = false;
    m_uiVid = 0;
    m_uiPid = 0;
    ::memset(m_szDescription, 0, sizeof(m_szDescription));
    ::memset(m_szSerial, 0, sizeof(m_szSerial));
}

CFtdiDeviceDescr::CFtdiDeviceDescr(uint32 uiVid, uint32 uiPid, const char *szDescription, const char *szSerial)
{
    m_bUsed = false;
    m_uiVid = uiVid;
    m_uiPid = uiPid;
    ::strcpy(m_szDescription, szDescription);
    ::strcpy(m_szSerial, szSerial);
}

CFtdiDeviceDescr::CFtdiDeviceDescr(const CFtdiDeviceDescr &descr)
{
    m_bUsed = descr.m_bUsed;
    m_uiVid = descr.m_uiVid;
    m_uiPid = descr.m_uiPid;
    ::memcpy(m_szDescription, descr.m_szDescription, sizeof(m_szDescription));
    ::memcpy(m_szSerial, descr.m_szSerial, sizeof(m_szSerial));
}
