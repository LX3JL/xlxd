//
//  cusb3000baointerface.cpp
//  ambed
//
//  Created by Jean-Luc Deltombe (LX3JL) on 21/08/2017.
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
#include "cusb3000baointerface.h"
#include "cvocodecs.h"

////////////////////////////////////////////////////////////////////////////////////////
// constructor

CUsb3000BaoInterface::CUsb3000BaoInterface(uint32 uiVid, uint32 uiPid, const char *szDeviceName, const char *szDeviceSerial)
: CUsb3000Interface(uiVid, uiPid, szDeviceName, szDeviceSerial)
{
}

////////////////////////////////////////////////////////////////////////////////////////
// low level

bool CUsb3000BaoInterface::OpenDevice(void)
{
    FT_STATUS ftStatus;
    int baudrate = 460800;
    
    //sets serial VID/PID for a Standard Device NOTE:  This is for legacy purposes only.  This can be ommitted.
    ftStatus = FT_SetVIDPID(m_uiVid, m_uiPid);
    if (ftStatus != FT_OK) {FTDI_Error((char *)"FT_SetVIDPID", ftStatus ); return false; }
    
    ftStatus = FT_OpenEx((PVOID)m_szDeviceSerial, FT_OPEN_BY_SERIAL_NUMBER, &m_FtdiHandle);
    if (ftStatus != FT_OK) { FTDI_Error((char *)"FT_OpenEx", ftStatus ); return false; }
    
    CTimePoint::TaskSleepFor(50);
    FT_Purge(m_FtdiHandle, FT_PURGE_RX | FT_PURGE_TX );
    CTimePoint::TaskSleepFor(50);
    
    ftStatus = FT_SetDataCharacteristics(m_FtdiHandle, FT_BITS_8, FT_STOP_BITS_1, FT_PARITY_NONE);
    if ( ftStatus != FT_OK ) { FTDI_Error((char *)"FT_SetDataCharacteristics", ftStatus ); return false; }
    
    ftStatus = FT_SetFlowControl(m_FtdiHandle, FT_FLOW_NONE, 0x11, 0x13);
    if (ftStatus != FT_OK) { FTDI_Error((char *)"FT_SetFlowControl", ftStatus ); return false; }
    
    ftStatus = FT_SetRts (m_FtdiHandle);
    if (ftStatus != FT_OK) { FTDI_Error((char *)"FT_SetRts", ftStatus ); return false; }
    
    ftStatus = FT_ClrDtr(m_FtdiHandle);
    if (ftStatus != FT_OK) { FTDI_Error((char *)"FT_ClrDtr", ftStatus); return false; }
    
    ftStatus = FT_SetBaudRate(m_FtdiHandle, baudrate );
    if (ftStatus != FT_OK) { FTDI_Error((char *)"FT_SetBaudRate", ftStatus ); return false; }
    
    ftStatus = FT_SetLatencyTimer(m_FtdiHandle, 4);
    if (ftStatus != FT_OK) { FTDI_Error((char *)"FT_SetLatencyTimer", ftStatus ); return false; }
    
    ftStatus = FT_SetUSBParameters(m_FtdiHandle, USB3XXX_MAXPACKETSIZE, 0);
    if (ftStatus != FT_OK) { FTDI_Error((char *)"FT_SetUSBParameters", ftStatus ); return false; }
    
    ftStatus = FT_SetTimeouts(m_FtdiHandle, 500, 500 );
    if (ftStatus != FT_OK) { FTDI_Error((char *)"FT_SetTimeouts", ftStatus ); return false; }
    
    // done
    return true;
}
