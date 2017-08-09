//
//  ctimepoint.h
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

#ifndef ctimepoint_h
#define ctimepoint_h


////////////////////////////////////////////////////////////////////////////////////////
// class

class CTimePoint : public std::chrono::steady_clock::time_point
{
public:
    // constructor
    CTimePoint();
    
    // destructor
    virtual ~CTimePoint() {}
    
    // operation
    void   Now(void)                        { m_TimePoint = std::chrono::steady_clock::now(); }
    double DurationSinceNow(void) const;
    
    // task
    static void TaskSleepFor(uint);
    
protected:
    // data
    std::chrono::steady_clock::time_point m_TimePoint;
};


////////////////////////////////////////////////////////////////////////////////////////
#endif /* ctimepoint_h */
