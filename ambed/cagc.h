//
//  cagc.h
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
// Geoffrey Merck F4FXL / KC3FRA AGC code largely inspired by Liquid DSP
// Only took the parts we need qnd recoeded it to be close the XLX coding style
// https://github.com/jgaeddert/liquid-dsp/blob/master/src/agc/src/agc.c 

#ifndef cagc_h
#define cagc_h

#include "csampleblockprocessor.h"

class CAGC : CSampleBlockProcessor
{
public:
    //Constructor
    CAGC(float initialLeveldB);

    //methods
    void ProcessSampleBlock(uint8* voice, int length) ;
    float GetGain();//gets current gain 
    
private:
    float m_Gain;            // current gain value
    float m_GainMax, m_GainMin;       //gain clamping
    float m_targetEnergy;        // scale value for target energy

    // gain control loop filter parameters
    float m_Bandwidth;    // bandwidth-time constant
    float m_Alpha;        // feed-back gain

    // signal level estimate
    float m_EnergyPrime;     // filtered output signal energy estimate
};

#endif /* cgc_h */