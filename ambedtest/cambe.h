//
//  cambe.h
//  ambedtest
//
//  Created by Jean-Luc Deltombe (LX3JL) on 16/05/2017.
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

#ifndef cambe_h
#define cambe_h


////////////////////////////////////////////////////////////////////////////////////////
// define

// frame sizes
#define AMBE_SIZE           9


////////////////////////////////////////////////////////////////////////////////////////
// class

class CAmbe
{
public:
    // constructor
    CAmbe();
    CAmbe(uint8 *);
    
    // destructor
    virtual ~CAmbe() {}
    
    // get
    const uint8 *GetData(void) const        { return m_uiAmbe; }
    
protected:
    // data
    uint8   m_uiAmbe[AMBE_SIZE];
};


////////////////////////////////////////////////////////////////////////////////////////
#endif /* cambe_h */
