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
// AGC with hang, replaces the sample by sample loop by Geoffrey Merck
// F4FXL / KC3FRA that was inspired by Liquid DSP

#ifndef cagc_h
#define cagc_h

#include "csampleblockprocessor.h"

class CAGC : CSampleBlockProcessor
{
public:
    //Constructor, levels in dBFS and gains in dB, attack and hang in ms,
    //recovery in dB/s for each dB below the target, between recoveryMin and recoveryMax in dB/s
    CAGC(float targetdB, float clampingdB, float attackMs, float hangMs,
         float recovery, float recoveryMin, float recoveryMax, float gatedB, float limitdB);

    //methods
    void ProcessSampleBlock(uint8* voice, int length);
    float GetGain();//gets current gain in dB
    float GetSpeechPeak();//gets the speech peak level in, in dBFS
    bool IsVoice() { return m_bVoice; }//did the last frame pass the gate
    long GetLimitedSamples() { return m_LimitedSamples; }

private:
    bool  m_Enabled;            // a clamping of 0 measures but leaves the audio alone
    float m_TargetdB;           // speech peak level to aim for
    float m_ClampingdB;
    float m_AttackCoef;         // share of the way down per frame
    int   m_HangFrames;
    float m_Recovery;           // dB per frame, for each dB below the target
    float m_RecoveryMin;        // dB per frame
    float m_RecoveryMax;        // dB per frame
    float m_GateLevel;          // quieter frames aren't voice and leave the gain alone
    float m_LimitLevel;         // no sample goes above this

    float m_SpeechPeak;         // envelope of the frame levels
    float m_RecentPeak;         // the same, falling slowly so a word's tail isn't a quiet station
    bool  m_bVoice;
    int   m_HangLeft;           // frames until the gain may rise

    float m_GaindB;             // current gain
    float m_Gain;               // the same, linear
    float m_Limiter;            // peak limiter gain, 1.0 when idle
    long  m_LimitedSamples;
};

#endif /* cagc_h */
