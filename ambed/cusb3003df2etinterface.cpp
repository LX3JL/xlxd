//
//  cusb3003df2etinterface.cpp
//  ambed
//
//  Created by Jean-Luc Deltombe (LX3JL) and Florian Wolters (DF2ET) on 03/11/2017.
//  Copyright © 2017 Jean-Luc Deltombe (LX3JL) and Florian Wolters (DF2ET).
//  All rights reserved.
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
//  Created by Florian Wolters (DF2ET) on 03/11/2017.
//  Copyright © 2017 Florian Wolters (DF2ET). All rights reserved.

#include "main.h"
#include "ctimepoint.h"
#include "cambepacket.h"
#include "cusb3003df2etinterface.h"
#include "cvocodecs.h"

////////////////////////////////////////////////////////////////////////////////////////
// constructor

CUsb3003DF2ETInterface::CUsb3003DF2ETInterface(uint32 uiVid, uint32 uiPid, const char *szDeviceName, const char *szDeviceSerial)
: CUsb3003Interface(uiVid, uiPid, szDeviceName, szDeviceSerial)
{
}

////////////////////////////////////////////////////////////////////////////////////////
// low level

bool CUsb3003DF2ETInterface::OpenDevice(void)
{
    FT_STATUS ftStatus;
    int baudrate = 921600;
    
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
    
    ftStatus = FT_SetFlowControl(m_FtdiHandle, FT_FLOW_RTS_CTS, 0x11, 0x13);
    if (ftStatus != FT_OK) { FTDI_Error((char *)"FT_SetFlowControl", ftStatus ); return false; }
    
    ftStatus = FT_SetRts (m_FtdiHandle);
    if (ftStatus != FT_OK) { FTDI_Error((char *)"FT_SetRts", ftStatus ); return false; }
    
    // for DF2ET-3003 interface pull DTR low to take AMBE3003 out of reset.
    ftStatus = FT_SetDtr( m_FtdiHandle );
    CTimePoint::TaskSleepFor(50);
    if (ftStatus != FT_OK) { FTDI_Error((char *)"FT_SetDtr", ftStatus); return false; }
    
    ftStatus = FT_SetBaudRate(m_FtdiHandle, baudrate );
    if (ftStatus != FT_OK) { FTDI_Error((char *)"FT_SetBaudRate", ftStatus ); return false; }
    
    ftStatus = FT_SetLatencyTimer(m_FtdiHandle, 4);
    if (ftStatus != FT_OK) { FTDI_Error((char *)"FT_SetLatencyTimer", ftStatus ); return false; }
    
    ftStatus = FT_SetUSBParameters(m_FtdiHandle, USB3XXX_MAXPACKETSIZE, 0);
    if (ftStatus != FT_OK) { FTDI_Error((char *)"FT_SetUSBParameters", ftStatus ); return false; }
    
    ftStatus = FT_SetTimeouts(m_FtdiHandle, 200, 200 );
    if (ftStatus != FT_OK) { FTDI_Error((char *)"FT_SetTimeouts", ftStatus ); return false; }
    
    // done
    return true;
}

bool CUsb3003DF2ETInterface::ResetDevice(void)
{
    bool ok = false;
    FT_STATUS ftStatus;
    int len, i;
    char rxpacket[100];
    
    std::cout << "Trying DF2ET-3003 soft reset" << std::endl;
    
    DWORD n, b;
    char txpacket[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    char reset_packet[7] = { PKT_HEADER, 0, 3, 0, PKT_RESET, PKT_PARITYBYTE, 3 ^ PKT_RESET ^ PKT_PARITYBYTE };
    char *p;
    
    for (i = 0; i < 35; i++)
    {
        p = &txpacket[0];
        n = 10;
        do
        {
            ftStatus = FT_Write( m_FtdiHandle, p, n, &b);
            if (FT_OK != ftStatus)
            {
                return 1;
            }
            n -= b;
            p += b;
        } while (n > 0);
    }
    
    p = &reset_packet[0];
    n = 7;
    do
    {
        ftStatus = FT_Write( m_FtdiHandle, p, n, &b);
        if (FT_OK != ftStatus)
        {
            return 1;
        }
        n -= b;
        p += b;
    } while (n > 0);
    
    len = FTDI_read_packet( m_FtdiHandle, rxpacket, sizeof(rxpacket) );
    ok = ((len == 7) && (rxpacket[4] == PKT_READY));
    if ( ok )
    {
        std::cout << "DF2ET-3003 soft reset succeeded" << std::endl;
    }
    else
    {
        std::cout << "DF2ET-3003 soft reset failed" << std::endl;
        
        std::cout << "Trying DF2ET-3003 hard reset" << std::endl;
        
        ftStatus = FT_ClrDtr( m_FtdiHandle );
        CTimePoint::TaskSleepFor(10);
        ftStatus = FT_SetDtr( m_FtdiHandle );
        CTimePoint::TaskSleepFor(10);
        
        len = FTDI_read_packet( m_FtdiHandle, rxpacket, sizeof(rxpacket) );
        ok = ((len == 7) && (rxpacket[4] == PKT_READY));
        if ( ok )
        {
            std::cout << "DF2ET-3003 hard reset succeeded" << std::endl;
        }
        else
        {
            std::cout << "DF2ET-3003 hard reset failed" << std::endl;
        }
    }
    return ok;
}

