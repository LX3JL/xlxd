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

#include <vector>
#include "csampleblockprocessor.h"

class CSignalProcessor
{
public:
    //Constructor
    CSignalProcessor(float gaindB);

    //Destructor
    ~CSignalProcessor();

    //Processing
    void Process(uint8* voice, int length);

private:
    std::vector<CSampleBlockProcessor *> m_sampleProcessors;
};

#endif /* csignalprocessor_h */