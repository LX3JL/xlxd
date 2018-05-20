//
//  cusb3000baointerface.h
//  ambed
//
//  Created by Jean-Luc Deltombe (LX3JL) on 21/08/2017.
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

#ifndef cusb3000baointerface_h
#define cusb3000baointerface_h


#include "ftd2xx.h"
#include "cbuffer.h"
#include "cusb3000interface.h"

////////////////////////////////////////////////////////////////////////////////////////
// class

class CUsb3000BaoInterface : public CUsb3000Interface
{
public:
    // constructors
    CUsb3000BaoInterface(uint32, uint32, const char *, const char *);
    
    // destructor
    virtual ~CUsb3000BaoInterface() {}
    
protected:
    // low level
    bool OpenDevice(void);
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cusb3000baointerface_h */
