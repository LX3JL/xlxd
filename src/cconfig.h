//
//  cconfig.h
//  xlxd
//
//  Created by Alessio Caiazza (IU5BON) on 22/07/2022.
//  Copyright © 2022 Alessio Caiazza (IU5BON). All rights reserved.
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

#ifndef cconfig_h
#define cconfig_h

#include <string>
#include "cip.h"
#include "ccallsign.h"

////////////////////////////////////////////////////////////////////////////////////////
// class

class CConfig
{
public:
    // constructor
    CConfig();

    void DumpConfig();

    // getters
    const CCallsign &GetCallsign(void) const        { return m_Callsign; }
    const CIp &GetListenIp(void) const              { return m_ListenIp; }
    const CIp &GetTranscoderIp(void) const          { return m_TranscoderIp; }

    // setters for bacward compatible CLI parameters
    void SetCallsign(const CCallsign &callsign)     { m_Callsign = callsign; }
    void SetListenIp(const CIp &ip)                 { m_ListenIp = ip; }
    void SetTranscoderIp(const CIp &ip)             { m_TranscoderIp = ip; }

protected:
    // config
    void ReadOptions(void);
    char* TrimWhiteSpaces(char *str);

protected:
    CCallsign       m_Callsign;
    CIp             m_ListenIp;
    CIp             m_TranscoderIp;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cconfig_h */
