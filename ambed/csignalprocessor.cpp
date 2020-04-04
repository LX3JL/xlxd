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
// Geoffrey Merck F4FXL / KC3FRA AGC

#include "main.h"
#include "csignalprocessor.h"

#if USE_AGC == 1
#include "cagc.h"
#else
#include "cfixedgain.h"
#endif

#if USE_BANDPASSFILTER == 1
#include "cfirfilter.h"
#endif

////////////////////////////////////////////////////////////////////////////////////////
// constructor

CSignalProcessor::CSignalProcessor(float gaindB)
{
#if USE_BANDPASSFILTER
    m_sampleProcessors.push_back((CSampleBlockProcessor*)new CFIRFilter(FILTER_TAPS, FILTER_TAPS_LENGTH));
#endif
#if USE_AGC == 1
    m_sampleProcessors.push_back((CSampleBlockProcessor*)new CAGC(gaindB));
#else
    m_sampleProcessors.push_back((CSampleBlockProcessor*)new CFixedGain(gaindB));
#endif
}

////////////////////////////////////////////////////////////////////////////////////////
// destructor

CSignalProcessor::~CSignalProcessor()
{
    for(int i = 0; i < m_sampleProcessors.size(); i++)
    {
        delete m_sampleProcessors[i];
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// processing

void CSignalProcessor::Process(uint8* voice, int length)
{
    /*float sample;
    int j;*/
    auto processorsSize = m_sampleProcessors.size();

    for(int j = 0; j < processorsSize; j++)
    {
        m_sampleProcessors[j]->ProcessSampleBlock(voice, length);
    }

    /*for(int i = 0; i < length; i += 2)
    {
        //Get the sample
        sample = (float)(short)MAKEWORD(voice[i+1], voice[i]);

        for(j = 0; j < processorsSize; j++)
        {
            sample = m_sampleProcessors[j]->ProcessSample(sample);
        }

        //write processed sample back
        voice[i] = HIBYTE((short)sample);
        voice[i+1] = LOBYTE((short)sample);
    }*/
}