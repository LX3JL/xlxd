//
//  cfixedgain.h
//  ambed
//
//  Created by Jean-Luc Deltombe (LX3JL) on 26/04/2017.
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
// Geoffrey Merck F4FXL / KC3FRA

#ifndef cfixedgain_h
#define cfixedgain_h

#include "csampleblockprocessor.h"

class CFixedGain : CSampleBlockProcessor
{
public:
    //Constructor
    CFixedGain(float gaindB);

    //processing
    void ProcessSampleBlock(uint8* voice, int length);

private:
    float m_gaindB; //gain in dB
    float m_gainLinear; //linearized gain
};

#endif /* cfixedgain_h */