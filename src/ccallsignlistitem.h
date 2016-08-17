//
//  ccallsignlistitem.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 31/01/2016.
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

#ifndef ccallsignlistitem_h
#define ccallsignlistitem_h

#include "main.h"
#include "ccallsign.h"
#include "cip.h"

////////////////////////////////////////////////////////////////////////////////////////
// define

#define URL_MAXLEN          256

////////////////////////////////////////////////////////////////////////////////////////
// class

class CCallsignListItem
{
public:
    // constructor
    CCallsignListItem();
    CCallsignListItem(const CCallsign &, const CIp &, const char *);
    CCallsignListItem(const CCallsign &, const char *, const char *);
    CCallsignListItem(const CCallsignListItem &);
    
    // destructor
    virtual ~CCallsignListItem() {}

    // compare
    bool HasSameCallsign(const CCallsign &) const;
    bool HasSameCallsignWithWidlcard(const CCallsign &) const;
    bool HasModuleListed(char) const;
    bool CheckListedModules(char*) const;
    
    // get
    const CCallsign &GetCallsign(void) const        { return m_Callsign; }
    const CIp &GetIp(void) const                    { return m_Ip; }
    const char *GetModules(void)                    { return m_Modules; }
    
    // update
    void ResolveIp(void)                            { m_Ip = CIp(m_szUrl); }
    
protected:
    // data
    CCallsign   m_Callsign;
    char        m_szUrl[URL_MAXLEN+1];
    CIp         m_Ip;
    char        m_Modules[NB_MODULES_MAX+1];
};


////////////////////////////////////////////////////////////////////////////////////////

#endif /* ccallsignlistitem_h */
