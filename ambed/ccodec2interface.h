//
//  ccodec2interface.h
//  ambed
//
//  Created by Antony Chazapis (SV9OAN) on 26/12/2018.
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

#ifndef ccodec2interface_h
#define ccodec2interface_h

#include "cvocodecinterface.h"
#include <codec2/codec2.h>

////////////////////////////////////////////////////////////////////////////////////////
// class

class CCodec2Interface : public CVocodecInterface
{
public:
    // constructors
    CCodec2Interface();
    
    // destructor
    virtual ~CCodec2Interface();
    
    // initialization
    bool Init(void);

    // get
    const char *GetName(void) const     { return "Codec 2"; }
    
    // manage channels
    int   GetNbChannels(void) const     { return 2; }
    uint8 GetChannelCodec(int) const;

    // task
    void Task(void);

protected:
    // decoder helper
    void DecodeAmbePacket(CAmbePacket *, CVoicePacket *, uint8);
    
    // encoder helpers
    void EncodeVoicePacket(CVoicePacket *, CAmbePacket *, uint8);

    // data
    struct CODEC2 *codec2_3200_state;
    struct CODEC2 *codec2_2400_state;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* ccodec2interface_h */
