//
//  cfirfilter.cpp
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
// FIRFilter by Geoffrey Merck F4FXL / KC3FRA

#include "cfirfilter.h"
#include <string.h>

CFIRFilter::CFIRFilter(const float* taps, int tapsLength)
{
    m_taps = new float[tapsLength];
    m_buffer = new float[tapsLength];
    m_tapsLength = tapsLength;

    ::memcpy(m_taps, taps, tapsLength * sizeof(float));
    ::memset(m_buffer, 0, tapsLength * sizeof(float));
    m_currentBufferPosition = 0;
}

CFIRFilter::~CFIRFilter()
{
    delete[] m_taps;
    delete[] m_buffer;
}

inline void CFIRFilter::ProcessSampleBlock(uint8* voice, int length)
{
    for(int i = 0; i < length; i += 2)
    {
        float input = (float)(short)MAKEWORD(voice[i+1], voice[i]);
        float output = 0.0f;
        int iTaps = 0;

        // Buffer latest sample into delay line
        m_buffer[m_currentBufferPosition] = input;

        for(int i = m_currentBufferPosition; i >= 0; i--)
        {
            output += m_taps[iTaps++] * m_buffer[i];
        }

        for(int i = m_tapsLength - 1; i > m_currentBufferPosition; i--)
        {
            output += m_taps[iTaps++] * m_buffer[i];
        }
        
        m_currentBufferPosition = (m_currentBufferPosition + 1) % m_tapsLength;

        //write processed sample back
        voice[i] = HIBYTE((short)output);
        voice[i+1] = LOBYTE((short)output);
    }
}


