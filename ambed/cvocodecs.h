//
//  cvocodecs.h
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

#ifndef cvocodecs_h
#define cvocodecs_h

#include "cftdidevicedescr.h"
#include "cvocodecinterface.h"
#include "cvocodecchannel.h"

////////////////////////////////////////////////////////////////////////////////////////
// class

class CVocodecs
{
public:
    // constructors
    CVocodecs();
    
    // destructor
    virtual ~CVocodecs();
    
    // initialization
    bool Init(void);
    
    // manage interfaces
    int  GetNbInterfaces(void) const               { return (int)m_Interfaces.size(); }
    CVocodecInterface *GetInterface(int);
    
    // manage channels
    CVocodecChannel *OpenChannel(uint8, uint8);
    void AddChannel(CVocodecChannel *ch)            { m_Channels.push_back(ch); }
    void CloseChannel(CVocodecChannel *);

protected:
    // initialisation helpers
    bool DiscoverFtdiDevices(void);
    
    // helpers
    bool IsEven(int i) const        { return ((i % 2) == 0); }
    bool IsOdd(int i) const         { return !IsEven(i); }
    
protected:
    // array of interfaces
    std::mutex                          m_MutexInterfaces;
    std::vector<CVocodecInterface *>    m_Interfaces;
    
    // array of channels
    std::mutex                          m_MutexChannels;
    std::vector<CVocodecChannel *>      m_Channels;
    
    // array of FTDI desciptors
    std::vector<CFtdiDeviceDescr *>     m_FtdiDeviceDescrs;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cvocodecs_h */
