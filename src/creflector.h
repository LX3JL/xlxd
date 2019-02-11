//
//  creflector.h
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

#ifndef creflector_h
#define creflector_h

#include "cusers.h"
#include "cclients.h"
#include "cpeers.h"
#include "cprotocols.h"
#include "cpacketstream.h"
#include "cnotificationqueue.h"


////////////////////////////////////////////////////////////////////////////////////////
// define

// event defines
#define EVENT_NONE      0
#define EVENT_CLIENTS   1
#define EVENT_USERS     2

////////////////////////////////////////////////////////////////////////////////////////
// class

class CReflector
{
public:
    // constructor
    CReflector();
    CReflector(const CCallsign &);
    
    // destructor
    virtual ~CReflector();
    
    // settings
    void SetCallsign(const CCallsign &callsign)     { m_Callsign = callsign; }
    const CCallsign &GetCallsign(void) const        { return m_Callsign; }
    void SetListenIp(const CIp &ip)                 { m_Ip = ip; }
    void SetTranscoderIp(const CIp &ip)             { m_AmbedIp = ip; }
    const CIp &GetListenIp(void) const              { return m_Ip; }
    const CIp &GetTranscoderIp(void) const          { return m_AmbedIp; }
    
    // operation
    bool Start(void);
    void Stop(void);
    
    // clients
    CClients  *GetClients(void)                     { m_Clients.Lock(); return &m_Clients; }
    void      ReleaseClients(void)                  { m_Clients.Unlock(); }
    
    // peers
    CPeers   *GetPeers(void)                        { m_Peers.Lock(); return &m_Peers; }
    void      ReleasePeers(void)                    { m_Peers.Unlock(); }
    
    // stream opening & closing
    CPacketStream *OpenStream(CDvHeaderPacket *, CClient *, uint8 CodecIn);
    bool IsStreaming(char);
    void CloseStream(CPacketStream *);
    
    // users
    CUsers  *GetUsers(void)                         { m_Users.Lock(); return &m_Users; }
    void    ReleaseUsers(void)                      { m_Users.Unlock(); }
    
    // get
    bool IsValidModule(char c) const                { return (GetModuleIndex(c) >= 0); }
    int  GetModuleIndex(char) const;
    char GetModuleLetter(int i) const               { return 'A' + (char)i; }
    
    // notifications
    void OnPeersChanged(void);
    void OnClientsChanged(void);
    void OnUsersChanged(void);
    void OnStreamOpen(const CCallsign &);
    void OnStreamClose(const CCallsign &);
    
protected:
    // threads
    static void RouterThread(CReflector *, CPacketStream *);
    static void XmlReportThread(CReflector *);
    static void JsonReportThread(CReflector *);
    
    // streams
    CPacketStream *GetStream(char);
    bool          IsStreamOpen(const CDvHeaderPacket *);
    char          GetStreamModule(CPacketStream *);
    
    // xml helpers
    void WriteXmlFile(std::ofstream &);
    
    // json helpers
    void SendJsonReflectorObject(CUdpSocket &, CIp &);
    void SendJsonNodesObject(CUdpSocket &, CIp &);
    void SendJsonStationsObject(CUdpSocket &, CIp &);
    void SendJsonOnairObject(CUdpSocket &, CIp &, const CCallsign &);
    void SendJsonOffairObject(CUdpSocket &, CIp &, const CCallsign &);
    
protected:
    // identity
    CCallsign       m_Callsign;
    CIp             m_Ip;
    CIp             m_AmbedIp;
    
    // objects
    CUsers          m_Users;            // sorted list of lastheard stations
    CClients        m_Clients;          // list of linked repeaters/nodes/peers's modules
    CPeers          m_Peers;            // list of linked peers
    CProtocols      m_Protocols;        // list of supported protocol handlers
    
    // queues
    std::array<CPacketStream, NB_OF_MODULES> m_Streams;
    
    // threads
    bool            m_bStopThreads;
    std::array<std::thread *, NB_OF_MODULES> m_RouterThreads;
    std::thread    *m_XmlReportThread;
    std::thread    *m_JsonReportThread;
    
    // notifications
    CNotificationQueue  m_Notifications;
    
public:
#ifdef DEBUG_DUMPFILE
    std::ofstream        m_DebugFile;
#endif
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* creflector_h */
