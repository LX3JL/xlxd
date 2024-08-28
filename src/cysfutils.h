//
//  cysfutils.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 14/04/2019.
//  Copyright © 2019 Jean-Luc Deltombe (LX3JL). All rights reserved.
//  Copyright (C) 2016,2017 by Jonathan Naylor G4KLX
//  Copyright (C) 2018 by Andy Uribe CA6JAU
//  Copyright (C) 2018 by Manuel Sanchez EA7EE
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

#ifndef cysfutils_h
#define cysfutils_h

////////////////////////////////////////////////////////////////////////////////////////
// class

class CYsfUtils
{
public:
    // constructor
    CYsfUtils() {};
    
    // destructor
    virtual ~CYsfUtils() {};
    
    // code / decode
    static void DecodeVD2Vchs(uint8 *, uint8 **);
    static void DecodeVD2Vch(uint8 *, uint8 *);
    static void EncodeVD2Vch(uint8 *, uint8 *);
    
protected:
    // data
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cysfutils_h */
