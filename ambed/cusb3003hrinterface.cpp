//
//  cusb3003hrinterface.cpp
//  ambed
//
//  Created by Jean-Luc Deltombe (LX3JL) on 30/10/2017.
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
#include "ctimepoint.h"
#include "cambepacket.h"
#include "cusb3003hrinterface.h"
#include "cvocodecs.h"

////////////////////////////////////////////////////////////////////////////////////////
// constructor

CUsb3003HRInterface::CUsb3003HRInterface(uint32 uiVid, uint32 uiPid, const char *szDeviceName, const char *szDeviceSerial)
: CUsb3003Interface(uiVid, uiPid, szDeviceName, szDeviceSerial)
{
}

////////////////////////////////////////////////////////////////////////////////////////
// low level

bool CUsb3003HRInterface::ResetDevice(void)
{
    bool ok = false;
    FT_STATUS ftStatus;
    int len;
    char rxpacket[100];
    
    //if the device is a USB-3003, it supports reset via UART break signal
    //printf("reset via uart break...\n");
    ftStatus = FT_SetBreakOn( m_FtdiHandle );
    CTimePoint::TaskSleepFor(10);
    ftStatus = FT_SetBreakOff( m_FtdiHandle );
    //CTimePoint::TaskSleepFor(10);
    
    len = FTDI_read_packet( m_FtdiHandle, rxpacket, sizeof(rxpacket) );
    ok = ((len == 7) && (rxpacket[4] == PKT_READY));
    if ( !ok )
    {
        std::cout << "USB-3003 hard reset failed" << std::endl;
    }
    
    // done
    return ok;
}

