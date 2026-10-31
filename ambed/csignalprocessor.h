//
//  csignalprocessor.h
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

#ifndef csignalprocessor_h
#define csignalprocessor_h

#include <string>
#include "cconfig.h"
#include "csampleblockprocessor.h"

class CAGC;
class CFixedGain;
class CFIRFilter;

// voice level at one point of the chain, for the tuning report
class CLevelMeter
{
public:
    CLevelMeter();

    void Add(const uint8* voice, int length);
    void Report(const std::string& label) const;

private:
    double  m_Sum;          // sum of absolute sample values
    long    m_Samples;
    long    m_Clipped;
    int     m_Peak;
};

class CSignalProcessor
{
public:
    //Constructor, takes the input codec and the current ambed.ini values
    CSignalProcessor(uint8 uiCodecIn);

    //Destructor
    ~CSignalProcessor();

    //Processing
    void Process(uint8* voice, int length);                     //decode gain, filter, AGC
    void ProcessOutput(uint8 uiCodecOut, uint8* voice, int length);   //encode gain

    //Tuning report, logged when the stream closes
    void Report(uint16 uiStreamId, const std::string& callsign) const;

private:
    uint8       m_uiCodecIn;
    CTuning     m_Tuning;
    CFixedGain  *m_pDecodeGain;
    CFIRFilter  *m_pFilter;
    CAGC        *m_pAgc;
    CFixedGain  *m_pEncodeGain[NB_OF_CODECS];

    // tuning report
    bool        m_bVoice;                       //is the current frame voice
    long        m_Frames;
    long        m_VoiceFrames;
    CLevelMeter m_Decoded;
    CLevelMeter m_ToAgc;
    CLevelMeter m_FromAgc;
    CLevelMeter m_Output[NB_OF_CODECS];
    double      m_AgcSum;
    float       m_AgcMin;
    float       m_AgcMax;
    long        m_AgcAtMax;
    long        m_AgcAtMin;
    double      m_SpeechPeakSum;
    long        m_SpeechPeakFrames;
};

#endif /* csignalprocessor_h */
