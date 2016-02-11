//
//  cxlxclient.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 28/01/2016.
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

#include <string.h>
#include "main.h"
#include "cxlxclient.h"


////////////////////////////////////////////////////////////////////////////////////////
// constructors

CXlxClient::CXlxClient()
{
    ::memset(m_ReflectorModules, 0, sizeof(m_ReflectorModules));
}

CXlxClient::CXlxClient(const CCallsign &callsign, const CIp &ip, char *reflectorModules)
: CClient(callsign, ip)
{
    ::memset(m_ReflectorModules, 0, sizeof(m_ReflectorModules));
    if ( reflectorModules != NULL )
    {
        while ( *reflectorModules != 0x00 )
        {
            if ( (*reflectorModules >= 'A') && (*reflectorModules < ('A'+ NB_OF_MODULES)) )
            {
                ::strncat(m_ReflectorModules, reflectorModules, 1);
            }
            reflectorModules++;
        }
    }
    
}

CXlxClient::CXlxClient(const CXlxClient &client)
: CClient(client)
{
    ::strcpy(m_ReflectorModules, client.m_ReflectorModules);
}

////////////////////////////////////////////////////////////////////////////////////////
// status

bool CXlxClient::IsAlive(void) const
{
    return (m_LastKeepaliveTime.DurationSinceNow() < XLX_KEEPALIVE_TIMEOUT);
}

////////////////////////////////////////////////////////////////////////////////////////
// get

bool CXlxClient::HasThisReflectorModule(char module) const
{
    return (::strchr(m_ReflectorModules, module) != NULL);
}

////////////////////////////////////////////////////////////////////////////////////////
// reporting

void CXlxClient::WriteXml(std::ofstream &xmlFile)
{
    xmlFile << "<PEER>" << std::endl;
    xmlFile << "\t<Callsign>" << m_Callsign << "</Callsign>" << std::endl;
    xmlFile << "\t<IP>" << m_Ip << "</IP>" << std::endl;
    xmlFile << "\t<LinkedModule>" << m_ReflectorModules << "</LinkedModule>" << std::endl;
    xmlFile << "\t<Protocol>" << GetProtocolName() << "</Protocol>" << std::endl;
    char mbstr[100];
    if (std::strftime(mbstr, sizeof(mbstr), "%A %c", std::localtime(&m_ConnectTime)))
    {
        xmlFile << "\t<ConnectTime>" << mbstr << "</ConnectTime>" << std::endl;
    }
    if (std::strftime(mbstr, sizeof(mbstr), "%A %c", std::localtime(&m_LastHeardTime)))
    {
        xmlFile << "\t<LastHeardTime>" << mbstr << "</LastHeardTime>" << std::endl;
    }
    xmlFile << "</PEER>" << std::endl;
}


