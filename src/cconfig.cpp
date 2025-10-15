//
//  cconfig.cpp
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

#include "main.h"
#include "cconfig.h"
#include "ccallsign.h"
#include "cip.h"
#include <string.h>


CConfig::CConfig() :
    m_Callsign("N0CALL"),
    m_ListenIp(CIp("0.0.0.0")),
    m_TranscoderIp(CIp("127.0.0.1"))
{
    ReadOptions();
}


void CConfig::DumpConfig()
{
    std::cout << "Configuration options" << std::endl;
    std::cout << "callsign " << GetCallsign() << std::endl;
    std::cout << "listen " << GetListenIp() << std::endl;
    std::cout << "transcoder " << GetTranscoderIp() << std::endl;
    std::cout << std::endl;
}

////////////////////////////////////////////////////////////////////////////////////////
// option helpers

char *CConfig::TrimWhiteSpaces(char *str)
{
    char *end;
    while ((*str == ' ') || (*str == '\t')) str++;
    if (*str == 0)
        return str;
    end = str + strlen(str) - 1;
    while ((end > str) && ((*end == ' ') || (*end == '\t') || (*end == '\r'))) end --;
    *(end + 1) = 0;
    return str;
}

void CConfig::ReadOptions(void)
{
    char sz[256];

    std::ifstream file(CONFIG_PATH);
    if (file.is_open())
    {
        while (file.getline(sz, sizeof(sz)).good())
        {
            char *szt = TrimWhiteSpaces(sz);
            char *szval;

            if ((::strlen(szt) > 0) && szt[0] != '#')
            {
                if ((szt = ::strtok(szt, " ,\t")) != NULL)
                {
                    if ((szval = ::strtok(NULL, " ,\t")) != NULL)
                    {
                        if (::strncmp(szt, "callsign", 8) == 0)
                        {
                            m_Callsign = CCallsign(szval);
                        }
                        else if (strncmp(szt, "listen", 5) == 0)
                        {
                            m_ListenIp = CIp(szval);
                        }
                        else if (strncmp(szt, "transcoder", 10) == 0)
                        {
                            m_TranscoderIp = CIp(szval);
                        }
                        else
                        {
                            // unknown option - ignore
                        }
                    }
                }
            }
        }
    }
}
