//
//  cuser.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 13/11/2015.
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
#include "cuser.h"


////////////////////////////////////////////////////////////////////////////////////////
// constructors

CUser::CUser()
{
    m_LastHeardTime = std::time(NULL);
}

CUser::CUser(const CCallsign &my, const CCallsign &rpt1, const CCallsign &rpt2, const CCallsign &xlx)
{
    m_My = my;
    m_Rpt1 = rpt1;
    m_Rpt2 = rpt2;
    m_Xlx = xlx;
    m_LastHeardTime = std::time(NULL);
}

CUser::CUser(const CUser &user)
{
    m_My = user.m_My;
    m_Rpt1 = user.m_Rpt1;
    m_Rpt2 = user.m_Rpt2;
    m_Xlx = user.m_Xlx;
    m_LastHeardTime = user.m_LastHeardTime;
}

////////////////////////////////////////////////////////////////////////////////////////
// operators

bool CUser::operator ==(const CUser &user) const
{
    return ((user.m_My == m_My) && (user.m_Rpt1 == m_Rpt1) && (user.m_Rpt2 == m_Rpt2)  && (user.m_Xlx == m_Xlx));
}


bool CUser::operator <(const CUser &user) const
{
    // smallest is youngest
    return (std::difftime(m_LastHeardTime, user.m_LastHeardTime) > 0);
}

////////////////////////////////////////////////////////////////////////////////////////
// reporting

void CUser::WriteXml(std::ofstream &xmlFile)
{
    xmlFile << "<STATION>" << std::endl;
    xmlFile << "\t<Callsign>" << m_My << "</Callsign>" << std::endl;
    xmlFile << "\t<DMRID>" << m_My.GetDmrid() << "</DMRID>" << std::endl;
    xmlFile << "\t<Via node>" << m_Rpt1 << "</Via node>" << std::endl;
    xmlFile << "\t<On module>" << m_Rpt2.GetModule() << "</On module>" << std::endl;
    xmlFile << "\t<Via peer>" << m_Xlx << "</Via peer>" << std::endl;

    char mbstr[100];
    if (std::strftime(mbstr, sizeof(mbstr), "%A %c", std::localtime(&m_LastHeardTime)))
    {
        xmlFile << "\t<LastHeardTime>" << mbstr << "</LastHeardTime>" << std::endl;
    }
    xmlFile << "</STATION>" << std::endl;
}

void CUser::GetJsonObject(char *Buffer)
{
    char sz[512];
    char mbstr[100];
    char my[16];
    char rpt1[16];
    
    if (std::strftime(mbstr, sizeof(mbstr), "%A %c", std::localtime(&m_LastHeardTime)))
    {
        m_My.GetCallsignString(my);
        m_Rpt1.GetCallsignString(rpt1);
        
        ::sprintf(sz, "{\"callsign\":\"%s\",\"node\":\"%s\",\"module\":\"%c\",\"time\":\"%s\"}",
              my,
              rpt1,
              m_Rpt1.GetModule(),
              mbstr);
        ::strcat(Buffer, sz);
    }
}
