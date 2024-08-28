//
//  cversion.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 05/01/2018.
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


#ifndef cversion_h
#define cversion_h


////////////////////////////////////////////////////////////////////////////////////////
// class

class CVersion
{
public:
    // constructor
    CVersion();
    CVersion(int, int, int);
    CVersion(const CVersion &);
    
    // destructor
    virtual ~CVersion() {}
    
    // get
    int GetMajor(void) const        { return m_iMajor; }
    int GetMinor(void) const        { return m_iMinor; }
    int GetRevision(void) const     { return m_iRevision; }

    // comparaison
    bool IsEqualOrHigherTo(const CVersion &) const;
    
    // operator
    bool operator ==(const CVersion &) const;

protected:
    // data
    int     m_iMajor;
    int     m_iMinor;
    int     m_iRevision;
};


////////////////////////////////////////////////////////////////////////////////////////
#endif /* cversion_h */
