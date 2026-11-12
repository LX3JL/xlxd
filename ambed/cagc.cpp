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
// AGC with hang, replaces the sample by sample loop by Geoffrey Merck
// F4FXL / KC3FRA that was inspired by Liquid DSP
//
// Works like the AGC on an HF receiver: fast down, hold through gaps, then up.
// The further a station is below the target the faster the gain rises, easing
// off as it gets close.
// It follows the peaks of the speech over tens of ms, so the gain moves between
// syllables and the shape of the speech and pitch is left alone for the
// vocoders. Silence doesn't count, so pauses leave the gain where it is.

#include <math.h>
#include "cagc.h"
#include "main.h"

// Speech peak envelope, falling share per 20 ms frame: about 50 ms
#define AGC_PEAK_DECAY          0.33f
// Voice frames at the start of a stream before the gain may rise
#define AGC_START_HANG          5
// Recent peak fall per 20 ms frame: 12 dB/s
#define AGC_RECENT_DECAY        0.973f
// Peaks within this of the target still hold the gain, in dB, so ordinary
// softer words don't lift it
#define AGC_HANG_MARGIN         1.5f
// A station this far below the target doesn't wait for the hang, in dB
#define AGC_NO_HANG_ERROR       6.0f
// Limiter recovery: 6 dB in 100 ms at 8 kHz
#define AGC_LIMITER_RECOVERY    1.000867f

////////////////////////////////////////////////////////////////////////////////////////
// constructor

CAGC::CAGC(float targetdB, float clampingdB, float attackMs, float hangMs,
           float recovery, float recoveryMin, float recoveryMax, float gatedB, float limitdB)
{
    m_Enabled = (clampingdB > 0.0f);
    m_TargetdB = targetdB;
    m_ClampingdB = clampingdB;
    m_AttackCoef = 1.0f - exp(-20.0f / attackMs);
    m_HangFrames = (int)(hangMs / 20.0f);
    m_Recovery = recovery * 0.02f;
    m_RecoveryMin = recoveryMin * 0.02f;
    m_RecoveryMax = recoveryMax * 0.02f;
    m_GateLevel = 32768.0f * pow(10.0f, gatedB/20.0f);
    m_LimitLevel = 32768.0f * pow(10.0f, limitdB/20.0f);

    m_SpeechPeak = 0.0f;
    m_RecentPeak = 0.0f;
    m_bVoice = false;
    m_HangLeft = AGC_START_HANG;

    m_GaindB = 0.0f;
    m_Gain = 1.0f;
    m_Limiter = 1.0f;
    m_LimitedSamples = 0;
}

////////////////////////////////////////////////////////////////////////////////////////
// get

float CAGC::GetGain()
{
    return m_GaindB;
}

float CAGC::GetSpeechPeak()
{
    return (m_SpeechPeak > 0.0f) ? 20.0f * log10(m_SpeechPeak / 32768.0f) : -96.0f;
}

////////////////////////////////////////////////////////////////////////////////////////
// process

void CAGC::ProcessSampleBlock(uint8* voice, int length)
{
    int samples = length / 2;
    if(samples == 0)
    {
        return;
    }

    float sum = 0.0f;
    for(int i = 0; i < length; i += 2)
    {
        sum += fabs((float)(short)MAKEWORD(voice[i+1], voice[i]));
    }
    float level = sum / samples;

    // follow the speech peaks, measured even with the gain off for tuning
    m_bVoice = (level >= m_GateLevel);
    if(m_bVoice)
    {
        m_SpeechPeak = MAX(level, m_SpeechPeak + (level - m_SpeechPeak) * AGC_PEAK_DECAY);
        m_RecentPeak = MAX(m_SpeechPeak, m_RecentPeak * AGC_RECENT_DECAY);
    }

    if(!m_Enabled)
    {
        return;
    }

    // only voice moves the gain
    if(m_bVoice)
    {
        float wanted = m_TargetdB - 20.0f * log10(m_RecentPeak / 32768.0f);
        wanted = MAX(-m_ClampingdB, MIN(m_ClampingdB, wanted));
        float error = wanted - m_GaindB;

        if(error < 0.0f)
        {
            m_GaindB += error * m_AttackCoef;
            m_HangLeft = MAX(m_HangLeft, m_HangFrames);
        }
        else if(error < AGC_HANG_MARGIN)
        {
            m_HangLeft = m_HangFrames;
        }
        else if(m_HangLeft > 0 && error < AGC_NO_HANG_ERROR)
        {
            m_HangLeft--;
        }
        else
        {
            // quick when far off, easing in as it gets close
            float step = MAX(m_RecoveryMin, MIN(m_RecoveryMax, error * m_Recovery));
            m_GaindB = MIN(wanted, m_GaindB + step);
        }
    }

    // ramp across the frame so a gain change can't click
    float gain = m_Gain;
    float newGain = pow(10.0f, m_GaindB/20.0f);
    float ramp = (newGain - gain) / samples;

    for(int i = 0; i < length; i += 2)
    {
        gain += ramp;
        float output = (float)(short)MAKEWORD(voice[i+1], voice[i]) * gain * m_Limiter;

        // the limiter pulls a peak down at once and recovers slowly
        if(fabs(output) > m_LimitLevel)
        {
            float reduction = m_LimitLevel / fabs(output);
            m_Limiter *= reduction;
            output *= reduction;
            m_LimitedSamples++;
        }
        m_Limiter = MIN(1.0f, m_Limiter * AGC_LIMITER_RECOVERY);

        //write processed sample back, clamp to short range
        if (output > 32767.0f) output = 32767.0f;
        else if (output < -32768.0f) output = -32768.0f;
        voice[i] = HIBYTE((short)output);
        voice[i+1] = LOBYTE((short)output);
    }
    m_Gain = newGain;
}
