//
//  ctimepoint.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 05/11/2015.
//  Copyright © 2015 Jean-Luc Deltombe (LX3JL). All rights reserved.
//
// ----------------------------------------------------------------------------
//    This file is part of xlxd.
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

#include "main.h"
#include "ctimepoint.h"

////////////////////////////////////////////////////////////////////////////////////////
// constructor

CTimePoint::CTimePoint()
{
    m_TimePoint = std::chrono::steady_clock::now();
}

////////////////////////////////////////////////////////////////////////////////////////
// operation

double CTimePoint::DurationSinceNow(void) const
{
    std::chrono::steady_clock::time_point Now = std::chrono::steady_clock::now();
    std::chrono::steady_clock::duration time_span = (Now - m_TimePoint);
    return double(time_span.count()) * std::chrono::steady_clock::period::num / std::chrono::steady_clock::period::den;
}

////////////////////////////////////////////////////////////////////////////////////////
// task

void CTimePoint::TaskSleepFor(uint ms)
{
    std::chrono::milliseconds timespan(ms);
    std::this_thread::sleep_for(timespan);
}
