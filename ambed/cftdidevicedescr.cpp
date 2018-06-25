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
#include "cusb3000interface.h"
#include "cusb3003interface.h"
#include "cusb3003hrinterface.h"
#include "cusb3003df2etinterface.h"
#include "cusb3000baointerface.h"
#include "cusb3000baofarminterface.h"
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

////////////////////////////////////////////////////////////////////////////////////////
// interface factory

int CFtdiDeviceDescr::CreateInterface(CFtdiDeviceDescr *descr, std::vector<CVocodecChannel *>*channels)
{
    int iNbChs = 0;
    
    // single channel devices cannot be created alone
    // three channels devices
    if ( (::strcmp(descr->GetDescription(), "USB-3003")    == 0) ||     // DVSI's  USB-3003
         (::strcmp(descr->GetDescription(), "DF2ET-3003")  == 0) ||     // DF2ET's USB-3003 opensource device
         (::strcmp(descr->GetDescription(), "DVstick-33")  == 0) ||     // DVMEGA USB-3003 device  
         (::strcmp(descr->GetDescription(), "ThumbDV-3")   == 0) )      // ThumbDV-3
    {
        iNbChs = CreateUsb3003(descr, channels);
    }
    // four channels devices
    else if ( (::strcmp(descr->GetDescription(), "BAOFARM1 A") == 0) )  // Team6160 AMBE3000 x4
    {
        iNbChs = CreateBaoFarm(descr, channels);
    }
    // six channels devices
    else if ( (::strcmp(descr->GetDescription(), "USB-3006 A") == 0) )  // LX3JL's USB-3006 opensource device
    {
        iNbChs = CreateUsb3006(descr, channels);
    }
    // twelves channels devices
    else if ( (::strcmp(descr->GetDescription(), "USB-3012 A") == 0) )  // DVSI's USB-3012
    {
        iNbChs = CreateUsb3012(descr, channels);
    }
    // done
    return iNbChs;
}

int CFtdiDeviceDescr::CreateInterfacePair(CFtdiDeviceDescr *descr1, CFtdiDeviceDescr *descr2, std::vector<CVocodecChannel *>*channels)
{
    int iNbChs = 0;

    // create interface objects
    if ( (descr1->GetNbChannels() == 1) && (descr2->GetNbChannels() == 1) )
    {
        // create 3000-3000 pair
        CUsb3000Interface *Usb3000A = InstantiateUsb3000(descr1);
        CUsb3000Interface *Usb3000B = InstantiateUsb3000(descr2);
        iNbChs = CreatePair(Usb3000A, Usb3000B, channels);
    }
    else if ( (descr1->GetNbChannels() == 3) && (descr2->GetNbChannels() == 1) )
    {
        // create 3003-3000 pair
        CUsb3003Interface *Usb3003 = InstantiateUsb3003(descr1);
        CUsb3000Interface *Usb3000 = InstantiateUsb3000(descr2);
        iNbChs = CreatePair(Usb3003, Usb3000, channels);
   }
    else if ( (descr1->GetNbChannels() == 1) && (descr2->GetNbChannels() == 3) )
    {
        // create 3000-3003 pair
        CUsb3000Interface *Usb3000 = InstantiateUsb3000(descr1);
        CUsb3003Interface *Usb3003 = InstantiateUsb3003(descr2);
        iNbChs = CreatePair(Usb3003, Usb3000, channels);
    }
    else if ( (descr1->GetNbChannels() == 3) && (descr2->GetNbChannels() == 3) )
    {
        // create 3003-3003 pair
        CUsb3003Interface *Usb3003A = InstantiateUsb3003(descr1);
        CUsb3003Interface *Usb3003B = InstantiateUsb3003(descr2);
        iNbChs = CreatePair(Usb3003A, Usb3003B, channels);
    }
    
    // done
    return iNbChs;
}

////////////////////////////////////////////////////////////////////////////////////////
// get

const char * CFtdiDeviceDescr::GetChannelDescription(int ch) const
{
    static char descr[FTDI_MAX_STRINGLENGTH];
    char tag[3] = "_X";
    
    ::strcpy(descr, GetDescription());
    if ( ::strlen(descr) >= 2 )
    {
        descr[::strlen(descr)-2] = 0x00;
        tag[1] = (char)ch + 'A';
        ::strcat(descr, tag);
    }
    return descr;
}

const char * CFtdiDeviceDescr::GetChannelSerialNumber(int ch) const
{
    static char serial[FTDI_MAX_STRINGLENGTH];
    
    ::strcpy(serial, GetSerialNumber());
    if ( ::strlen(serial) >= 1 )
    {
        serial[::strlen(serial)-1] = (char)ch + 'A';
    }
    return serial;
}

int CFtdiDeviceDescr::GetNbChannels(void) const
{
    int iNbChs = 0;
    
    // single channel devices
    if ( (::strcmp(m_szDescription, "USB-3000")   == 0) ||           // DVSI's USB-3000
         (::strcmp(m_szDescription, "DVstick-30") == 0) ||           // DVMEGA AMBE3000 device
         (::strcmp(m_szDescription, "ThumbDV")    == 0) ||           // ThumbDV
         (::strcmp(m_szDescription, "BAO3000")    == 0) )            // Team6160 AMBE3000
    {
        iNbChs = 1;
    }
    // three channels devices
    else if ( (::strcmp(m_szDescription, "USB-3003")    == 0) ||     // DVSI's  USB-3003
              (::strcmp(m_szDescription, "DF2ET-3003")  == 0) ||     // DF2ET's USB-3003 opensource device
              (::strcmp(m_szDescription, "DVstick-33")  == 0) ||     // DVMEGA AMBE 3003 device
              (::strcmp(m_szDescription, "ThumbDV-3")   == 0) )      // ThumbDV-3
    {
        iNbChs = 3;
    }
    // four channels devices
    else if ( (::strcmp(m_szDescription, "BAOFARM1 A") == 0) )       // Team6160 AMBE3000 x4
    {
        iNbChs = 4;
    }
    // six channels devices
    else if ( (::strcmp(m_szDescription, "USB-3006 A") == 0) )       // LX3JL's USB-3006 opensource device
    {
        iNbChs = 6;
    }
    // twelves channels devices
    else if ( (::strcmp(m_szDescription, "USB-3012 A") == 0) )       // DVSI's USB-3012
    {
        iNbChs = 12;
    }
  
    // done
    return iNbChs;
}

////////////////////////////////////////////////////////////////////////////////////////
// DVSI's USB-3012 factory helper
//
//      This device uses 3 AMBE3003 connected on a single FTDI 4 channels
//      USB to serial interface. The reset mechanism is based
//      on DTR and SetBreak. Baudrate is 921600
//

int CFtdiDeviceDescr::CreateUsb3012(CFtdiDeviceDescr *descr, std::vector<CVocodecChannel *>*channels)
{
    int nStreams = 0;
    
    // create the interfaces for the four 3003 chips
    CUsb3003HRInterface *Usb3003A =
        new CUsb3003HRInterface(descr->GetVid(), descr->GetPid(), descr->GetChannelDescription(0), descr->GetChannelSerialNumber(0));
    CUsb3003HRInterface *Usb3003B =
        new CUsb3003HRInterface(descr->GetVid(), descr->GetPid(), descr->GetChannelDescription(1), descr->GetChannelSerialNumber(1));
    CUsb3003HRInterface *Usb3003C =
        new CUsb3003HRInterface(descr->GetVid(), descr->GetPid(), descr->GetChannelDescription(2), descr->GetChannelSerialNumber(2));
    CUsb3003HRInterface *Usb3003D =
        new CUsb3003HRInterface(descr->GetVid(), descr->GetPid(), descr->GetChannelDescription(3), descr->GetChannelSerialNumber(3));
    
    // init the interfaces
    if ( Usb3003A->Init(CODEC_AMBEPLUS) && Usb3003B->Init(CODEC_AMBE2PLUS) &&
         Usb3003C->Init(CODEC_AMBEPLUS) && Usb3003D->Init(CODEC_AMBE2PLUS) )
    {
        CVocodecChannel *Channel;
        // create all channels
        {
            // ch1
            Channel = new CVocodecChannel(Usb3003A, 0, Usb3003A, 1, CODECGAIN_AMBEPLUS);
            channels->push_back(Channel);
            Usb3003A->AddChannel(Channel);
            // ch2
            Channel = new CVocodecChannel(Usb3003A, 1, Usb3003A, 0, CODECGAIN_AMBE2PLUS);
            channels->push_back(Channel);
            Usb3003A->AddChannel(Channel);
            // ch3
            Channel = new CVocodecChannel(Usb3003B, 0, Usb3003B, 1, CODECGAIN_AMBEPLUS);
            channels->push_back(Channel);
            Usb3003B->AddChannel(Channel);
            // ch4
            Channel = new CVocodecChannel(Usb3003B, 1, Usb3003B, 0, CODECGAIN_AMBE2PLUS);
            channels->push_back(Channel);
            Usb3003B->AddChannel(Channel);
            // ch5
            Channel = new CVocodecChannel(Usb3003A, 2, Usb3003B, 2, CODECGAIN_AMBEPLUS);
            channels->push_back(Channel);
            Usb3003A->AddChannel(Channel);
            Usb3003B->AddChannel(Channel);
            // ch6
            Channel = new CVocodecChannel(Usb3003B, 2, Usb3003A, 2, CODECGAIN_AMBE2PLUS);
            channels->push_back(Channel);
            Usb3003A->AddChannel(Channel);
            Usb3003B->AddChannel(Channel);
            // ch7
            Channel = new CVocodecChannel(Usb3003C, 0, Usb3003C, 1, CODECGAIN_AMBEPLUS);
            channels->push_back(Channel);
            Usb3003C->AddChannel(Channel);
            // ch8
            Channel = new CVocodecChannel(Usb3003C, 1, Usb3003C, 0, CODECGAIN_AMBE2PLUS);
            channels->push_back(Channel);
            Usb3003C->AddChannel(Channel);
            // ch9
            Channel = new CVocodecChannel(Usb3003D, 0, Usb3003D, 1, CODECGAIN_AMBEPLUS);
            channels->push_back(Channel);
            Usb3003D->AddChannel(Channel);
            // ch10
            Channel = new CVocodecChannel(Usb3003D, 1, Usb3003D, 0, CODECGAIN_AMBE2PLUS);
            channels->push_back(Channel);
            Usb3003D->AddChannel(Channel);
            // ch11
            Channel = new CVocodecChannel(Usb3003C, 2, Usb3003D, 2, CODECGAIN_AMBEPLUS);
            channels->push_back(Channel);
            Usb3003C->AddChannel(Channel);
            Usb3003D->AddChannel(Channel);
            // ch12
            Channel = new CVocodecChannel(Usb3003D, 2, Usb3003C, 2, CODECGAIN_AMBE2PLUS);
            channels->push_back(Channel);
            Usb3003C->AddChannel(Channel);
            Usb3003D->AddChannel(Channel);
            //done
            nStreams = 12;
        }
    }
    else
    {
        // cleanup
        delete Usb3003A;
        delete Usb3003B;
        delete Usb3003C;
        delete Usb3003D;
    }
    
    // done
    return nStreams;
}

////////////////////////////////////////////////////////////////////////////////////////
// Bao Farm1 AMBE3000 x4 factory helper
//
//      This device uses 4 AMBE3000 connected on a single FTDI 4 channels
//      USB to serial interface. Baudrate is 460800
//
int CFtdiDeviceDescr::CreateBaoFarm(CFtdiDeviceDescr *descr, std::vector<CVocodecChannel *>*channels)
{
    int nStreams = 0;
    
    // create the interfaces for the four 3000 chips
    CUsb3000BaoFarmInterface *UsbBaoFarmA =
        new CUsb3000BaoFarmInterface(descr->GetVid(), descr->GetPid(), "BAOFARM1_A", descr->GetSerialNumber());
    CUsb3000BaoFarmInterface *UsbBaoFarmB =
        new CUsb3000BaoFarmInterface(descr->GetVid(), descr->GetPid(), "BAOFARM1_B", descr->GetSerialNumber());
    CUsb3000BaoFarmInterface *UsbBaoFarmC =
        new CUsb3000BaoFarmInterface(descr->GetVid(), descr->GetPid(), "BAOFARM1_C", descr->GetSerialNumber());
    CUsb3000BaoFarmInterface *UsbBaoFarmD =
        new CUsb3000BaoFarmInterface(descr->GetVid(), descr->GetPid(), "BAOFARM1_D", descr->GetSerialNumber());

    // init the interfaces
    if ( UsbBaoFarmA->Init(CODEC_AMBEPLUS) && UsbBaoFarmB->Init(CODEC_AMBE2PLUS) && UsbBaoFarmC->Init(CODEC_AMBEPLUS) && UsbBaoFarmD->Init(CODEC_AMBE2PLUS))
    {
        CVocodecChannel *Channel;
        // create all channels
        {
            // ch1
            Channel = new CVocodecChannel(UsbBaoFarmA, 0, UsbBaoFarmB, 0, CODECGAIN_AMBEPLUS);
            channels->push_back(Channel);
            UsbBaoFarmA->AddChannel(Channel);
            UsbBaoFarmB->AddChannel(Channel);
            // ch2
            Channel = new CVocodecChannel(UsbBaoFarmB, 0, UsbBaoFarmA, 0, CODECGAIN_AMBE2PLUS);
            channels->push_back(Channel);
            UsbBaoFarmA->AddChannel(Channel);
            UsbBaoFarmB->AddChannel(Channel);
            // ch3
            Channel = new CVocodecChannel(UsbBaoFarmC, 0, UsbBaoFarmD, 0, CODECGAIN_AMBEPLUS);
            channels->push_back(Channel);
            UsbBaoFarmC->AddChannel(Channel);
            UsbBaoFarmD->AddChannel(Channel);
            // ch4
            Channel = new CVocodecChannel(UsbBaoFarmD, 0, UsbBaoFarmC, 0, CODECGAIN_AMBE2PLUS);
            channels->push_back(Channel);
            UsbBaoFarmC->AddChannel(Channel);
            UsbBaoFarmD->AddChannel(Channel);
            //done
            nStreams = 4;
        }
    }
    else
    {
        // cleanup
        delete UsbBaoFarmA;
        delete UsbBaoFarmB;
        delete UsbBaoFarmC;
        delete UsbBaoFarmD;
    }
    
    // done
    return nStreams;
}

////////////////////////////////////////////////////////////////////////////////////////
// LX3JL's USB-3006 factory helper
//
//      This device uses 2 AMBE3003 connected on a single FTDI 2 channels
//      USB to serial interface. The reset mechanism is based
//      on DTR and software reset. Baudrate is 921600
//
int CFtdiDeviceDescr::CreateUsb3006(CFtdiDeviceDescr *descr, std::vector<CVocodecChannel *>*channels)
{
    int  nStreams = 0;
    
    // create the interfaces for the two 3003 chips
    CUsb3003Interface *Usb3003A =
        new CUsb3003Interface(descr->GetVid(), descr->GetPid(), descr->GetChannelDescription(0), descr->GetChannelSerialNumber(0));
    CUsb3003Interface *Usb3003B =
        new CUsb3003Interface(descr->GetVid(), descr->GetPid(), descr->GetChannelDescription(1), descr->GetChannelSerialNumber(1));
    
    // init the interfaces
    if ( Usb3003A->Init(CODEC_AMBEPLUS) && Usb3003B->Init(CODEC_AMBE2PLUS) )
    {
        CVocodecChannel *Channel;
        // create all channels
        {
            // ch1
            Channel = new CVocodecChannel(Usb3003A, 0, Usb3003A, 1, CODECGAIN_AMBEPLUS);
            channels->push_back(Channel);
            Usb3003A->AddChannel(Channel);
            // ch2
            Channel = new CVocodecChannel(Usb3003A, 1, Usb3003A, 0, CODECGAIN_AMBE2PLUS);
            channels->push_back(Channel);
            Usb3003A->AddChannel(Channel);
            // ch3
            Channel = new CVocodecChannel(Usb3003B, 0, Usb3003B, 1, CODECGAIN_AMBEPLUS);
            channels->push_back(Channel);
            Usb3003B->AddChannel(Channel);
            // ch4
            Channel = new CVocodecChannel(Usb3003B, 1, Usb3003B, 0, CODECGAIN_AMBE2PLUS);
            channels->push_back(Channel);
            Usb3003B->AddChannel(Channel);
            // ch5
            Channel = new CVocodecChannel(Usb3003A, 2, Usb3003B, 2, CODECGAIN_AMBEPLUS);
            channels->push_back(Channel);
            Usb3003A->AddChannel(Channel);
            Usb3003B->AddChannel(Channel);
            // ch6
            Channel = new CVocodecChannel(Usb3003B, 2, Usb3003A, 2, CODECGAIN_AMBE2PLUS);
            channels->push_back(Channel);
            Usb3003A->AddChannel(Channel);
            Usb3003B->AddChannel(Channel);
            //done
            nStreams = 6;
        }
    }
    else
    {
        // cleanup
        delete Usb3003A;
        delete Usb3003B;
    }
    
    // done
    return nStreams;
}

////////////////////////////////////////////////////////////////////////////////////////
// USB-3003 factory helpers
//   DVSI
//   DF2ET
//   ThumbDV
//   DVMEGA
//
//      These devices uses a AMBE3003 connected on a single FTDI
//      USB to serial interface. The reset mechanism is based
//      on DTR and SetBreak, or software. Baudrate is 921600
//
int CFtdiDeviceDescr::CreateUsb3003(CFtdiDeviceDescr *descr, std::vector<CVocodecChannel *>*channels)
{
    int nStreams = 0;
    
    // create the interfaces for the 3003 chip
    CUsb3003Interface *Usb3003 = InstantiateUsb3003(descr);
    
    // init the interface
    if ( (Usb3003 != NULL) && Usb3003->Init(CODEC_NONE) )
    {
        CVocodecChannel *Channel;
        // create all channels
        {
            // ch1
            Channel = new CVocodecChannel(Usb3003, 0, Usb3003, 1, CODECGAIN_AMBEPLUS);
            channels->push_back(Channel);
            Usb3003->AddChannel(Channel);
            // ch2
            Channel = new CVocodecChannel(Usb3003, 1, Usb3003, 0, CODECGAIN_AMBE2PLUS);
            channels->push_back(Channel);
            Usb3003->AddChannel(Channel);
            // done
            nStreams = 2;
        }
    }
    else
    {
        // cleanup
        delete Usb3003;
    }
    
    // done
    return nStreams;
}


////////////////////////////////////////////////////////////////////////////////////////
// 1 ch + 1 ch pair creation

int CFtdiDeviceDescr::CreatePair(CUsb3000Interface *Usb3000A, CUsb3000Interface *Usb3000B, std::vector<CVocodecChannel *>*channels)
{
    int nStreams = 0;
    
    // init the interfaces
    if ( Usb3000A->Init(CODEC_AMBEPLUS) && Usb3000B->Init(CODEC_AMBE2PLUS) )
    {
        CVocodecChannel *Channel;
        // create all channels
        {
            // ch1
            Channel = new CVocodecChannel(Usb3000A, 0, Usb3000B, 0, CODECGAIN_AMBEPLUS);
            channels->push_back(Channel);
            Usb3000A->AddChannel(Channel);
            Usb3000B->AddChannel(Channel);
            // ch2
            Channel = new CVocodecChannel(Usb3000B, 0, Usb3000A, 0, CODECGAIN_AMBE2PLUS);
            channels->push_back(Channel);
            Usb3000A->AddChannel(Channel);
            Usb3000B->AddChannel(Channel);
            // done
            nStreams = 2;
        }
    }
    else
    {
        // cleanup
        delete Usb3000A;
        delete Usb3000B;
    }
    
    // done
    return nStreams;
   
}

////////////////////////////////////////////////////////////////////////////////////////
// 3 ch + 3 ch pair creation

int CFtdiDeviceDescr::CreatePair(CUsb3003Interface *Usb3003A, CUsb3003Interface *Usb3003B, std::vector<CVocodecChannel *>*channels)
{
    int nStreams = 0;
    
    // init the interfaces
    if ( Usb3003A->Init(CODEC_AMBEPLUS) && Usb3003B->Init(CODEC_AMBE2PLUS) )
    {
        CVocodecChannel *Channel;
        // create all channels
        {
            // ch1
            Channel = new CVocodecChannel(Usb3003A, 0, Usb3003A, 1, CODECGAIN_AMBEPLUS);
            channels->push_back(Channel);
            Usb3003A->AddChannel(Channel);
            // ch2
            Channel = new CVocodecChannel(Usb3003A, 1, Usb3003A, 0, CODECGAIN_AMBE2PLUS);
            channels->push_back(Channel);
            Usb3003A->AddChannel(Channel);
            // ch3
            Channel = new CVocodecChannel(Usb3003B, 0, Usb3003B, 1, CODECGAIN_AMBEPLUS);
            channels->push_back(Channel);
            Usb3003B->AddChannel(Channel);
            // ch4
            Channel = new CVocodecChannel(Usb3003B, 1, Usb3003B, 0, CODECGAIN_AMBE2PLUS);
            channels->push_back(Channel);
            Usb3003B->AddChannel(Channel);
            // ch5
            Channel = new CVocodecChannel(Usb3003A, 2, Usb3003B, 2, CODECGAIN_AMBEPLUS);
            channels->push_back(Channel);
            Usb3003A->AddChannel(Channel);
            Usb3003B->AddChannel(Channel);
            // ch6
            Channel = new CVocodecChannel(Usb3003B, 2, Usb3003A, 2, CODECGAIN_AMBE2PLUS);
            channels->push_back(Channel);
            Usb3003A->AddChannel(Channel);
            Usb3003B->AddChannel(Channel);
            // done
            nStreams = 6;
        }
    }
    else
    {
        // cleanup
        delete Usb3003A;
        delete Usb3003B;
    }
    
    // done
    return nStreams;
}

////////////////////////////////////////////////////////////////////////////////////////
// 3 ch + 1 ch pair creation

int CFtdiDeviceDescr::CreatePair(CUsb3003Interface *Usb3003A, CUsb3000Interface *Usb3000B, std::vector<CVocodecChannel *>*channels)
{
    int nStreams = 0;
    
    // init the interfaces
    if ( Usb3003A->Init(CODEC_AMBEPLUS) && Usb3000B->Init(CODEC_AMBE2PLUS) )
    {
        CVocodecChannel *Channel;
        // create all channels
        {
            // ch1
            Channel = new CVocodecChannel(Usb3003A, 0, Usb3003A, 1, CODECGAIN_AMBEPLUS);
            channels->push_back(Channel);
            Usb3003A->AddChannel(Channel);
            // ch2
            Channel = new CVocodecChannel(Usb3003A, 1, Usb3003A, 0, CODECGAIN_AMBE2PLUS);
            channels->push_back(Channel);
            Usb3003A->AddChannel(Channel);
            // ch3
            Channel = new CVocodecChannel(Usb3003A, 2, Usb3000B, 0, CODECGAIN_AMBEPLUS);
            channels->push_back(Channel);
            Usb3003A->AddChannel(Channel);
            Usb3000B->AddChannel(Channel);
            // ch4
            Channel = new CVocodecChannel(Usb3000B, 0, Usb3003A, 2, CODECGAIN_AMBE2PLUS);
            channels->push_back(Channel);
            Usb3003A->AddChannel(Channel);
            Usb3000B->AddChannel(Channel);
            // done
            nStreams = 4;
        }
    }
    else
    {
        // cleanup
        delete Usb3003A;
        delete Usb3000B;
    }
    
    // done
    return nStreams;
}


////////////////////////////////////////////////////////////////////////////////////////
// interface instantiation helpers

CUsb3003Interface *CFtdiDeviceDescr::InstantiateUsb3003(CFtdiDeviceDescr *descr)
{
    CUsb3003Interface *Usb3003 = NULL;
    
    // intstantiate the proper version of USB-3003
    if ( (::strcmp(descr->GetDescription(), "USB-3003") == 0) )              // DVSI's  USB-3003
    {
        // hardware reset, 921600 bps
        Usb3003 = new CUsb3003HRInterface
            (descr->GetVid(), descr->GetPid(), descr->GetDescription(), descr->GetSerialNumber());
    }
    else if ( (::strcmp(descr->GetDescription(), "DF2ET-3003") == 0) )       // DF2ET's USB-3003 opensource device
    {
        // specific hardware reset, 921600 bps
        Usb3003 = new CUsb3003DF2ETInterface
            (descr->GetVid(), descr->GetPid(), descr->GetDescription(), descr->GetSerialNumber());
    }
    else if ( (::strcmp(descr->GetDescription(), "ThumbDV-3")   == 0) )      // ThumbDV-3
    {
        // software reset, 921600 bps
        Usb3003 = new CUsb3003Interface
            (descr->GetVid(), descr->GetPid(), descr->GetDescription(), descr->GetSerialNumber());
    }
    else if ( (::strcmp(descr->GetDescription(), "DVstick-33")  == 0) )      // DVMEGA AMBE3003 device
    {
        // specific fardware reset, 921600 bps
        Usb3003 = new CUsb3003DF2ETInterface
            (descr->GetVid(), descr->GetPid(), descr->GetDescription(), descr->GetSerialNumber());
    }

    // done
    return Usb3003;
}

CUsb3000Interface *CFtdiDeviceDescr::InstantiateUsb3000(CFtdiDeviceDescr *descr)
{
    CUsb3000Interface *Usb3000 = NULL;
    
    // instantiate the proper version of USB-3000
    if ( (::strcmp(descr->GetDescription(), "USB-3000")  == 0) ||           // DVSI's USB-3000
         (::strcmp(descr->GetDescription(), "DVstick-30")== 0) ||           // DVMEGA AMBE3000 device
         (::strcmp(descr->GetDescription(), "ThumbDV")   == 0) )            // ThumbDV
    {
        Usb3000 = new CUsb3000Interface
            (descr->GetVid(), descr->GetPid(), descr->GetDescription(), descr->GetSerialNumber());
    }
    else if ( (::strcmp(descr->GetDescription(), "BAO3000") == 0) )         // Team6160 AMBE3000
    {
        Usb3000 = new CUsb3000BaoInterface
            (descr->GetVid(), descr->GetPid(), descr->GetDescription(), descr->GetSerialNumber());
    } 
    // done
    return Usb3000;
}
