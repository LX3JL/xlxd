//
//  cfirfilter.h
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

#ifndef cfirfilter_h
#define cfirfilter_h

#include "csampleblockprocessor.h"

class CFIRFilter : CSampleBlockProcessor
{
public :
    //Constructor
    CFIRFilter(const float* taps, int tapsLength);

    // Destructor
    ~CFIRFilter();

    // Processing
    void ProcessSampleBlock(uint8* voice, int length);

private:
    float* m_taps;
    int m_tapsLength;
    float* m_buffer;
    int m_currentBufferPosition;
};

#endif //cfirfilter_h

