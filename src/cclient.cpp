//
//  cclient.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 31/10/2015.
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

#include "main.h"
#include <string.h>
#include "cclient.h"


////////////////////////////////////////////////////////////////////////////////////////
// constructors

CClient::CClient()
{
    m_ReflectorModule = ' ';
    m_ModuleMastered = ' ';
    m_LastKeepaliveTime.Now();
    m_ConnectTime = std::time(NULL);
    m_LastHeardTime = std::time(NULL);
}

CClient::CClient(const CCallsign &callsign, const CIp &ip, char reflectorModule)
{
    m_ReflectorModule = reflectorModule;
    m_Callsign = callsign;
    m_Ip = ip;
    m_ModuleMastered = ' ';
    m_LastKeepaliveTime.Now();
    m_ConnectTime = std::time(NULL);
    m_LastHeardTime = std::time(NULL);
}

CClient::CClient(const CClient &client)
{
    m_Callsign = client.m_Callsign;
    m_Ip = client.m_Ip;
    m_ReflectorModule = client.m_ReflectorModule;
    m_ModuleMastered = client.m_ModuleMastered;
    m_LastKeepaliveTime = client.m_LastKeepaliveTime;
    m_ConnectTime = client.m_ConnectTime;
    m_LastHeardTime = client.m_LastHeardTime;
}

////////////////////////////////////////////////////////////////////////////////////////
// status

void CClient::Alive(void)
{
    m_LastKeepaliveTime.Now();;
}


////////////////////////////////////////////////////////////////////////////////////////
// operators

bool CClient::operator ==(const CClient &client) const
{
    return ((client.m_Callsign == m_Callsign) &&
            (client.m_Ip == m_Ip));
}

////////////////////////////////////////////////////////////////////////////////////////
// reporting

void CClient::WriteXml(std::ofstream &xmlFile)
{
    xmlFile << "<NODE>" << std::endl;
    xmlFile << "\t<Callsign>" << m_Callsign << "</Callsign>" << std::endl;
    xmlFile << "\t<IP>" << m_Ip << "</IP>" << std::endl;
    xmlFile << "\t<LinkedModule>" << m_ReflectorModule << "</LinkedModule>" << std::endl;
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
    xmlFile << "</NODE>" << std::endl;
}

void CClient::GetJsonObject(char *Buffer)
{
    char sz[512];
    char mbstr[100];
    char cs[16];
    
    if (std::strftime(mbstr, sizeof(mbstr), "%A %c", std::localtime(&m_LastHeardTime)))
    {
        m_Callsign.GetCallsignString(cs);
        
        ::sprintf(sz, "{\"callsign\":\"%s\",\"module\":\"%c\",\"linkedto\":\"%c\",\"time\":\"%s\"}",
                  cs,
                  m_Callsign.GetModule(),
                  m_ReflectorModule,
                  mbstr);
        ::strcat(Buffer, sz);
    }
}
