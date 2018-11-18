//
//  cftdidevicedescr.h
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

#ifndef cftdidevicedescr_h
#define cftdidevicedescr_h

#include <string.h>
#include "ftd2xx.h"
#include "cvocodecchannel.h"

////////////////////////////////////////////////////////////////////////////////////////
// define

#define FTDI_MAX_STRINGLENGTH   256



////////////////////////////////////////////////////////////////////////////////////////
// class

class CUsb3000Interface;
class CUsb3003Interface;

class CFtdiDeviceDescr
{
public:
    // constructors
    CFtdiDeviceDescr(void);
    CFtdiDeviceDescr(uint32, uint32, const char *, const char *);
    CFtdiDeviceDescr(const CFtdiDeviceDescr &);
    
    // destructor
    virtual ~CFtdiDeviceDescr() {}
 
    // interface factory
    static int CreateInterface(CFtdiDeviceDescr *, std::vector<CVocodecChannel *>*);
    static int CreateInterfacePair(CFtdiDeviceDescr *, CFtdiDeviceDescr *, std::vector<CVocodecChannel *>*);

    // get
    bool IsUsed(void) const                         { return m_bUsed; }
    int  GetNbChannels(void) const;
    uint32 GetVid(void) const                       { return m_uiVid; }
    uint32 GetPid(void) const                       { return m_uiPid; }
    const char *GetDescription(void) const          { return m_szDescription; }
    const char *GetSerialNumber(void) const         { return m_szSerial; }
    const char * GetChannelDescription(int) const;
    const char * GetChannelSerialNumber(int) const;

    
    // set
    void SetUsed(bool used)                 { m_bUsed = used; }

protected:
    // factory helper
    static int  CreateUsb3012(CFtdiDeviceDescr *, std::vector<CVocodecChannel *>*);
    static int  CreateUsb3006(CFtdiDeviceDescr *, std::vector<CVocodecChannel *>*);
    static int  CreateUsb3003(CFtdiDeviceDescr *, std::vector<CVocodecChannel *>*);
    static int  CreateBaoFarm(CFtdiDeviceDescr *, std::vector<CVocodecChannel *>*);
    static int  CreatePair(CUsb3003Interface *, CUsb3003Interface *, std::vector<CVocodecChannel *>*);
    static int  CreatePair(CUsb3003Interface *, CUsb3000Interface *, std::vector<CVocodecChannel *>*);
    static int  CreatePair(CUsb3000Interface *, CUsb3000Interface *, std::vector<CVocodecChannel *>*);
    static CUsb3003Interface *InstantiateUsb3003(CFtdiDeviceDescr *);
    static CUsb3000Interface *InstantiateUsb3000(CFtdiDeviceDescr *);
  
protected:
    // status
    bool    m_bUsed;
    
    // data
    uint32  m_uiVid;
    uint32  m_uiPid;
    char    m_szDescription[FTDI_MAX_STRINGLENGTH];
    char    m_szSerial[FTDI_MAX_STRINGLENGTH];
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cftdidevicedescr_h */
