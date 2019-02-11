//
//  cdvframepacket.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 01/11/2015.
//  Copyright © 2015 Jean-Luc Deltombe (LX3JL). All rights reserved.
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

#ifndef cdvframepacket_h
#define cdvframepacket_h

#include "cpacket.h"

////////////////////////////////////////////////////////////////////////////////////////
// defines

#define AMBE_SIZE       9
#define DVDATA_SIZE     3

#define AMBEPLUS_SIZE   9
#define DVSYNC_SIZE     7

// typedef & structures

struct __attribute__ ((__packed__))dstar_dvframe
{
    uint8	AMBE[AMBE_SIZE];
    uint8	DVDATA[DVDATA_SIZE];
};

////////////////////////////////////////////////////////////////////////////////////////
// class

class CDvFramePacket : public CPacket
{
friend class CCodecStream;
public:
    // constructor
    CDvFramePacket();
    CDvFramePacket(const struct dstar_dvframe *, uint16, uint8);
    CDvFramePacket(const uint8 *, const uint8 *, uint16, uint8, uint8);
    CDvFramePacket(uint16, uint8, const uint8 *, const uint8 *, const uint8 *, uint8, uint8, const uint8 *, const uint8 *);
    CDvFramePacket(const CDvFramePacket &);
    
    // destructor
    virtual ~CDvFramePacket() {};
    
    // virtual duplication
    CPacket *Duplicate(void) const;
    
    // identity
    bool IsDvFrame(void) const              { return true; }
    bool HaveTranscodableAmbe(void) const   { return true; }
    
    // get
    const uint8 *GetAmbe(uint8) const;
    const uint8 *GetAmbe(void) const        { return m_uiAmbe; }
    const uint8 *GetAmbePlus(void) const    { return m_uiAmbePlus; }
    const uint8 *GetCodec2(void) const      { return m_uiCodec2; }
    const uint8 *GetDvData(void) const      { return m_uiDvData; }
    const uint8 *GetDvSync(void) const      { return m_uiDvSync; }
    
    // set
    void SetDvData(uint8 *);
    void SetAmbe(uint8, uint8 *);
    void ClearAmbe(uint8);

    // operators
    bool operator ==(const CDvFramePacket &) const;

protected:
    // get
    uint8 *GetAmbeData(void)                { return m_uiAmbe; }
    uint8 *GetAmbePlusData(void)            { return m_uiAmbePlus; }
    uint8 *GetCodec2Data(void)              { return m_uiCodec2; }
    
protected:
    // data (dstar)
    uint8       m_uiAmbe[AMBE_SIZE];
    uint8       m_uiCodec2[AMBE_SIZE];
    uint8       m_uiDvData[DVDATA_SIZE];
    // data (dmr)
    uint8       m_uiAmbePlus[AMBEPLUS_SIZE];
    uint8       m_uiDvSync[DVSYNC_SIZE];
};


////////////////////////////////////////////////////////////////////////////////////////
#endif /* cdvframepacket_h */
