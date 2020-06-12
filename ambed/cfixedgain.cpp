//
//  cfixedgain.cpp
//  ambed
//
//  Created by Jean-Luc Deltombe (LX3JL) on 28/04/2017.
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
// Geoffrey Merck F4FXL / KC3FRA AGC

#include "cfixedgain.h"
#include <math.h>

////////////////////////////////////////////////////////////////////////////////////////
// constructor

CFixedGain::CFixedGain(float gaindB)
{
    m_gaindB = gaindB;
    m_gainLinear = pow(10.0f, m_gaindB/20.0f);
}

////////////////////////////////////////////////////////////////////////////////////////
// processing
 
inline void CFixedGain::ProcessSampleBlock(uint8* voice, int length)
{
    for(int i = 0; i < length; i += 2)
    {
        float input = (float)(short)MAKEWORD(voice[i+1], voice[i]);
        //apply gain
        float output = input * m_gainLinear;

        //write processed sample back
        voice[i] = HIBYTE((short)output);
        voice[i+1] = LOBYTE((short)output);
    }
}