//
//  cusb3000interface.h
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

#ifndef cusb3000interface_h
#define cusb3000interface_h


#include "ftd2xx.h"
#include "cbuffer.h"
#include "cusb3xxxinterface.h"

////////////////////////////////////////////////////////////////////////////////////////
// define

#define USB3000_NB_CH       1

////////////////////////////////////////////////////////////////////////////////////////
// class

class CUsb3000Interface : public CUsb3xxxInterface
{
public:
    // constructors
    CUsb3000Interface(uint32, uint32, const char *, const char *);
    
    // destructor
    virtual ~CUsb3000Interface() {}
    
    // initialization
    bool Init(uint8);
    
    // manage channels
    int GetNbChannels(void) const       { return USB3000_NB_CH; }
    uint8 GetChannelCodec(int) const;
    
    // manage vocodec channels
    CVocodecChannel *GetChannelWithChannelIn(int);
    CVocodecChannel *GetChannelWithChannelOut(int);
    
protected:
    // decoder helper
    bool IsValidChannelPacket(const CBuffer &, int *, CAmbePacket *);
    bool IsValidSpeechPacket(const CBuffer &, int *, CVoicePacket *);
    
    // encoder helpers
    void EncodeChannelPacket(CBuffer *, int, CAmbePacket *);
    void EncodeSpeechPacket(CBuffer *, int, CVoicePacket *);
    
    // low level
    bool OpenDevice(void);
    bool ResetDevice(void);
    bool ConfigureDevice(void);
    
    // data
    uint8   m_uiChCodec;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cusb3000interface_h */
