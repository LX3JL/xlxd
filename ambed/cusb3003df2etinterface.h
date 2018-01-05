//
//  cusb3003df2etinterface.h
//  ambed
//
//  Created by Jean-Luc Deltombe (LX3JL) and Florian Wolters (DF2ET) on 03/11/2017.
//  Copyright © 2017 Jean-Luc Deltombe (LX3JL) and Florian Wolters (DF2ET).
//  All rights reserved.
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

#ifndef cusb3003df2etinterface_h
#define cusb3003df2etinterface_h


#include "ftd2xx.h"
#include "cbuffer.h"
#include "cusb3003interface.h"

////////////////////////////////////////////////////////////////////////////////////////
// define


////////////////////////////////////////////////////////////////////////////////////////
// class

class CUsb3003DF2ETInterface : public CUsb3003Interface
{
public:
    // constructors
    CUsb3003DF2ETInterface(uint32, uint32, const char *, const char *);
    
    // destructor
    virtual ~CUsb3003DF2ETInterface() {}
    
protected:
    // low level
    bool OpenDevice(void);
    bool ResetDevice(void);
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cusb3003df2etinterface_h */
