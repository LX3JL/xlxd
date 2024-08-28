//
//  cdmrididirfile.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 29/12/2017.
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

#ifndef cdmrididirfile_h
#define cdmrididirfile_h

#include "cdmriddir.h"

////////////////////////////////////////////////////////////////////////////////////////

class CDmridDirFile : public CDmridDir
{
public:
    // constructor
    CDmridDirFile();
    
    // destructor
    ~CDmridDirFile() {}
    
    // init & close
    bool Init(void);
    
    // refresh
    bool LoadContent(CBuffer *);
    bool RefreshContent(const CBuffer &);
    
protected:
    // reload helpers
    bool NeedReload(void);
    bool GetLastModTime(time_t *);
    
protected:
    // data
    time_t      m_LastModTime;
 };

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cdmrididirfile_h */
