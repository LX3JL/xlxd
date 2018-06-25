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
    // first handle all even number of channels devices
    std::vector<CVocodecChannel *>  Multi3003DevicesChs;
    for ( int i = 0; i < m_FtdiDeviceDescrs.size(); i++ )
    {
        CFtdiDeviceDescr *descr = m_FtdiDeviceDescrs[i];
        if ( !descr->IsUsed() && IsEven(descr->GetNbChannels()) )
        {
            // create the object
            iNbCh += CFtdiDeviceDescr::CreateInterface(descr, &Multi3003DevicesChs);
            // and flag as used
            descr->SetUsed(true);
        }
    }
    // next handle all single channel devices.
    // they must be handeled in pair, or in pair with another
    // even number of channels device.
    std::vector<CVocodecChannel *>  PairsOf3000DevicesChs;
    for ( int i = 0; i < m_FtdiDeviceDescrs.size(); i++ )
    {
        CFtdiDeviceDescr *descr1 = m_FtdiDeviceDescrs[i];
        CFtdiDeviceDescr *descr2 = NULL;
        if ( !descr1->IsUsed() && (descr1->GetNbChannels() == 1) )
        {
            // any other single channel device to pair with ?
            bool found = false;
            int j = i+1;
            while ( !found && (j < m_FtdiDeviceDescrs.size()) )
            {
                descr2 = m_FtdiDeviceDescrs[j];
                found = (!descr2->IsUsed() && (descr2->GetNbChannels() == 1));
            }
            // found one ?
            if ( found )
            {
                // yes, create and pairboth interfaces
                iNbCh += CFtdiDeviceDescr::CreateInterfacePair(descr1, descr2, &PairsOf3000DevicesChs);
                // and flag as used
                descr1->SetUsed(true);
                descr2->SetUsed(true);
            }
        }
    }
    // now we should have only remaining the 3 channels device(s)
    // and possibly an unique single channel device
    std::vector<CVocodecChannel *>  Single3003DeviceChannels;
    for ( int i = 0; i < m_FtdiDeviceDescrs.size(); i++ )
    {
        CFtdiDeviceDescr *descr1 = m_FtdiDeviceDescrs[i];
        CFtdiDeviceDescr *descr2 = NULL;
        if ( !descr1->IsUsed() && (descr1->GetNbChannels() == 3) )
        {
            // any other odd channel device to pair with ?
            // any other single channel device to pair with ?
            bool found = false;
            int j = i+1;
            while ( !found && (j < m_FtdiDeviceDescrs.size()) )
            {
                descr2 = m_FtdiDeviceDescrs[j];
                found = (!descr2->IsUsed() && IsOdd(descr2->GetNbChannels()));
            }
            // found one ?
            if ( found )
            {
                // yes, create and pairboth interfaces
                iNbCh += CFtdiDeviceDescr::CreateInterfacePair(descr1, descr2, &Multi3003DevicesChs);
                // and flag as used
                descr1->SetUsed(true);
                descr2->SetUsed(true);
            }
            else
            {
                // no, just create a standalone 3003 interface
                iNbCh += CFtdiDeviceDescr::CreateInterface(descr1, &Single3003DeviceChannels);
                // and flag as used
                descr1->SetUsed(true);
            }
        }
    }
    
    // now agregate channels by order of priority
    // for proper load sharing
    // pairs of 300 devices first
    {
        for ( int i = 0;  i < PairsOf3000DevicesChs.size(); i++ )
        {
            m_Channels.push_back(PairsOf3000DevicesChs.at(i));
        }
        PairsOf3000DevicesChs.clear();
    }
    // next the left-over single 3003 device
    {
        for ( int i = 0;  i < Single3003DeviceChannels.size(); i++ )
        {
            m_Channels.push_back(Single3003DeviceChannels.at(i));
        }
        Single3003DeviceChannels.clear();
    }
    // and finally interlace multi-3003 and pairs of 3003 devices which always
    // results to 6 channels per pair of 3003
    {
        int n = (int)Multi3003DevicesChs.size() / 6;
        for ( int i = 0; i < 6; i++ )
        {
            for ( int j = 0; j < n; j++ )
            {
                m_Channels.push_back(Multi3003DevicesChs.at((j*6) + i));
            }
        }
        Multi3003DevicesChs.clear();
    }
    
    
    // done
    if ( ok )
    {
        std::cout << "Codec interfaces initialized successfully : " << iNbCh << " channels available" << std::endl;
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
