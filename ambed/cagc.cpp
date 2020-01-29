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

#include "main.h"
#include <math.h>
#include "cagc.h"


////////////////////////////////////////////////////////////////////////////////////////
// constructor

CAGC::CAGC(float initialLeveldB)
{
    // set internal gain appropriately
    m_g = pow(10.0f, initialLeveldB/20.0f);

    // ensure resulting gain is not arbitrarily low
    if (m_g < 1e-16f)
        m_g = 1e-16f;

    m_scale = (float)0xFFFF;
    m_bandwidth = 1e-2f;                              //TODO : Move to parameter ?
    m_gMax = pow(10.0f, (initialLeveldB + 10.0f)/20.0f);//+- 10dB Margin, TODO Move margin to constant
    m_gMin = pow(10.0f, (initialLeveldB - 10.0f)/20.0f);
    m_alpha = m_bandwidth;
    m_y2_prime = 1.0f;
}

float CAGC::GetGain()
{
    return 20.0f*log10(m_g);
}

void CAGC::Apply(uint8 * voice, int size)
{
    for (int i = 0; i < size; i+=2)
    {
        //Get the sample
        float _x = (float)(short)MAKEWORD(voice[i+1], voice[i]);

        //This AGC tries to smooth energy so it does not exceed 1
        //Therefore divide by our max supposed value
        //Maybe we could also change y2 prime calculation below, but for now stick with this
        _x /= m_scale;

        //apply AGC
        // apply gain to input sample
        float _y = _x * m_g;

        // compute output signal energy
        float y2 = _y * _y;

        // smooth energy estimate using single-pole low-pass filter
        m_y2_prime = (1.0f - m_alpha) * m_y2_prime + m_alpha*y2;

        // update gain according to output energy
        if (m_y2_prime > 1e-6f)
            m_g *= exp( -0.5f * m_alpha * log(m_y2_prime) ); 

        // clamp gain
        if (m_g > m_gMax)
            m_g = m_gMax;
        else if(m_g < m_gMin)
            m_g = m_gMin;

        // apply output scale
        _y *= m_scale;

        //write processed sample back
        voice[i] = HIBYTE((short)_y);
        voice[i+1] = LOBYTE((short)_y);
    }    
}