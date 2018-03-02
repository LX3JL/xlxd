//
//  cpeer.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 10/12/2016.
//  Copyright © 2016 Jean-Luc Deltombe (LX3JL). All rights reserved.
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

#ifndef cpeer_h
#define cpeer_h

#include "cversion.h"
#include "ctimepoint.h"
#include "cip.h"
#include "ccallsign.h"
#include "cclient.h"

////////////////////////////////////////////////////////////////////////////////////////
//

////////////////////////////////////////////////////////////////////////////////////////
// class

class CPeer
{
public:
    // constructors
    CPeer();
    CPeer(const CCallsign &, const CIp &, const char *, const CVersion &);
    CPeer(const CPeer &);
    
    // destructor
    virtual ~CPeer();
    
    // operators
    bool operator ==(const CPeer &) const;
    
    // get
    const CCallsign &GetCallsign(void) const            { return m_Callsign; }
    const CIp &GetIp(void) const                        { return m_Ip; }
    char *GetReflectorModules(void)                     { return m_ReflectorModules; }
    
    // set
    
    // identity
    virtual int GetProtocol(void) const                 { return PROTOCOL_NONE; }
    virtual int GetProtocolRevision(void) const         { return 0; }
    virtual const char *GetProtocolName(void) const     { return "none"; }
    
    // status
    virtual bool IsAMaster(void) const;
    virtual void Alive(void);
    virtual bool IsAlive(void) const                    { return false; }
    virtual void Heard(void)                            { m_LastHeardTime = std::time(NULL); }
    
    // clients access
    int     GetNbClients(void) const                    { return (int)m_Clients.size(); }
    void    ClearClients(void)                          { m_Clients.clear(); }
    CClient *GetClient(int);
    
    // reporting
    virtual void WriteXml(std::ofstream &);
    virtual void GetJsonObject(char *);
    
protected:
    // data
    CCallsign               m_Callsign;
    CIp                     m_Ip;
    char                    m_ReflectorModules[NB_MODULES_MAX+1];
    CVersion                m_Version;
    std::vector<CClient *>  m_Clients;
    
    // status
    CTimePoint              m_LastKeepaliveTime;
    std::time_t             m_ConnectTime;
    std::time_t             m_LastHeardTime;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cpeer_h */
