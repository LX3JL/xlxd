//
//  cagc.cpp
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
// Geoffrey Merck F4FXL / KC3FRA AGC code borrowed from Liquid DSP
// Only took the parts we need qnd recoeded it to be close the XLX coding style
// https://github.com/jgaeddert/liquid-dsp/blob/master/src/agc/src/agc.c

#include <math.h>
#include "cagc.h"
#include "main.h"

////////////////////////////////////////////////////////////////////////////////////////
// constructor

CAGC::CAGC(float initialLeveldB)
{
    // set internal gain appropriately
    m_Gain = pow(10.0f, initialLeveldB/20.0f);
    //+- 10dB Margin, TODO Move margin to constant
    m_GainMax = pow(10.0f, (initialLeveldB + AGC_CLAMPING)/20.0f);
    m_GainMin = pow(10.0f, (initialLeveldB - AGC_CLAMPING)/20.0f);
                        
    m_EnergyPrime = 1.0f;

    // We do not target full scale to avoid stauration
    m_targetEnergy = 32767.0f * pow(10.0f, (initialLeveldB - 25.0)/20.0f);//25 dB below saturation as stated in docs
                                                                        //we also substract our target gain

    //this is the time constant of our AGC...
    m_Bandwidth = 1e-2f;//TODO : Move to parameter ?    
    m_Alpha = m_Bandwidth;  
}

////////////////////////////////////////////////////////////////////////////////////////
// get

float CAGC::GetGain()
{
    return 20.0f*log10(m_Gain);
}

////////////////////////////////////////////////////////////////////////////////////////
// process

inline void CAGC::ProcessSampleBlock(uint8* voice, int length) 
{
    for(int i = 0; i < length; i += 2)
    {
        float input = (float)(short)MAKEWORD(voice[i+1], voice[i]);
        //apply AGC
        // apply gain to input sample
        float output = input * m_Gain;

        // compute output signal energy, scaled to 0 to 1
        float instantEnergy = abs(output) / m_targetEnergy;

        // smooth energy estimate using single-pole low-pass filter
        m_EnergyPrime = (1.0f - m_Alpha) * m_EnergyPrime + m_Alpha * instantEnergy;

        // update gain according to output energy
        if (m_EnergyPrime > 1e-6f)
            m_Gain *= exp( -0.5f * m_Alpha * log(m_EnergyPrime) ); 

        // clamp gain
        if (m_Gain > m_GainMax)
            m_Gain = m_GainMax;
        else if(m_Gain < m_GainMin)
            m_Gain = m_GainMin;

        //write processed sample back
        voice[i] = HIBYTE((short)output);
        voice[i+1] = LOBYTE((short)output);
    }
}
