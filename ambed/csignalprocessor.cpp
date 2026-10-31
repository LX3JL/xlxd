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

#include <cmath>
#include <iomanip>
#include <sstream>
#include "main.h"
#include "csignalprocessor.h"
#include "cagc.h"
#include "cfixedgain.h"
#include "cfirfilter.h"

// within this of the clamp, the AGC is at its limit
#define AGC_LIMIT_MARGIN    0.05f

static float ToDbfs(double level)
{
    return (level > 0.0) ? (float)(20.0 * log10(level / 32768.0)) : -96.0f;
}

// a gain in the report, as DMR_TO_PCM -1.50
static std::string GainLabel(const std::string& name, float gain)
{
    std::ostringstream label;
    label << name << " " << std::showpos << std::fixed << std::setprecision(2) << gain;
    return label.str();
}

////////////////////////////////////////////////////////////////////////////////////////
// level meter

CLevelMeter::CLevelMeter()
{
    m_Sum = 0.0;
    m_Samples = 0;
    m_Clipped = 0;
    m_Peak = 0;
}

void CLevelMeter::Add(const uint8* voice, int length)
{
    for(int i = 0; i < length; i += 2)
    {
        int sample = abs((int)(short)MAKEWORD(voice[i+1], voice[i]));
        m_Sum += sample;
        m_Samples++;
        m_Peak = MAX(m_Peak, sample);
        if(sample >= 32767)
        {
            m_Clipped++;
        }
    }
}

void CLevelMeter::Report(const std::string& label) const
{
    if(m_Samples == 0)
    {
        return;
    }
    std::cout << "Tuning:   " << std::left << std::setw(22) << label << std::right
              << "level " << std::setw(6) << std::setprecision(1) << ToDbfs(m_Sum / m_Samples) << " dBFS"
              << "  peak " << std::setw(6) << ToDbfs(m_Peak) << " dBFS"
              << "  clipped " << std::setprecision(2) << (100.0 * m_Clipped / m_Samples) << "%" << std::endl;
}

////////////////////////////////////////////////////////////////////////////////////////
// constructor

CSignalProcessor::CSignalProcessor(uint8 uiCodecIn)
{
    m_uiCodecIn = uiCodecIn;
    m_Tuning = g_Config.GetTuning();

    m_pDecodeGain = new CFixedGain(m_Tuning.fToPcm[uiCodecIn]);
    m_pFilter = m_Tuning.bUseFilter ? new CFIRFilter(FILTER_TAPS, FILTER_TAPS_LENGTH) : NULL;
    // The encode gains come after the AGC, so leave room for the highest one
    float limit = AGC_CEILING;
    for(int i = 0; i < NB_OF_CODECS; i++)
    {
        limit = MIN(limit, AGC_CEILING - m_Tuning.fFromPcm[i]);
    }
    m_pAgc = m_Tuning.bUseAgc ? new CAGC(m_Tuning.fAgcTarget, m_Tuning.fAgcClamping, m_Tuning.fAgcAttack, m_Tuning.fAgcHang,
                                         m_Tuning.fAgcRecovery, m_Tuning.fAgcRecoveryMin, m_Tuning.fAgcRecoveryMax,
                                         m_Tuning.fAgcGate, limit) : NULL;
    for(int i = 0; i < NB_OF_CODECS; i++)
    {
        m_pEncodeGain[i] = new CFixedGain(m_Tuning.fFromPcm[i]);
    }

    m_bVoice = false;
    m_Frames = 0;
    m_VoiceFrames = 0;
    m_AgcSum = 0.0;
    m_AgcMin = 0.0f;
    m_AgcMax = 0.0f;
    m_AgcAtMax = 0;
    m_AgcAtMin = 0;
    m_SpeechPeakSum = 0.0;
    m_SpeechPeakFrames = 0;
}

////////////////////////////////////////////////////////////////////////////////////////
// destructor

CSignalProcessor::~CSignalProcessor()
{
    delete m_pDecodeGain;
    delete m_pFilter;
    delete m_pAgc;
    for(int i = 0; i < NB_OF_CODECS; i++)
    {
        delete m_pEncodeGain[i];
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// processing

void CSignalProcessor::Process(uint8* voice, int length)
{
    // Only voice counts towards the report, pauses would pull the levels down
    double sum = 0.0;
    for(int i = 0; i < length; i += 2)
    {
        sum += abs((int)(short)MAKEWORD(voice[i+1], voice[i]));
    }
    m_bVoice = (length > 0) && (ToDbfs(sum / (length / 2)) >= TUNING_SILENCE_LEVEL);

    m_Frames++;
    if(m_bVoice)
    {
        m_VoiceFrames++;
        m_Decoded.Add(voice, length);
    }

    m_pDecodeGain->ProcessSampleBlock(voice, length);
    if(m_pFilter != NULL)
    {
        m_pFilter->ProcessSampleBlock(voice, length);
    }

    if(m_bVoice)
    {
        m_ToAgc.Add(voice, length);
    }

    if(m_pAgc != NULL)
    {
        m_pAgc->ProcessSampleBlock(voice, length);

        if(m_bVoice)
        {
            float gain = m_pAgc->GetGain();
            m_AgcMin = (m_VoiceFrames == 1) ? gain : MIN(m_AgcMin, gain);
            m_AgcMax = (m_VoiceFrames == 1) ? gain : MAX(m_AgcMax, gain);
            m_AgcSum += gain;
            if(m_pAgc->IsVoice())
            {
                m_SpeechPeakSum += m_pAgc->GetSpeechPeak();
                m_SpeechPeakFrames++;
            }
            if(m_Tuning.fAgcClamping > 0.0f && gain >= m_Tuning.fAgcClamping - AGC_LIMIT_MARGIN)
            {
                m_AgcAtMax++;
            }
            else if(m_Tuning.fAgcClamping > 0.0f && gain <= -m_Tuning.fAgcClamping + AGC_LIMIT_MARGIN)
            {
                m_AgcAtMin++;
            }
        }
    }

    if(m_bVoice)
    {
        m_FromAgc.Add(voice, length);
    }
}

void CSignalProcessor::ProcessOutput(uint8 uiCodecOut, uint8* voice, int length)
{
    m_pEncodeGain[uiCodecOut]->ProcessSampleBlock(voice, length);
    if(m_bVoice)
    {
        m_Output[uiCodecOut].Add(voice, length);
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// tuning report

void CSignalProcessor::Report(uint16 uiStreamId, const std::string& callsign) const
{
    if(!m_Tuning.bTuningLog || m_VoiceFrames == 0)
    {
        return;
    }

    std::ios::fmtflags flags = std::cout.flags();
    std::streamsize precision = std::cout.precision();
    std::cout << std::fixed;

    std::string mode = CConfig::GetModeName(m_uiCodecIn);
    std::cout << "Tuning: stream " << uiStreamId << " " << callsign << " " << mode << " input, "
              << m_VoiceFrames << " voice frames of " << m_Frames << std::endl;

    m_Decoded.Report("decoded");
    m_ToAgc.Report(GainLabel(mode + "_TO_PCM", m_Tuning.fToPcm[m_uiCodecIn]));

    if(m_pAgc != NULL)
    {
        std::cout << "Tuning:   " << std::left << std::setw(22) << "AGC" << std::right << std::setprecision(1)
                  << "target " << m_Tuning.fAgcTarget << " dBFS  speech peaks "
                  << ((m_SpeechPeakFrames > 0) ? (float)(m_SpeechPeakSum / m_SpeechPeakFrames) : -96.0f) << " dBFS  gain average " << std::showpos << (m_AgcSum / m_VoiceFrames)
                  << " dB, " << m_AgcMin << " to " << m_AgcMax << " dB" << std::noshowpos
                  << "  at limit " << (100.0 * m_AgcAtMax / m_VoiceFrames) << "% high "
                  << (100.0 * m_AgcAtMin / m_VoiceFrames) << "% low"
                  << "  limiter " << std::setprecision(2) << (100.0 * m_pAgc->GetLimitedSamples() / (m_Frames * 160)) << "%" << std::endl;
        m_FromAgc.Report("after AGC");
    }

    for(int i = 0; i < NB_OF_CODECS; i++)
    {
        m_Output[i].Report(GainLabel(std::string("PCM_TO_") + CConfig::GetModeName((uint8)i), m_Tuning.fFromPcm[i]));
    }

    std::cout.flags(flags);
    std::cout.precision(precision);
}
