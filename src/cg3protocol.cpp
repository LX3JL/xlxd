//
//  cg3protocol.cpp
//  xlxd
//
//  Created by Marius Petrescu (YO2LOJ) on 03/06/2019.
//  Copyright © 2019 Marius Petrescu (YO2LOJ). All rights reserved.
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
#include <sys/stat.h>
#include "cg3client.h"
#include "cg3protocol.h"
#include "creflector.h"
#include "cgatekeeper.h"

#include <netinet/ip.h>
#include <netinet/ip_icmp.h>


////////////////////////////////////////////////////////////////////////////////////////
// operation

bool CG3Protocol::Init(void)
{
    bool ok;

    ReadOptions();

    // base class
    ok = CProtocol::Init();

    // update reflector callsign
    m_ReflectorCallsign.PatchCallsign(0, (const uint8 *)"XLX", 3);

    // create our DV socket
    ok &= m_Socket.Open(G3_DV_PORT);
    if ( !ok )
    {
        std::cout << "Error opening socket on port UDP" << G3_DV_PORT << " on ip " << g_Reflector.GetListenIp() << std::endl;
    }

    //create helper sockets
    ok &= m_PresenceSocket.Open(G3_PRESENCE_PORT);
    if ( !ok )
    {
        std::cout << "Error opening socket on port UDP" << G3_PRESENCE_PORT << " on ip " << g_Reflector.GetListenIp() << std::endl;
    }

    ok &= m_ConfigSocket.Open(G3_CONFIG_PORT);
    if ( !ok )
    {
        std::cout << "Error opening socket on port UDP" << G3_CONFIG_PORT << " on ip " << g_Reflector.GetListenIp() << std::endl;
    }

    ok &= ((m_IcmpRawSocket = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) >= 0);
    if ( !ok )
    {
        std::cout << "Error opening raw socket for ICMP" << std::endl;
    }

    if (ok)
    {
        // start helper threads
        m_pPresenceThread = new std::thread(PresenceThread, this);
        m_pPresenceThread = new std::thread(ConfigThread, this);
        m_pPresenceThread = new std::thread(IcmpThread, this);
    }

    // update time
    m_LastKeepaliveTime.Now();

    // done
    return ok;
}

void CG3Protocol::Close(void)
{
    if (m_pPresenceThread != NULL)
    {
        m_pPresenceThread->join();
        delete m_pPresenceThread;
        m_pPresenceThread = NULL;
    }

    if (m_pConfigThread != NULL)
    {
        m_pConfigThread->join();
        delete m_pConfigThread;
        m_pConfigThread = NULL;
    }

    if (m_pIcmpThread != NULL)
    {
        m_pIcmpThread->join();
        delete m_pIcmpThread;
        m_pIcmpThread = NULL;
    }
}


////////////////////////////////////////////////////////////////////////////////////////
// private threads

void CG3Protocol::PresenceThread(CG3Protocol *This)
{
    while ( !This->m_bStopThread )
    {
        This->PresenceTask();
    }
}

void CG3Protocol::ConfigThread(CG3Protocol *This)
{
    while ( !This->m_bStopThread )
    {
        This->ConfigTask();
    }
}

void CG3Protocol::IcmpThread(CG3Protocol *This)
{
    fcntl(This->m_IcmpRawSocket, F_SETFL, O_NONBLOCK);

    while ( !This->m_bStopThread )
    {
        This->IcmpTask();
    }

    close(This->m_IcmpRawSocket);
}


////////////////////////////////////////////////////////////////////////////////////////
// presence task

void CG3Protocol::PresenceTask(void)
{
    CBuffer             Buffer;
    CIp                 ReqIp;
    CCallsign           Callsign;
    CCallsign           Owner;
    CCallsign           Terminal;


    if ( m_PresenceSocket.Receive(&Buffer, &ReqIp, 20) != -1 )
    {

        CIp Ip(ReqIp);
        Ip.GetSockAddr()->sin_port = htons(G3_DV_PORT);

        if (Buffer.size() == 32)
        {
            Callsign.SetCallsign(&Buffer.data()[8], 8);
            Owner.SetCallsign(&Buffer.data()[16], 8);
            Terminal.SetCallsign(&Buffer.data()[24], 8);

            std::cout << "Presence from " << Ip << " as " << Callsign << " on terminal " << Terminal << std::endl; 

            // accept
            Buffer.data()[2] = 0x80; // response
            Buffer.data()[3] = 0x00; // ok

            if (m_GwAddress == 0)
            {
                Buffer.Append(*(uint32 *)m_ConfigSocket.GetLocalAddr()); 
            }
            else
            {
                Buffer.Append(m_GwAddress);
            }

            CClients *clients = g_Reflector.GetClients();

            int index = -1;
            CClient *extant = NULL;
            while ( (extant = clients->FindNextClient(PROTOCOL_G3, &index)) != NULL )
            {
                CIp ClIp = extant->GetIp();
                if (ClIp.GetAddr() == Ip.GetAddr())
                {
                    break;
                }
            }

            if (extant == NULL)
            {
                index = -1;
                
                // do we already have a client with the same call (IP changed)?
                while ( (extant = clients->FindNextClient(PROTOCOL_G3, &index)) != NULL )
                {
                    {
                        if (extant->GetCallsign().HasSameCallsign(Terminal))
                        {
                            //delete old client
                            clients->RemoveClient(extant);
                            break;
                        }
                    }
                }

                // create new client
                CG3Client *client = new CG3Client(Terminal, Ip);

                // and append
                clients->AddClient(client);
            }
            else
            {
                // client changed callsign
                if (!extant->GetCallsign().HasSameCallsign(Terminal))
                {
                    //delete old client
                    clients->RemoveClient(extant);

                    // create new client
                    CG3Client *client = new CG3Client(Terminal, Ip);

                    // and append
                    clients->AddClient(client);
                }
            }
            g_Reflector.ReleaseClients();

            m_PresenceSocket.Send(Buffer, ReqIp);
        }
    }
}


////////////////////////////////////////////////////////////////////////////////////////
// configuration task

void CG3Protocol::ConfigTask(void)
{
    CBuffer             Buffer;
    CIp                 Ip;
    CCallsign           Call;
    bool                isRepeaterCall;

    if ( m_ConfigSocket.Receive(&Buffer, &Ip, 20) != -1 )
    {

        if (Buffer.size() == 16)
        {
            if (memcmp(&Buffer.data()[8], "        ", 8) == 0)
            {
                Call.SetCallsign(GetReflectorCallsign(), 8);
            }
            else
            {
                Call.SetCallsign(&Buffer.data()[8], 8);
            }

            isRepeaterCall = ((Buffer.data()[2] & 0x10) == 0x10);

            std::cout << "Config request from " << Ip << " for " << Call << " (" << ((char *)(isRepeaterCall)?"repeater":"routed") << ")" << std::endl;

            //std::cout << "Local address: " << inet_ntoa(*m_ConfigSocket.GetLocalAddr()) << std::endl;

            Buffer.data()[2] |= 0x80; // response

            if (isRepeaterCall)
            {
                if ((Call.HasSameCallsign(GetReflectorCallsign())) && (g_Reflector.IsValidModule(Call.GetModule())))
                {
                    Buffer.data()[3] = 0x00; // ok
                }
                else
                {
                    std::cout << "Module " << Call << " invalid" << std::endl;
                    Buffer.data()[3] = 0x01; // reject
                }
            }
            else
            {
                // reject routed calls for now
                Buffer.data()[3] = 0x01; // reject
            }

            char module = Call.GetModule();

            if (!strchr(m_Modules.c_str(), module) && !strchr(m_Modules.c_str(), '*'))
            {
                // restricted
                std::cout << "Module " << Call << " restricted by configuration" << std::endl;
                Buffer.data()[3] = 0x01; // reject
            }

            // UR
            Buffer.resize(8);
            Buffer.Append((uint8 *)(const char *)Call, CALLSIGN_LEN - 1);
            Buffer.Append((uint8)module);

            // RPT1
            Buffer.Append((uint8 *)(const char *)GetReflectorCallsign(), CALLSIGN_LEN - 1);
            Buffer.Append((uint8)'G');

            // RPT2
            Buffer.Append((uint8 *)(const char *)GetReflectorCallsign(), CALLSIGN_LEN - 1);

            if (isRepeaterCall)
            {
                Buffer.Append((uint8)Call.GetModule());
            }
            else
            {
                // routed - no module for now
                Buffer.Append((uint8)' ');
            }

            if (Buffer.data()[3] == 0x00)
            {
                std::cout << "External G3 gateway address " << inet_ntoa(*(in_addr *)&m_GwAddress) << std::endl;

                if (m_GwAddress == 0)
                {
                    Buffer.Append(*(uint32 *)m_ConfigSocket.GetLocalAddr()); 
                }
                else
                {
                    Buffer.Append(m_GwAddress);
                }
            }
            else
            {
                Buffer.Append(0u);
            }

            m_ConfigSocket.Send(Buffer, Ip);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// icmp task

void CG3Protocol::IcmpTask(void)
{
    unsigned char buffer[256];

    struct sockaddr_in sa;
    unsigned int sasize = sizeof(sa);

    fd_set FdSet;
    struct timeval tv;

    if (m_IcmpRawSocket != -1)
    {
        FD_ZERO(&FdSet);
        FD_SET(m_IcmpRawSocket, &FdSet);
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        select(m_IcmpRawSocket + 1, &FdSet, 0, 0, &tv);

        int len = recvfrom(m_IcmpRawSocket, buffer, 255, 0, (struct sockaddr *)&sa, &sasize);

        if (len > 28)
        {
            struct ip *iph = (struct ip *)buffer;
            int iphdrlen = iph->ip_hl * 4;
            struct icmp *icmph = (struct icmp *)(buffer + iphdrlen);

            if (icmph->icmp_type == ICMP_DEST_UNREACH)
            {
                struct ip *remote_iph = (struct ip *)(buffer + iphdrlen + 8);

                CClients *clients = g_Reflector.GetClients();

                int index = -1;
                CClient *client = NULL;
                while ( (client = clients->FindNextClient(PROTOCOL_G3, &index)) != NULL )
                {
                    CIp Ip = client->GetIp();
                    if (Ip.GetAddr() == remote_iph->ip_dst.s_addr)
                    {
                        clients->RemoveClient(client);
                    }
                }
                g_Reflector.ReleaseClients();

            }
        }
    }
}


////////////////////////////////////////////////////////////////////////////////////////
// DV task

void CG3Protocol::Task(void)
{
    CBuffer             Buffer;
    CIp                 Ip;
    CCallsign           Callsign;
    char                ToLinkModule;
    int                 ProtRev;
    CDvHeaderPacket     *Header;
    CDvFramePacket      *Frame;
    CDvLastFramePacket  *LastFrame;

    // any incoming packet ?
    if ( m_Socket.Receive(&Buffer, &Ip, 20) != -1 )
    {
        CIp ClIp;
        CIp *BaseIp = NULL;
        CClients *clients = g_Reflector.GetClients();
        int index = -1;
        CClient *client = NULL;
        while ( (client = clients->FindNextClient(PROTOCOL_G3, &index)) != NULL )
        {
            ClIp = client->GetIp();
            if (ClIp.GetAddr() == Ip.GetAddr())
            {
                BaseIp = &ClIp;
                client->Alive();
                // supress host checks
                m_LastKeepaliveTime.Now();
                break;
            }
        }
        g_Reflector.ReleaseClients();

        if (BaseIp != NULL)
        {
            // crack the packet
            if ( (Frame = IsValidDvFramePacket(Buffer)) != NULL )
            {
                //std::cout << "Terminal DV frame"  << std::endl;

                // handle it
                OnDvFramePacketIn(Frame, BaseIp);
            }
            else if ( (Header = IsValidDvHeaderPacket(Buffer)) != NULL )
            {
                //std::cout << "Terminal DV header"  << std::endl;

                // callsign muted?
                if ( g_GateKeeper.MayTransmit(Header->GetMyCallsign(), Ip, PROTOCOL_G3, Header->GetRpt2Module()) )
                {
                    // handle it
                    OnDvHeaderPacketIn(Header, *BaseIp);
                }
                else
                {
                    delete Header;
                }
            }
            else if ( (LastFrame = IsValidDvLastFramePacket(Buffer)) != NULL )
            {
                //std::cout << "Terminal DV last frame" << std::endl;
            
                // handle it
                OnDvLastFramePacketIn(LastFrame, BaseIp);
            }
            else
            {
                //std::cout << "Invalid terminal packet (" << Buffer.size() << ")" << std::endl;
                //std::cout << Buffer.data() << std::endl;
            }
        }
        else
        {
            //std::cout << "Invalid client " <<  Ip << std::endl;
        }
    }

    // handle end of streaming timeout
    CheckStreamsTimeout();
        
    // handle queue from reflector
    HandleQueue();
        
    // keep alive
    if ( m_LastKeepaliveTime.DurationSinceNow() > G3_KEEPALIVE_PERIOD )
    {
        // handle keep alives
        HandleKeepalives();
        
        // update time
        m_LastKeepaliveTime.Now();

        // reload option if needed
        NeedReload();
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// queue helper

void CG3Protocol::HandleQueue(void)
{
    m_Queue.Lock();
    while ( !m_Queue.empty() )
    {
        // supress host checks
        m_LastKeepaliveTime.Now();

        // get the packet
        CPacket *packet = m_Queue.front();
        m_Queue.pop();
        
        // encode it
        CBuffer buffer;
        if ( EncodeDvPacket(*packet, &buffer) )
        {
            // and push it to all our clients linked to the module and who are not streaming in
            CClients *clients = g_Reflector.GetClients();
            int index = -1;
            CClient *client = NULL;
            while ( (client = clients->FindNextClient(PROTOCOL_G3, &index)) != NULL )
            {
                // is this client busy ?
                if ( !client->IsAMaster() && (client->GetReflectorModule() == packet->GetModuleId()) )
                {
                    // not busy, send the packet
                    int n = packet->IsDvHeader() ? 5 : 1;
                    for ( int i = 0; i < n; i++ )
                    {
                        m_Socket.Send(buffer, client->GetIp());
                    }
                }
            }
            g_Reflector.ReleaseClients();
        }
        
        // done
        delete packet;
    }
    m_Queue.Unlock();
}

////////////////////////////////////////////////////////////////////////////////////////
// keepalive helpers

void CG3Protocol::HandleKeepalives(void)
{
    // G3 Terminal mode does not support keepalive
    // We will send some short packed and expect
    // A ICMP unreachable on failure
    CBuffer keepalive((uint8 *)"PING", 4);

    // iterate on clients
    CClients *clients = g_Reflector.GetClients();
    int index = -1;
    CClient *client = NULL;
    while ( (client = clients->FindNextClient(PROTOCOL_G3, &index)) != NULL )
    {
        if (!client->IsAlive())
        {
            clients->RemoveClient(client);
        }
        else
        {
            // send keepalive packet
            m_Socket.Send(keepalive, client->GetIp());
        }
    }
    g_Reflector.ReleaseClients();
}

////////////////////////////////////////////////////////////////////////////////////////
// streams helpers

bool CG3Protocol::OnDvHeaderPacketIn(CDvHeaderPacket *Header, const CIp &Ip)
{
    bool newstream = false;

    // find the stream
    CPacketStream *stream = GetStream(Header->GetStreamId(), &Ip);

    if ( stream == NULL )
    {
        // no stream open yet, open a new one
        CCallsign via(Header->GetRpt1Callsign());

        // find this client
        CClients *clients = g_Reflector.GetClients();

        int index = -1;
        CClient *client = NULL;
        while ( (client = clients->FindNextClient(PROTOCOL_G3, &index)) != NULL )
        {
            CIp ClIp = client->GetIp();
            if (ClIp.GetAddr() == Ip.GetAddr())
            {
                break;
            }
        }

        if ( client != NULL )
        {

            // move it to the proper module
            if (m_ReflectorCallsign.HasSameCallsign(Header->GetRpt2Callsign()))
            {
                if (client->GetReflectorModule() != Header->GetRpt2Callsign().GetModule())
                {
                    char new_module = Header->GetRpt2Callsign().GetModule();
                    if (strchr(m_Modules.c_str(), '*') || strchr(m_Modules.c_str(), new_module))
                    {
                        client->SetReflectorModule(new_module);
                    }
                    else
                    {
                        // drop if invalid module
                        delete Header;
                        g_Reflector.ReleaseClients();
                        return NULL;
                    }
                }

                // get client callsign
                via = client->GetCallsign();

                // and try to open the stream
                if ( (stream = g_Reflector.OpenStream(Header, client)) != NULL )
                {
                    // keep the handle
                    m_Streams.push_back(stream);
                    newstream = true;
                }

                // update last heard
                g_Reflector.GetUsers()->Hearing(Header->GetMyCallsign(), via, Header->GetRpt2Callsign());
                g_Reflector.ReleaseUsers();

                // delete header if needed
                if ( !newstream )
                {
                    delete Header;
                }

            }
            else
            {
                // drop
                delete Header;
            }
        }
        // release
        g_Reflector.ReleaseClients();
    }
    else
    {
        // stream already open
        // skip packet, but tickle the stream
        stream->Tickle();
        // and delete packet
        delete Header;
    }

    // done
    return newstream;
}


////////////////////////////////////////////////////////////////////////////////////////
// packet decoding helpers

CDvHeaderPacket *CG3Protocol::IsValidDvHeaderPacket(const CBuffer &Buffer)
{
    CDvHeaderPacket *header = NULL;
    
    if ( (Buffer.size() == 56) && (Buffer.Compare((uint8 *)"DSVT", 4) == 0) &&
         (Buffer.data()[4] == 0x10) && (Buffer.data()[8] == 0x20) )
    {
        // create packet
        header = new CDvHeaderPacket((struct dstar_header *)&(Buffer.data()[15]),
                                *((uint16 *)&(Buffer.data()[12])), 0x80);
        // check validity of packet
        if ( !header->IsValid() )
        {
            delete header;
            header = NULL;
        }
    }
    return header;
}

CDvFramePacket *CG3Protocol::IsValidDvFramePacket(const CBuffer &Buffer)
{
    CDvFramePacket *dvframe = NULL;
    
    if ( (Buffer.size() == 27) && (Buffer.Compare((uint8 *)"DSVT", 4) == 0) &&
         (Buffer.data()[4] == 0x20) && (Buffer.data()[8] == 0x20) &&
         ((Buffer.data()[14] & 0x40) == 0) )
    {
        // create packet
        dvframe = new CDvFramePacket((struct dstar_dvframe *)&(Buffer.data()[15]),
                                     *((uint16 *)&(Buffer.data()[12])), Buffer.data()[14]);
        // check validity of packet
        if ( !dvframe->IsValid() )
        {
            delete dvframe;
            dvframe = NULL;
        }
    }
    return dvframe;
}

CDvLastFramePacket *CG3Protocol::IsValidDvLastFramePacket(const CBuffer &Buffer)
{
    CDvLastFramePacket *dvframe = NULL;
    
    if ( (Buffer.size() == 27) && (Buffer.Compare((uint8 *)"DSVT", 4) == 0) &&
         (Buffer.data()[4] == 0x20) && (Buffer.data()[8] == 0x20) &&
         ((Buffer.data()[14] & 0x40) != 0) )
    {
        // create packet
        dvframe = new CDvLastFramePacket((struct dstar_dvframe *)&(Buffer.data()[15]),
                                         *((uint16 *)&(Buffer.data()[12])), Buffer.data()[14]);
        // check validity of packet
        if ( !dvframe->IsValid() )
        {
            delete dvframe;
            dvframe = NULL;
        }
    }
    return dvframe;
}

////////////////////////////////////////////////////////////////////////////////////////
// packet encoding helpers

bool CG3Protocol::EncodeDvHeaderPacket(const CDvHeaderPacket &Packet, CBuffer *Buffer) const
{
    uint8 tag[]	= { 'D','S','V','T',0x10,0x00,0x00,0x00,0x20,0x00,0x01,0x02 };
    struct dstar_header DstarHeader;
    
    Packet.ConvertToDstarStruct(&DstarHeader);
    
    Buffer->Set(tag, sizeof(tag));
    Buffer->Append(Packet.GetStreamId());
    Buffer->Append((uint8)0x80);
    Buffer->Append((uint8 *)&DstarHeader, sizeof(struct dstar_header));
    
    return true;
}

bool CG3Protocol::EncodeDvFramePacket(const CDvFramePacket &Packet, CBuffer *Buffer) const
{
    uint8 tag[] = { 'D','S','V','T',0x20,0x00,0x00,0x00,0x20,0x00,0x01,0x02 };
    
    Buffer->Set(tag, sizeof(tag));
    Buffer->Append(Packet.GetStreamId());
    Buffer->Append((uint8)(Packet.GetPacketId() % 21));
    Buffer->Append((uint8 *)Packet.GetAmbe(), AMBE_SIZE);
    Buffer->Append((uint8 *)Packet.GetDvData(), DVDATA_SIZE);
    
    return true;
    
}

bool CG3Protocol::EncodeDvLastFramePacket(const CDvLastFramePacket &Packet, CBuffer *Buffer) const
{
    uint8 tag1[] = { 'D','S','V','T',0x20,0x00,0x00,0x00,0x20,0x00,0x01,0x02 };
    uint8 tag2[] = { 0x55,0xC8,0x7A,0x00,0x00,0x00,0x00,0x00,0x00,0x25,0x1A,0xC6 };
    
    Buffer->Set(tag1, sizeof(tag1));
    Buffer->Append(Packet.GetStreamId());
    Buffer->Append((uint8)((Packet.GetPacketId() % 21) | 0x40));
    Buffer->Append(tag2, sizeof(tag2));
    
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
// option helpers

char *CG3Protocol::TrimWhiteSpaces(char *str)
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


void CG3Protocol::NeedReload(void)
{
    struct stat fileStat;

    if (::stat(TERMINALOPTIONS_PATH, &fileStat) != -1)
    {
        if (m_LastModTime != fileStat.st_mtime)
        {
            ReadOptions();
        }
    }

    // iterate on clients
    CClients *clients = g_Reflector.GetClients();
    int index = -1;
    CClient *client = NULL;
    while ( (client = clients->FindNextClient(PROTOCOL_G3, &index)) != NULL )
    {
        char module = client->GetReflectorModule();
        if (!strchr(m_Modules.c_str(), module) && !strchr(m_Modules.c_str(), '*'))
        {
            clients->RemoveClient(client);
        }
    }
    g_Reflector.ReleaseClients();
}

void CG3Protocol::ReadOptions(void)
{
    char sz[256];
    int opts = 0;


    std::ifstream file(TERMINALOPTIONS_PATH);
    if (file.is_open())
    {
        m_GwAddress = 0u;
        m_Modules = "*";

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
                        if (::strncmp(szt, "address", 7) == 0)
                        {
                            in_addr addr = { .s_addr = inet_addr(szval) };
                            if (addr.s_addr)
                            {
                                std::cout << "G3 handler address set to " << inet_ntoa(addr) << std::endl;
                                m_GwAddress = addr.s_addr;
                                opts++;
                            }
                        }
                        else if (strncmp(szt, "modules", 7) == 0)
                        {
                            std::cout << "G3 handler module list set to " << szval << std::endl;
                            m_Modules = szval;
                            opts++;
                        }
                        else
                        {
                            // unknown option - ignore
                        }
                    }
                }
            }
        }
        std::cout << "G3 handler loaded " << opts << " options from file " << TERMINALOPTIONS_PATH << std::endl;
        file.close();

        struct stat fileStat;

        if (::stat(TERMINALOPTIONS_PATH, &fileStat) != -1)
        {
            m_LastModTime = fileStat.st_mtime;
        }
    }
}

