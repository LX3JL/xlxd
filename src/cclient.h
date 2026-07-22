//
//  cclient.h
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

#ifndef cclient_h
#define cclient_h

#include "ctimepoint.h"
#include "cip.h"
#include "ccallsign.h"
#include "cbuffer.h"
#include <random>

////////////////////////////////////////////////////////////////////////////////////////
// defines

// Jitter ranges for staggering keepalives and connection acks
#define KEEPALIVE_JITTER_MAX        5.0    // seconds - max random delay for first outbound keepalive
#define CONNECT_ACK_JITTER_MAX      2.0    // seconds - max random delay for connection ack

////////////////////////////////////////////////////////////////////////////////////////
// class

class CClient
{
public:
    // constructors
    CClient();
    CClient(const CCallsign &, const CIp &, char = ' ');
    CClient(const CClient &);
    
    // destructor
    virtual ~CClient() {};
    
    // operators
    bool operator ==(const CClient &) const;
    
    // get
    const CCallsign &GetCallsign(void) const            { return m_Callsign; }
    const CIp &GetIp(void) const                        { return m_Ip; }
    bool HasModule(void) const                          { return m_Callsign.HasModule(); }
    char GetModule(void) const                          { return m_Callsign.GetModule(); }
    bool HasReflectorModule(void) const                 { return m_ReflectorModule != ' '; }
    char GetReflectorModule(void) const                 { return m_ReflectorModule; }
   
    // set
    void SetModule(char c)                              { m_Callsign.SetModule(c); }
    void SetReflectorModule(char c)                     { m_ReflectorModule = c; }
    
    // identity
    virtual int GetProtocol(void) const                 { return PROTOCOL_NONE; }
    virtual int GetProtocolRevision(void) const         { return 0; }
    virtual int GetCodec(void) const                    { return CODEC_NONE; }
    // Bitmask of codecs this client provides on the wire (one per
    // CODEC_* bit position). Default = the single codec returned by
    // GetCodec, expressed as a bitmask. Multi-codec source clients
    // (XLX interlink peers in particular) override to return the
    // actual set: rev 0/1 = AMBE+ only, rev 2 = AMBE+/AMBE+2,
    // rev 3 = AMBE+/AMBE+2/Codec2. This is read by
    // CReflector::OpenStream to decide whether the receiving side
    // needs to engage AMBEd to fill missing codecs for the clients
    // connected to the target module.
    virtual uint32 GetAvailableCodecs(void) const
    {
        int c = GetCodec();
        return (c == CODEC_NONE) ? 0U : (1U << c);
    }
    virtual const char *GetProtocolName(void) const     { return "none"; }
    virtual bool IsNode(void) const                     { return false; }
    virtual bool IsPeer(void) const                     { return false; }
    virtual bool IsDextraDongle(void) const             { return false; }
    virtual void SetDextraDongle(void)                  { }
    
    // status
    virtual void Alive(void);
    virtual bool IsAlive(void) const                    { return false; }
    virtual bool IsAMaster(void) const                  { return (m_ModuleMastered != ' '); }
    virtual void SetMasterOfModule(char c)              { m_ModuleMastered = c; }
    virtual void NotAMaster(void)                       { m_ModuleMastered = ' '; }
    virtual void Heard(void)                            { m_LastHeardTime = std::time(NULL); }

    // keepalive scheduling (for outbound keepalives from reflector)
    double GetNextKeepaliveTime(void) const             { return m_NextKeepaliveTime; }
    void SetNextKeepaliveTime(double t)                 { m_NextKeepaliveTime = t; }
    void ScheduleNextKeepalive(double period)           { m_NextKeepaliveTime = period; }
    bool IsKeepaliveDue(void) const                     { return m_KeepaliveTimer.DurationSinceNow() >= m_NextKeepaliveTime; }
    void ResetKeepaliveTimer(void)                      { m_KeepaliveTimer.Now(); }

    // static helper for random jitter
    static double GetRandomJitter(double maxJitter);
    
    // reporting
    virtual void WriteXml(std::ofstream &);
    virtual void GetJsonObject(char *);

protected:
    // data
    CCallsign   m_Callsign;
    CIp         m_Ip;

    // linked to
    char        m_ReflectorModule;

    // status
    char        m_ModuleMastered;
    CTimePoint  m_LastKeepaliveTime;
    std::time_t m_ConnectTime;
    std::time_t m_LastHeardTime;

    // outbound keepalive scheduling (for staggering keepalives to clients)
    CTimePoint  m_KeepaliveTimer;       // when timer was last reset
    double      m_NextKeepaliveTime;    // seconds until next outbound keepalive
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cclient_h */
