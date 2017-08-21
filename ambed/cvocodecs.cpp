//
//  cvocodecs.cpp
//  ambed
//
//  Created by Jean-Luc Deltombe (LX3JL) on 23/04/2017.
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
#include "cvocodecs.h"

////////////////////////////////////////////////////////////////////////////////////////
// global object

CVocodecs g_Vocodecs;

////////////////////////////////////////////////////////////////////////////////////////
// constructor

CVocodecs::CVocodecs()
{
    m_Interfaces.reserve(5);
    m_Channels.reserve(20);
    m_FtdiDeviceDescrs.reserve(10);
}

////////////////////////////////////////////////////////////////////////////////////////
// destructor

CVocodecs::~CVocodecs()
{
    // delete channels
    m_MutexChannels.lock();
    {
        for ( int i = 0; i < m_Channels.size(); i++ )
        {
            delete m_Channels[i];
        }
        m_Channels.clear();
    }
    m_MutexChannels.unlock();
    
    // delete interfaces
    m_MutexInterfaces.lock();
    {
        for ( int i = 0; i < m_Interfaces.size(); i++ )
        {
            delete m_Interfaces[i];
        }
        m_Interfaces.clear();
    }
    m_MutexInterfaces.unlock();
    
    // delete ftdi device descriptors
    for ( int i = 0; i < m_FtdiDeviceDescrs.size(); i++ )
    {
        delete m_FtdiDeviceDescrs[i];
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// initialization

bool CVocodecs::Init(void)
{
    bool ok = true;
    int iNbCh = 0;
    
    // discover and add vocodecs interfaces
    DiscoverFtdiDevices();

    // and create interfaces for the discovered devices
    for ( int i = 0; i < m_FtdiDeviceDescrs.size(); i++ )
    {
        // create relevant interface
        if ( m_FtdiDeviceDescrs[i]->IsUsb3012() )
        {
            iNbCh += InitUsb3012(*m_FtdiDeviceDescrs[i]);
            m_FtdiDeviceDescrs[i]->SetUsed(true);
        }
        else if ( m_FtdiDeviceDescrs[i]->IsUsb3003() && !m_FtdiDeviceDescrs[i]->IsUsed() )
        {
            // another unsed USB-3003 avaliable for a pair ?
            bool found = false;
            int j = i+1;
            while ( !found && (j < m_FtdiDeviceDescrs.size()) )
            {
                if ( m_FtdiDeviceDescrs[j]->IsUsb3003() && !m_FtdiDeviceDescrs[i]->IsUsed() )
                {
                    found = true;
                }
                else
                {
                    j++;
                }
            }
            
            // pair ?
            if ( found )
            {
                // yes!
                iNbCh += InitUsb3003Pair(*m_FtdiDeviceDescrs[i], *m_FtdiDeviceDescrs[j]);
                m_FtdiDeviceDescrs[i]->SetUsed(true);
                m_FtdiDeviceDescrs[j]->SetUsed(true);
            }
            else
            {
                // just single
                iNbCh += InitUsb3003(*m_FtdiDeviceDescrs[i]);
                m_FtdiDeviceDescrs[i]->SetUsed(true);
            }
        }
        else if ( m_FtdiDeviceDescrs[i]->IsUsb3000() && !m_FtdiDeviceDescrs[i]->IsUsed() )
        {
            // another unsed USB-3000 avaliable for a pair ?
            bool found = false;
            int j = i+1;
            while ( !found && (j < m_FtdiDeviceDescrs.size()) )
            {
                if ( m_FtdiDeviceDescrs[j]->IsUsb3000() && !m_FtdiDeviceDescrs[i]->IsUsed() )
                {
                    found = true;
                }
                else
                {
                    j++;
                }
            }
            
            // pair ?
            if ( found )
            {
                // yes!
                iNbCh += InitUsb3000Pair(*m_FtdiDeviceDescrs[i], *m_FtdiDeviceDescrs[j]);
                m_FtdiDeviceDescrs[i]->SetUsed(true);
                m_FtdiDeviceDescrs[j]->SetUsed(true);
            }
            // otherwise anonther unused USB-3003 for a pair ?            
       }
    }
    
    if ( ok )
    {
        std::cout << "Codec interfaces initialized successfully : " << iNbCh << " channels availables" << std::endl;
    }
    else
    {
        std::cout << "At least one codec interfaces failed to initialize : " << iNbCh << " channels availables" << std::endl;
    }
    // done
    return ok;
}

////////////////////////////////////////////////////////////////////////////////////////
// initialisation helpers

bool CVocodecs::DiscoverFtdiDevices(void)
{
    bool ok = false;
    int iNbDevices = 0;
    FT_DEVICE_LIST_INFO_NODE *list;
    
    // clear vector
    for ( int i = 0; i < m_FtdiDeviceDescrs.size(); i++ )
    {
        delete m_FtdiDeviceDescrs[i];
    }
    
    // and discover
    if ( FT_CreateDeviceInfoList((LPDWORD)&iNbDevices) == FT_OK )
    {
        std::cout << "Detected " << iNbDevices << " USB-FTDI devices" << std::endl << std::endl;
        ok = true;
        if ( iNbDevices > 0 )
        {
            // allocate the list
            list = new FT_DEVICE_LIST_INFO_NODE[iNbDevices];
            
            // fill
            if ( FT_GetDeviceInfoList(list, (LPDWORD)&iNbDevices) == FT_OK )
            {
                // process
                for ( int i = 0; i < iNbDevices; i++ )
                {
                    std::cout << "Description : " << list[i].Description << "\t Serial : " << list[i].SerialNumber << std::endl;
                    CFtdiDeviceDescr *descr = new CFtdiDeviceDescr(
                        LOWORD(list[i].ID), HIWORD(list[i].ID),
                        list[i].Description, list[i].SerialNumber);
                    m_FtdiDeviceDescrs.push_back(descr);
                }
            }
            else
            {
                ok = false;
            }

            // and delete
            delete list;
        }
    }
    
    // done
    return ok;
}

int CVocodecs::InitUsb3012(const CFtdiDeviceDescr &descr)
{
    int nStreams = 0;
    
    // create the interfaces for the four 3003 chips
    CUsb3003Interface *Usb3003A = new CUsb3003Interface(descr.GetVid(), descr.GetPid(), "USB-3012_A", descr.GetSerialNumber());
    CUsb3003Interface *Usb3003B = new CUsb3003Interface(descr.GetVid(), descr.GetPid(), "USB-3012_B", descr.GetSerialNumber());
    CUsb3003Interface *Usb3003C = new CUsb3003Interface(descr.GetVid(), descr.GetPid(), "USB-3012_C", descr.GetSerialNumber());
    CUsb3003Interface *Usb3003D = new CUsb3003Interface(descr.GetVid(), descr.GetPid(), "USB-3012_D", descr.GetSerialNumber());
    
    // init the interfaces
    if ( Usb3003A->Init(CODEC_AMBEPLUS) && Usb3003B->Init(CODEC_AMBE2PLUS) &&
         Usb3003C->Init(CODEC_AMBEPLUS) && Usb3003D->Init(CODEC_AMBE2PLUS) )
    {
        CVocodecChannel *Channel;
        // create all channels
        {
            // ch1
            Channel = new CVocodecChannel(Usb3003A, 0, Usb3003A, 1, CODECGAIN_AMBEPLUS);
            m_Channels.push_back(Channel);
            Usb3003A->AddChannel(Channel);
            // ch2
            Channel = new CVocodecChannel(Usb3003A, 1, Usb3003A, 0, CODECGAIN_AMBE2PLUS);
            m_Channels.push_back(Channel);
            Usb3003A->AddChannel(Channel);
            // ch3
            Channel = new CVocodecChannel(Usb3003B, 0, Usb3003B, 1, CODECGAIN_AMBEPLUS);
            m_Channels.push_back(Channel);
            Usb3003B->AddChannel(Channel);
            // ch4
            Channel = new CVocodecChannel(Usb3003B, 1, Usb3003B, 0, CODECGAIN_AMBE2PLUS);
            m_Channels.push_back(Channel);
            Usb3003B->AddChannel(Channel);
            // ch5
            Channel = new CVocodecChannel(Usb3003C, 0, Usb3003C, 1, CODECGAIN_AMBEPLUS);
            m_Channels.push_back(Channel);
            Usb3003C->AddChannel(Channel);
            // ch6
            Channel = new CVocodecChannel(Usb3003C, 1, Usb3003C, 0, CODECGAIN_AMBE2PLUS);
            m_Channels.push_back(Channel);
            Usb3003C->AddChannel(Channel);
            // ch7
            Channel = new CVocodecChannel(Usb3003D, 0, Usb3003D, 1, CODECGAIN_AMBEPLUS);
            m_Channels.push_back(Channel);
            Usb3003D->AddChannel(Channel);
            // ch8
            Channel = new CVocodecChannel(Usb3003D, 1, Usb3003D, 0, CODECGAIN_AMBE2PLUS);
            m_Channels.push_back(Channel);
            Usb3003D->AddChannel(Channel);
            // ch9
            Channel = new CVocodecChannel(Usb3003A, 2, Usb3003B, 2, CODECGAIN_AMBEPLUS);
            m_Channels.push_back(Channel);
            Usb3003A->AddChannel(Channel);
            Usb3003B->AddChannel(Channel);
            // ch10
            Channel = new CVocodecChannel(Usb3003B, 2, Usb3003A, 2, CODECGAIN_AMBE2PLUS);
            m_Channels.push_back(Channel);
            Usb3003A->AddChannel(Channel);
            Usb3003B->AddChannel(Channel);
            // ch11
            Channel = new CVocodecChannel(Usb3003C, 2, Usb3003D, 2, CODECGAIN_AMBEPLUS);
            m_Channels.push_back(Channel);
            Usb3003C->AddChannel(Channel);
            Usb3003D->AddChannel(Channel);
            // ch12
            Channel = new CVocodecChannel(Usb3003D, 2, Usb3003C, 2, CODECGAIN_AMBE2PLUS);
            m_Channels.push_back(Channel);
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

int CVocodecs::InitUsb3003(const CFtdiDeviceDescr &descr)
{
    int nStreams = 0;
    
    // create the interfaces for the 3003 chip
    CUsb3003Interface *Usb3003 = new CUsb3003Interface(descr.GetVid(), descr.GetPid(), "USB-3003", descr.GetSerialNumber());
    
    // init the interface
    if ( Usb3003->Init(CODEC_NONE) )
    {
        CVocodecChannel *Channel;
        // create all channels
        {
            // ch1
            Channel = new CVocodecChannel(Usb3003, 0, Usb3003, 1, CODECGAIN_AMBEPLUS);
            m_Channels.push_back(Channel);
            Usb3003->AddChannel(Channel);
            // ch2
            Channel = new CVocodecChannel(Usb3003, 1, Usb3003, 0, CODECGAIN_AMBE2PLUS);
            m_Channels.push_back(Channel);
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

int CVocodecs::InitUsb3003Pair(const CFtdiDeviceDescr &descr1, const CFtdiDeviceDescr &descr2)
{
    int nStreams = 0;
    
    // create the interfaces for the two 3003 chips
    CUsb3003Interface *Usb3003A = new CUsb3003Interface(descr1.GetVid(), descr1.GetPid(), "USB-3003", descr1.GetSerialNumber());
    CUsb3003Interface *Usb3003B = new CUsb3003Interface(descr2.GetVid(), descr2.GetPid(), "USB-3003", descr2.GetSerialNumber());
    
    // init the interfaces
    if ( Usb3003A->Init(CODEC_AMBEPLUS) && Usb3003B->Init(CODEC_AMBE2PLUS) )
    {
        CVocodecChannel *Channel;
        // create all channels
        {
            // ch1
            Channel = new CVocodecChannel(Usb3003A, 0, Usb3003A, 1, CODECGAIN_AMBEPLUS);
            m_Channels.push_back(Channel);
            Usb3003A->AddChannel(Channel);
            // ch2
            Channel = new CVocodecChannel(Usb3003A, 1, Usb3003A, 0, CODECGAIN_AMBE2PLUS);
            m_Channels.push_back(Channel);
            Usb3003A->AddChannel(Channel);
            // ch3
            Channel = new CVocodecChannel(Usb3003B, 0, Usb3003B, 1, CODECGAIN_AMBEPLUS);
            m_Channels.push_back(Channel);
            Usb3003B->AddChannel(Channel);
            // ch4
            Channel = new CVocodecChannel(Usb3003B, 1, Usb3003B, 0, CODECGAIN_AMBE2PLUS);
            m_Channels.push_back(Channel);
            Usb3003B->AddChannel(Channel);
            // ch5
            Channel = new CVocodecChannel(Usb3003A, 2, Usb3003B, 2, CODECGAIN_AMBEPLUS);
            m_Channels.push_back(Channel);
            Usb3003A->AddChannel(Channel);
            Usb3003B->AddChannel(Channel);
            // ch6
            Channel = new CVocodecChannel(Usb3003B, 2, Usb3003A, 2, CODECGAIN_AMBE2PLUS);
            m_Channels.push_back(Channel);
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

int CVocodecs::InitUsb3000Pair(const CFtdiDeviceDescr &descr1, const CFtdiDeviceDescr &descr2)
{
    int nStreams = 0;
    
    // create the interfaces for the two 3000 chips
    CUsb3000Interface *Usb3000A = new CUsb3000Interface(descr1.GetVid(), descr1.GetPid(), "USB-3000", descr1.GetSerialNumber());
    CUsb3000Interface *Usb3000B = new CUsb3000Interface(descr2.GetVid(), descr2.GetPid(), "USB-3000", descr2.GetSerialNumber());
    
    // init the interfaces
    if ( Usb3000A->Init(CODEC_AMBEPLUS) && Usb3000B->Init(CODEC_AMBE2PLUS) )
    {
        CVocodecChannel *Channel;
        // create all channels
        {
            // ch1
            Channel = new CVocodecChannel(Usb3000A, 0, Usb3000B, 0, CODECGAIN_AMBEPLUS);
            m_Channels.push_back(Channel);
            Usb3000A->AddChannel(Channel);
            Usb3000B->AddChannel(Channel);
            // ch2
            Channel = new CVocodecChannel(Usb3000B, 0, Usb3000A, 0, CODECGAIN_AMBE2PLUS);
            m_Channels.push_back(Channel);
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
// manage channels

CVocodecChannel *CVocodecs::OpenChannel(uint8 uiCodecIn, uint8 uiCodecOut)
{
    CVocodecChannel *Channel = NULL;
    bool done = false;
    
    // loop on all interface until suitable & available channel found
    m_MutexChannels.lock();
    for ( int i = 0; (i < m_Channels.size()) && !done; i++ )
    {
        if ( !m_Channels[i]->IsOpen() &&
             (m_Channels[i]->GetCodecIn() == uiCodecIn) &&
             (m_Channels[i]->GetCodecOut() == uiCodecOut) )
        {
            if ( m_Channels[i]->Open() )
            {
                Channel = m_Channels[i];
                done = true;
            }
        }
    }
    m_MutexChannels.unlock();
    
    // done
    return Channel;
}

void CVocodecs::CloseChannel(CVocodecChannel *Channel)
{
    Channel->Close();
}
