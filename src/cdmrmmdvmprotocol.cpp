//
//  cdmrmmdvmprotocol.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 104/03/2017.
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
#include <random>
#include "cdmrmmdvmclient.h"
#include "cdmrmmdvmprotocol.h"
#include "creflector.h"
#include "cgatekeeper.h"
#include "cbptc19696.h"
#include "crs129.h"
#include "cgolay2087.h"
#include "cqr1676.h"
#include "chamming.h"
#include "cutils.h"
#include "ccrc.h"


////////////////////////////////////////////////////////////////////////////////////////
// define

#define CMD_NONE        0
#define CMD_LINK        1
#define CMD_UNLINK      2

////////////////////////////////////////////////////////////////////////////////////////
// constants

static uint8 g_DmrSyncBSVoice[]    = { 0x07,0x55,0xFD,0x7D,0xF7,0x5F,0x70 };
static uint8 g_DmrSyncBSData[]     = { 0x0D,0xFF,0x57,0xD7,0x5D,0xF5,0xD0 };
static uint8 g_DmrSyncMSVoice[]    = { 0x07,0xF7,0xD5,0xDD,0x57,0xDF,0xD0 };
static uint8 g_DmrSyncMSData[]     = { 0x0D,0x5D,0x7F,0x77,0xFD,0x75,0x70 };


////////////////////////////////////////////////////////////////////////////////////////
// operation

bool CDmrmmdvmProtocol::Init(void)
{
    bool ok;
    
    // base class
    ok = CProtocol::Init();
    
    // update the reflector callsign
    //m_ReflectorCallsign.PatchCallsign(0, (const uint8 *)"DMR", 3);
    
    // create our socket
    ok &= m_Socket.Open(DMRMMDVM_PORT);
    
    // update time
    m_LastKeepaliveTime.Now();
    
    // authentication seed from hardware random source
    std::random_device rd;
    m_uiAuthSeed = (uint32)rd();
    
    // done
    return ok;
}



////////////////////////////////////////////////////////////////////////////////////////
// task

void CDmrmmdvmProtocol::Task(void)
{
    CBuffer             Buffer;
    CIp                 Ip;
    CCallsign           Callsign;
    int                 iRssi;
    uint8               Cmd;
    uint8               CallType;
    CDvHeaderPacket     *Header;
    CDvFramePacket      *Frames[3];
    CDvLastFramePacket  *LastFrame;
    
    // handle incoming packets
    if ( m_Socket.Receive(&Buffer, &Ip, 20) != -1 )
    {
        //Buffer.DebugDump(g_Reflector.m_DebugFile);
        // crack the packet
        if ( IsValidDvFramePacket(Buffer, Frames) )
        {
            //std::cout << "DMRmmdvm DV frame" << std::endl;
            
            for ( int i = 0; i < 3; i++ )
            {
                OnDvFramePacketIn(Frames[i], &Ip);
            }
        }
        else if ( IsValidDvHeaderPacket(Buffer, &Header, &Cmd, &CallType) )
        {
            //std::cout << "DMRmmdvm DV header:"  << std::endl <<  *Header << std::endl;
            //std::cout << "DMRmmdvm DV header"  << std::endl;
            
            // callsign muted?
            if ( g_GateKeeper.MayTransmit(Header->GetMyCallsign(), Ip, PROTOCOL_DMRMMDVM) )
            {
                // handle it
                OnDvHeaderPacketIn(Header, Ip, Cmd, CallType);
            }
            else
            {
                delete Header;
            }
        }
        else if ( IsValidDvLastFramePacket(Buffer, &LastFrame) )
        {
            //std::cout << "DMRmmdvm DV last frame"  << std::endl;
            
            OnDvLastFramePacketIn(LastFrame, &Ip);
        }
        else if ( IsValidConnectPacket(Buffer, &Callsign, Ip) )
        {
            std::cout << "DMRmmdvm connect packet from " << Callsign << " at " << Ip << std::endl;
            
            // callsign authorized?
            if ( g_GateKeeper.MayLink(Callsign, Ip, PROTOCOL_DMRMMDVM) )
            {
                // acknowledge the request
                EncodeConnectAckPacket(&Buffer, Callsign, m_uiAuthSeed);
                m_Socket.Send(Buffer, Ip);
            }
            else
            {
                // deny the request
                EncodeNackPacket(&Buffer, Callsign);
                m_Socket.Send(Buffer, Ip);
            }
            
        }
        else if ( IsValidAuthenticationPacket(Buffer, &Callsign, Ip) )
        {
            std::cout << "DMRmmdvm authentication packet from " << Callsign << " at " << Ip << std::endl;
            
            // callsign authorized?
            if ( g_GateKeeper.MayLink(Callsign, Ip, PROTOCOL_DMRMMDVM) )
            {
                // acknowledge the request
                EncodeAckPacket(&Buffer, Callsign);
                m_Socket.Send(Buffer, Ip);
                
                // add client if needed
                CClients *clients = g_Reflector.GetClients();
                CClient *client = clients->FindClient(Callsign, Ip, PROTOCOL_DMRMMDVM);
                // client already connected ?
                if ( client == NULL )
                {
                    std::cout << "DMRmmdvm login from " << Callsign << " at " << Ip << std::endl;
                    
                    // create the client
                    CDmrmmdvmClient *newclient = new CDmrmmdvmClient(Callsign, Ip);
                    
                    // and append
                    clients->AddClient(newclient);
                }
                else
                {
                    client->Alive();
                }
                // and done
                g_Reflector.ReleaseClients();
            }
            else
            {
                // deny the request
                EncodeNackPacket(&Buffer, Callsign);
                m_Socket.Send(Buffer, Ip);
            }
            
        }
        else if ( IsValidDisconnectPacket(Buffer, &Callsign) )
        {
            std::cout << "DMRmmdvm disconnect packet from " << Callsign << " at " << Ip << std::endl;
            
            // find client & remove it
            CClients *clients = g_Reflector.GetClients();
            CClient *client = clients->FindClient(Ip, PROTOCOL_DMRMMDVM);
            if ( client != NULL )
            {
                clients->RemoveClient(client);
            }
            g_Reflector.ReleaseClients();
        }
        else if ( IsValidConfigPacket(Buffer, &Callsign, Ip) )
        {
            std::cout << "DMRmmdvm configuration packet from " << Callsign << " at " << Ip << std::endl;
            
            // acknowledge the request
            EncodeAckPacket(&Buffer, Callsign);
            m_Socket.Send(Buffer, Ip);
        }
        else if ( IsValidKeepAlivePacket(Buffer, &Callsign) )
        {
            //std::cout << "DMRmmdvm keepalive packet from " << Callsign << " at " << Ip << std::endl;
            
            // find all clients with that callsign & ip and keep them alive
            CClients *clients = g_Reflector.GetClients();
            int index = -1;
            CClient *client = NULL;
            while ( (client = clients->FindNextClient(Callsign, Ip, PROTOCOL_DMRMMDVM, &index)) != NULL )
            {
                // acknowledge
                EncodeKeepAlivePacket(&Buffer, client);
                m_Socket.Send(Buffer, Ip);
                
                // and mark as alive
                client->Alive();
            }
            g_Reflector.ReleaseClients();
        }
        else if ( IsValidRssiPacket(Buffer, &Callsign, &iRssi) )
        {
            // std::cout << "DMRmmdvm RSSI packet from " << Callsign << " at " << Ip << std::endl
            
            // ignore...
        }
        else if ( IsValidOptionPacket(Buffer, &Callsign) )
        {
            std::cout << "DMRmmdvm options packet from " << Callsign << " at " << Ip << std::endl;
            
            // acknowledge the request
            EncodeAckPacket(&Buffer, Callsign);
            m_Socket.Send(Buffer, Ip);
        }
        else if ( Buffer.size() != 55 )
        {
            // Log unknown packet with details based on DMRplus IPSC Protocol spec
            std::cout << "DMRmmdvm unknown packet (" << Buffer.size() << " bytes) from " << Ip;

            // Try to identify packet type from signature (first 4-7 bytes are typically ASCII)
            if ( Buffer.size() >= 4 )
            {
                std::cout << " sig=[";
                int sigLen = MIN((int)Buffer.size(), 7);
                for ( int i = 0; i < sigLen; i++ )
                {
                    char c = Buffer.data()[i];
                    if ( c >= 0x20 && c <= 0x7e )
                        std::cout << c;
                    else
                        break;  // Stop at first non-printable
                }
                std::cout << "]";
            }

            // Provide more detail for known packet types that failed validation
            if ( Buffer.size() == 8 && ::memcmp(Buffer.data(), "RPTL", 4) == 0 )
            {
                // Login request (8 bytes: RPTL + 4-byte repeater ID)
                uint32_t rptId = (Buffer.data()[4] << 24) | (Buffer.data()[5] << 16) |
                                 (Buffer.data()[6] << 8) | Buffer.data()[7];
                std::cout << " (Login request, repeater ID " << rptId << ")";
            }
            else if ( Buffer.size() == 9 && ::memcmp(Buffer.data(), "RPTCL", 5) == 0 )
            {
                // Repeater close (9 bytes: RPTCL + 4-byte repeater ID)
                uint32_t rptId = (Buffer.data()[5] << 24) | (Buffer.data()[6] << 16) |
                                 (Buffer.data()[7] << 8) | Buffer.data()[8];
                std::cout << " (Repeater close, repeater ID " << rptId << ")";
            }
            else if ( Buffer.size() == 11 && ::memcmp(Buffer.data(), "RPTPONG", 7) == 0 )
            {
                // Pong response (11 bytes: RPTPONG + 4-byte repeater ID)
                uint32_t rptId = (Buffer.data()[7] << 24) | (Buffer.data()[8] << 16) |
                                 (Buffer.data()[9] << 8) | Buffer.data()[10];
                std::cout << " (Pong, repeater ID " << rptId << ")";
            }
            else if ( Buffer.size() == 40 && ::memcmp(Buffer.data(), "RPTK", 4) == 0 )
            {
                // Auth response (40 bytes: RPTK + 4-byte repeater ID + 32-byte SHA256)
                uint32_t rptId = (Buffer.data()[4] << 24) | (Buffer.data()[5] << 16) |
                                 (Buffer.data()[6] << 8) | Buffer.data()[7];
                std::cout << " (Auth response, repeater ID " << rptId << ")";
            }
            else if ( Buffer.size() == 302 && ::memcmp(Buffer.data(), "RPTC", 4) == 0 )
            {
                // Config packet (302 bytes: RPTC + config data)
                // Callsign is at bytes 4-11 (8 bytes, space-padded)
                char callsign[9];
                ::memcpy(callsign, Buffer.data() + 4, 8);
                callsign[8] = '\0';
                std::cout << " (Config from " << callsign << ")";
            }
            else if ( Buffer.size() == 53 && ::memcmp(Buffer.data(), "DMRD", 4) == 0 )
            {
                // Data packet that failed validation (53 bytes)
                std::cout << " (Data packet failed validation)";
            }
            else if ( Buffer.size() > 0 )
            {
                // Unknown packet - show first few bytes as hex
                std::cout << " hex=[";
                int showBytes = MIN((int)Buffer.size(), 8);
                for ( int i = 0; i < showBytes; i++ )
                {
                    std::cout << std::hex << std::setfill('0') << std::setw(2)
                              << (int)(uint8_t)Buffer.data()[i];
                    if ( i < showBytes - 1 ) std::cout << " ";
                }
                std::cout << std::dec << "]";
            }
            std::cout << std::endl;
        }
    }
    
    // handle end of streaming timeout
    CheckStreamsTimeout();
    
    // handle queue from reflector
    HandleQueue();
    
    
    // keep client alive
    if ( m_LastKeepaliveTime.DurationSinceNow() > DMRMMDVM_KEEPALIVE_PERIOD )
    {
        //
        HandleKeepalives();
        
        // update time
        m_LastKeepaliveTime.Now();
    }
    
}

////////////////////////////////////////////////////////////////////////////////////////
// streams helpers

bool CDmrmmdvmProtocol::OnDvHeaderPacketIn(CDvHeaderPacket *Header, const CIp &Ip, uint8 cmd, uint8 CallType)
{
    bool newstream = false;
    bool lastheard = false;

    // set default suffix if not already set
    if ( !Header->HasMySuffix() )
    {
        Header->SetMySuffix("DMR");
    }

    //
    CCallsign via(Header->GetRpt1Callsign());

    // find the stream (match on both StreamId and IP to avoid false matches)
    // save callsigns before any ownership transfer
    CCallsign myCallsign = Header->GetMyCallsign();
    CCallsign rpt2Callsign = Header->GetRpt2Callsign();
    CPacketStream *stream = GetStream(Header->GetStreamId(), &Ip);
    if ( stream == NULL )
    {
        // no stream open yet, open a new one
        // firstfind this client
        CClient *client = g_Reflector.GetClients()->FindClient(Ip, PROTOCOL_DMRMMDVM);
        if ( client != NULL )
        {
            // process cmd if any
            if ( !client->HasReflectorModule() )
            {
                // not linked yet
                if ( cmd == CMD_LINK )
                {
                    if ( g_Reflector.IsValidModule(Header->GetRpt2Module()) )
                    {
                        std::cout << "DMRmmdvm client " << client->GetCallsign() << " linking on module " << Header->GetRpt2Module() << std::endl;
                        // link
                        client->SetReflectorModule(Header->GetRpt2Module());
                   }
                    else
                    {
                        std::cout << "DMRMMDVM node " << via << " link attempt on non-existing module" << std::endl;
                    }
                }
                else if ( cmd == CMD_NONE )
                {
                    // unlinked client sent a TG that doesn't map to a module
                    std::cout << "DMRMMDVM unlinked client " << via << " sent invalid TG — ignored" << std::endl;
                }
            }
            else
            {
                // already linked
                if ( cmd == CMD_UNLINK )
                {
                    std::cout << "DMRmmdvm client " << client->GetCallsign() << " unlinking" << std::endl;
                    // unlink
                    client->SetReflectorModule(' ');
                }
                else
                {
                    // replace rpt2 module with currently linked module
                    Header->SetRpt2Module(client->GetReflectorModule());
                }
            }
            
            // re-save rpt2 callsign after module may have been updated
            rpt2Callsign = Header->GetRpt2Callsign();

            // and now, re-check module is valid && that it's not a private call
            if ( g_Reflector.IsValidModule(Header->GetRpt2Module()) && (CallType == DMR_GROUP_CALL) )
            {
                // yes, try to open the stream
                if ( (stream = g_Reflector.OpenStream(Header, client)) != NULL )
                {
                    m_Streams.push_back(stream);
                    newstream = true;
                    lastheard = true;
                    Header = NULL;
                }
                else if ( g_Reflector.TryLateEntry(Header, client) )
                {
                    Header = NULL;
                }
            }
        }
        else
        {
            lastheard = true;
        }

        // release
        g_Reflector.ReleaseClients();

        // update last heard
        if ( lastheard )
        {
            g_Reflector.GetUsers()->Hearing(myCallsign, via, rpt2Callsign);
            g_Reflector.ReleaseUsers();
        }

        // delete header if needed
        if ( Header != NULL )
        {
            delete Header;
        }
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
// client cache helpers

void CDmrmmdvmProtocol::RefreshClientCache(int iModId)
{
    if ( iModId < 0 || iModId >= NB_OF_MODULES )
        return;

    // Check freshness under cache lock, release before acquiring Clients lock
    {
        std::lock_guard<std::mutex> lock(m_ClientCache[iModId].m_Mutex);
        if ( m_ClientCache[iModId].m_bInitialized &&
             m_ClientCache[iModId].m_LastRefresh.DurationSinceNow() < DMRMMDVM_CLIENT_CACHE_REFRESH_INTERVAL )
        {
            return;  // Cache is still fresh
        }
    }

    // Scan clients WITHOUT holding cache lock
    char moduleId = 'A' + iModId;
    std::vector<CIp> freshIps;

    CClients *clients = g_Reflector.GetClients();
    int index = -1;
    CClient *client = NULL;
    while ( (client = clients->FindNextClient(PROTOCOL_DMRMMDVM, &index)) != NULL )
    {
        if ( !client->IsAMaster() && (client->GetReflectorModule() == moduleId) )
        {
            freshIps.push_back(client->GetIp());
        }
    }
    g_Reflector.ReleaseClients();

    // Write results under cache lock
    std::lock_guard<std::mutex> lock(m_ClientCache[iModId].m_Mutex);
    m_ClientCache[iModId].m_ClientIps.swap(freshIps);
    m_ClientCache[iModId].m_LastRefresh.Now();
    m_ClientCache[iModId].m_bInitialized = true;
}

void CDmrmmdvmProtocol::SendToModuleClients(int iModId, const CBuffer &buffer, uint16 streamId)
{
    if ( iModId < 0 || iModId >= NB_OF_MODULES )
        return;

    // Refresh cache if needed (acquires its own lock)
    RefreshClientCache(iModId);

    // Send to cached clients (cache mutex is TX-thread-only, safe to hold during sends)
    std::lock_guard<std::mutex> lock(m_ClientCache[iModId].m_Mutex);
    for ( const CIp &ip : m_ClientCache[iModId].m_ClientIps )
    {
        // skip the stream owner to prevent audio echo back to transmitter
        if ( m_StreamsCache[iModId].m_bHasOwner &&
             m_StreamsCache[iModId].m_uiOutboundStreamId == streamId &&
             m_StreamsCache[iModId].m_OwnerIp == ip )
        {
            continue;
        }
        m_Socket.SendVoice(buffer, ip);
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// queue helper

void CDmrmmdvmProtocol::HandleQueue(void)
{
    // Phase 1: Drain all packets from queue under lock, then release lock immediately
    std::vector<CPacket *> packets;
    packets.reserve(100);  // Pre-allocate for typical queue sizes

    m_Queue.Lock();
    while ( !m_Queue.empty() )
    {
        packets.push_back(m_Queue.front());
        m_Queue.pop();
    }
    m_Queue.Unlock();

    // Phase 2: Process packets without holding queue lock
    for ( size_t i = 0; i < packets.size(); i++ )
    {
        CPacket *packet = packets[i];

        // get our sender's id and validate bounds
        int iModId = g_Reflector.GetModuleIndex(packet->GetModuleId());
        if ( iModId < 0 || iModId >= NB_OF_MODULES )
        {
            // Invalid module - skip this packet
            delete packet;
            continue;
        }

        // encode
        CBuffer buffer;

        // check if it's header
        if ( packet->IsDvHeader() )
        {
            // update local stream cache
            // this relies on queue feeder setting valid module id
            m_StreamsCache[iModId].m_dvHeader = CDvHeaderPacket((const CDvHeaderPacket &)*packet);
            m_StreamsCache[iModId].m_uiSeqId = 0;
            m_StreamsCache[iModId].m_uiLastSubid = 0;
            EncodeEmbeddedLC(m_StreamsCache[iModId].m_embeddedLC, m_StreamsCache[iModId].m_dvHeader.GetMyCallsign().GetDmrid());
            m_StreamsCache[iModId].m_uiOutboundStreamId = packet->GetStreamId();
            // capture stream owner IP for send-time exclusion
            m_StreamsCache[iModId].m_bHasOwner = false;
            CPacketStream *stream = g_Reflector.GetStream(packet->GetModuleId());
            if ( stream != NULL )
            {
                stream->Lock();
                if ( stream->IsOpen() && stream->GetStreamId() == packet->GetStreamId() )
                {
                    const CIp *ownerIp = stream->GetOwnerIp();
                    if ( ownerIp != NULL )
                    {
                        m_StreamsCache[iModId].m_OwnerIp = *ownerIp;
                        m_StreamsCache[iModId].m_bHasOwner = true;
                    }
                }
                stream->Unlock();
            }

            // encode it
            EncodeDvHeaderPacket((const CDvHeaderPacket &)*packet, m_StreamsCache[iModId].m_uiSeqId, &buffer);
            m_StreamsCache[iModId].m_uiSeqId = 1;
        }
        // check if it's a last frame
        else if ( packet->IsLastPacket() )
        {
            // flush any incomplete triplet before sending terminator
            // pad missing frames with zeros (0x00) — the radio's squelch
            // masks the brief artifact at end of transmission
            if ( m_StreamsCache[iModId].m_uiLastSubid > 0 )
            {
                uint8 zeroAmbe[9];
                ::memset(zeroAmbe, 0, sizeof(zeroAmbe));

                if ( m_StreamsCache[iModId].m_uiLastSubid == 1 )
                {
                    // only frame0 cached — zero-pad frame1 and frame2
                    CDvFramePacket padFrame1(zeroAmbe, (uint8 *)"\x00\x00\x00\x00\x00\x00\x00",
                                             m_StreamsCache[iModId].m_uiOutboundStreamId, 0, 2);
                    CDvFramePacket padFrame2(zeroAmbe, (uint8 *)"\x00\x00\x00\x00\x00\x00\x00",
                                             m_StreamsCache[iModId].m_uiOutboundStreamId, 0, 3);
                    CBuffer flushBuf;
                    EncodeDvPacket(m_StreamsCache[iModId].m_dvHeader,
                                   m_StreamsCache[iModId].m_dvFrame0,
                                   padFrame1, padFrame2,
                                   m_StreamsCache[iModId].m_uiSeqId, &flushBuf);
                    m_StreamsCache[iModId].m_uiSeqId = (m_StreamsCache[iModId].m_uiSeqId + 1) & 0xFF;
                    if ( flushBuf.size() > 0 )
                        SendToModuleClients(iModId, flushBuf, packet->GetStreamId());
                }
                else if ( m_StreamsCache[iModId].m_uiLastSubid == 2 )
                {
                    // frame0 and frame1 cached — zero-pad frame2
                    CDvFramePacket padFrame2(zeroAmbe, (uint8 *)"\x00\x00\x00\x00\x00\x00\x00",
                                             m_StreamsCache[iModId].m_uiOutboundStreamId, 0, 3);
                    CBuffer flushBuf;
                    EncodeDvPacket(m_StreamsCache[iModId].m_dvHeader,
                                   m_StreamsCache[iModId].m_dvFrame0,
                                   m_StreamsCache[iModId].m_dvFrame1,
                                   padFrame2,
                                   m_StreamsCache[iModId].m_uiSeqId, &flushBuf);
                    m_StreamsCache[iModId].m_uiSeqId = (m_StreamsCache[iModId].m_uiSeqId + 1) & 0xFF;
                    if ( flushBuf.size() > 0 )
                        SendToModuleClients(iModId, flushBuf, packet->GetStreamId());
                }
                m_StreamsCache[iModId].m_uiLastSubid = 0;
            }

            // now send the terminator
            EncodeDvLastPacket(m_StreamsCache[iModId].m_dvHeader, m_StreamsCache[iModId].m_uiSeqId, &buffer);
            m_StreamsCache[iModId].m_uiSeqId = (m_StreamsCache[iModId].m_uiSeqId + 1) & 0xFF;
        }
        // otherwise, just a regular DV frame
        else
        {
            // update local stream cache or send triplet when needed
            switch ( packet->GetDmrPacketSubid() )
            {
                case 1:
                    m_StreamsCache[iModId].m_dvFrame0 = CDvFramePacket((const CDvFramePacket &)*packet);
                    m_StreamsCache[iModId].m_uiLastSubid = 1;
                    break;
                case 2:
                    m_StreamsCache[iModId].m_dvFrame1 = CDvFramePacket((const CDvFramePacket &)*packet);
                    m_StreamsCache[iModId].m_uiLastSubid = 2;
                    break;
                case 3:
                    EncodeDvPacket(
                                   m_StreamsCache[iModId].m_dvHeader,
                                   m_StreamsCache[iModId].m_dvFrame0,
                                   m_StreamsCache[iModId].m_dvFrame1,
                                   (const CDvFramePacket &)*packet,
                                   m_StreamsCache[iModId].m_uiSeqId,
                                   &buffer);
                    m_StreamsCache[iModId].m_uiSeqId = (m_StreamsCache[iModId].m_uiSeqId + 1) & 0xFF;
                    m_StreamsCache[iModId].m_uiLastSubid = 0;
                    break;
                default:
                    break;
            }
        }

        // send it
        if ( buffer.size() > 0 )
        {
            SendToModuleClients(iModId, buffer, packet->GetStreamId());
        }

        // done
        delete packet;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// keepalive helpers

void CDmrmmdvmProtocol::HandleKeepalives(void)
{
    // DMRhomebrew protocol keepalive request is client tasks
    // here, just check that all clients are still alive
    // and disconnect them if not
    
    // iterate on clients
    CClients *clients = g_Reflector.GetClients();
    int index = -1;
    CClient *client = NULL;
    std::vector<CClient *> toRemove;
    while ( (client = clients->FindNextClient(PROTOCOL_DMRMMDVM, &index)) != NULL )
    {
        // is this client busy ?
        if ( client->IsAMaster() )
        {
            // yes, just tickle it
            client->Alive();
        }
        // check it's still with us
        else if ( !client->IsAlive() )
        {
            // no, disconnect
            CBuffer disconnect;
            EncodeClosePacket(&disconnect, client);
            m_Socket.Send(disconnect, client->GetIp());

            // collect for removal after loop
            std::cout << "DMRmmdvm client " << client->GetCallsign() << " keepalive timeout" << std::endl;
            toRemove.push_back(client);
        }

    }
    for ( size_t i = 0; i < toRemove.size(); i++ )
    {
        clients->RemoveClient(toRemove[i]);
    }
    g_Reflector.ReleaseClients();
}

////////////////////////////////////////////////////////////////////////////////////////
// packet decoding helpers

bool CDmrmmdvmProtocol::IsValidKeepAlivePacket(const CBuffer &Buffer, CCallsign *callsign)
{
    uint8 tag[] = { 'R','P','T','P','I','N','G' };
    
    bool valid = false;
    if ( (Buffer.size() == 11) && (Buffer.Compare(tag, sizeof(tag)) == 0) )
    {
        uint32 uiRptrId = MAKEDWORD(MAKEWORD(Buffer.data()[10],Buffer.data()[9]),MAKEWORD(Buffer.data()[8],Buffer.data()[7]));
        callsign->SetDmrid(uiRptrId, true);
        callsign->SetModule(MMDVM_MODULE_ID);
        valid = callsign->IsValid();
    }
    return valid;
}

bool CDmrmmdvmProtocol::IsValidConnectPacket(const CBuffer &Buffer, CCallsign *callsign, const CIp &Ip)
{
    uint8 tag[] = { 'R','P','T','L' };
    
    bool valid = false;
    if ( (Buffer.size() == 8) && (Buffer.Compare(tag, sizeof(tag)) == 0) )
    {
        uint32 uiRptrId = MAKEDWORD(MAKEWORD(Buffer.data()[7],Buffer.data()[6]),MAKEWORD(Buffer.data()[5],Buffer.data()[4]));
        callsign->SetDmrid(uiRptrId, true);
        callsign->SetModule(MMDVM_MODULE_ID);
        valid = callsign->IsValid();
        if ( !valid)
        {
            std::cout << "DMRmmdvm connect packet from IP address " << Ip << " / unrecognized id " << (int)callsign->GetDmrid()  << std::endl;
        }
    }
    return valid;
}

bool CDmrmmdvmProtocol::IsValidAuthenticationPacket(const CBuffer &Buffer, CCallsign *callsign, const CIp &Ip)
{
    uint8 tag[] = { 'R','P','T','K' };
    
    bool valid = false;
    if ( (Buffer.size() == 40) && (Buffer.Compare(tag, sizeof(tag)) == 0) )
    {
        uint32 uiRptrId = MAKEDWORD(MAKEWORD(Buffer.data()[7],Buffer.data()[6]),MAKEWORD(Buffer.data()[5],Buffer.data()[4]));
        callsign->SetDmrid(uiRptrId, true);
        callsign->SetModule(MMDVM_MODULE_ID);
        valid = callsign->IsValid();
        if ( !valid)
        {
            std::cout << "DMRmmdvm authnetication packet from IP address " << Ip << " / unrecognized id " << (int)callsign->GetDmrid()  << std::endl;
        }

    }
    return valid;
}

bool CDmrmmdvmProtocol::IsValidDisconnectPacket(const CBuffer &Buffer, CCallsign *callsign)
{
    uint8 tag[] = { 'R','P','T','C','L' };
    
    bool valid = false;
    if ( (Buffer.size() == 13) && (Buffer.Compare(tag, sizeof(tag)) == 0) )
    {
        uint32 uiRptrId = MAKEDWORD(MAKEWORD(Buffer.data()[7],Buffer.data()[6]),MAKEWORD(Buffer.data()[5],Buffer.data()[4]));
        callsign->SetDmrid(uiRptrId, true);
        callsign->SetModule(MMDVM_MODULE_ID);
        valid = callsign->IsValid();
    }
    return valid;
}

bool CDmrmmdvmProtocol::IsValidConfigPacket(const CBuffer &Buffer, CCallsign *callsign, const CIp &Ip)
{
    uint8 tag[] = { 'R','P','T','C' };
    
    bool valid = false;
    if ( (Buffer.size() == 302) && (Buffer.Compare(tag, sizeof(tag)) == 0) )
    {
        uint32 uiRptrId = MAKEDWORD(MAKEWORD(Buffer.data()[7],Buffer.data()[6]),MAKEWORD(Buffer.data()[5],Buffer.data()[4]));
        callsign->SetDmrid(uiRptrId, true);
        callsign->SetModule(MMDVM_MODULE_ID);
        valid = callsign->IsValid();
        if ( !valid)
        {
            std::cout << "DMRmmdvm config packet from IP address " << Ip << " / unrecognized id " << (int)callsign->GetDmrid()  << std::endl;
        }

    }
    return valid;
}

bool CDmrmmdvmProtocol::IsValidOptionPacket(const CBuffer &Buffer, CCallsign *callsign)
{
    uint8 tag[] = { 'R','P','T','O' };
    
    bool valid = false;
    if ( (Buffer.size() >= 8) && (Buffer.size() <= 512) && (Buffer.Compare(tag, sizeof(tag)) == 0) )
    {
        uint32 uiRptrId = MAKEDWORD(MAKEWORD(Buffer.data()[7],Buffer.data()[6]),MAKEWORD(Buffer.data()[5],Buffer.data()[4]));
        callsign->SetDmrid(uiRptrId, true);
        callsign->SetModule(MMDVM_MODULE_ID);
        valid = callsign->IsValid();
    }
    return valid;
}

bool CDmrmmdvmProtocol::IsValidRssiPacket(const CBuffer &Buffer, CCallsign *callsign, int *rssi)
{
    uint8 tag[] = { 'R','P','T','I','N','T','R' };
    
    bool valid = false;
    if ( (Buffer.size() == 17) && (Buffer.Compare(tag, sizeof(tag)) == 0) )
    {
        // ignore rest of it, as not used
        // dmrid is asci hex on 8 bytes
        // rssi is ascii :x-xx
        valid = true;
    }
    return valid;
}

bool CDmrmmdvmProtocol::IsValidDvHeaderPacket(const CBuffer &Buffer, CDvHeaderPacket **header, uint8 *cmd, uint8 *CallType)
{
    uint8 tag[] = { 'D','M','R','D' };
    
    bool valid = false;
    *header = NULL;
    *cmd = CMD_NONE;
    
    if ( (Buffer.size() == 55) && (Buffer.Compare(tag, sizeof(tag)) == 0) )
    {
        // frame details
        uint8 uiFrameType = (Buffer.data()[15] & 0x30) >> 4;
        uint8 uiSlot = (Buffer.data()[15] & 0x80) ? DMR_SLOT2 : DMR_SLOT1;
        uint8 uiCallType = (Buffer.data()[15] & 0x40) ? DMR_PRIVATE_CALL : DMR_GROUP_CALL;
        uint8 uiSlotType = Buffer.data()[15] & 0x0F;
        //std::cout << (int)uiSlot << std::endl;
        if ( (uiFrameType == DMRMMDVM_FRAMETYPE_DATASYNC) &&
             (uiSlot == DMRMMDVM_REFLECTOR_SLOT) &&
             (uiSlotType == MMDVM_SLOTTYPE_HEADER) )
        {
            // extract sync
            uint8 dmrsync[7];
            dmrsync[0] = Buffer.data()[33] & 0x0F;
            ::memcpy(&dmrsync[1], &Buffer.data()[34], 5);
            dmrsync[6] = Buffer.data()[39] & 0xF0;
            // and check
            if ( (::memcmp(dmrsync, g_DmrSyncMSData, sizeof(dmrsync)) == 0) ||
                 (::memcmp(dmrsync, g_DmrSyncBSData, sizeof(dmrsync)) == 0))
            {
                // get payload
                //CBPTC19696 bptc;
                //uint8 lcdata[12];
                //bptc.decode(&(Buffer.data()[20]), lcdata);
                
                // crack DMR header
                uint32 uiSrcId = MAKEDWORD(MAKEWORD(Buffer.data()[7],Buffer.data()[6]),MAKEWORD(Buffer.data()[5],0));
                uint32 uiDstId = MAKEDWORD(MAKEWORD(Buffer.data()[10],Buffer.data()[9]),MAKEWORD(Buffer.data()[8],0));
                uint32 uiRptrId = MAKEDWORD(MAKEWORD(Buffer.data()[14],Buffer.data()[13]),MAKEWORD(Buffer.data()[12],Buffer.data()[11]));
                //uint8 uiVoiceSeq = (Buffer.data()[15] & 0x0F);
                uint32 uiStreamId32; ::memcpy(&uiStreamId32, &Buffer.data()[16], sizeof(uint32));
                // fold-XOR both halves to mix all 32 bits into 16
                uint16 uiStreamId = (uint16)(uiStreamId32 ^ (uiStreamId32 >> 16));
                if ( uiStreamId == 0 ) uiStreamId = 1;  // avoid sentinel

                // call type
                *CallType = uiCallType;
                
                // link/unlink command ?
                if ( uiDstId == 4000 )
                {
                    *cmd = CMD_UNLINK;
                }
                else if ( DmrDstIdToModule(uiDstId) != ' ' )
                {
                    *cmd = CMD_LINK;
                }
                else
                {
                    *cmd = CMD_NONE;
                }
                
                // build DVHeader
                // RPT1 uses caller's callsign + 'B' — D-Star hotspot convention.
                // See cnxdnprotocol.cpp for the rationale.
                CCallsign csMY = CCallsign("", uiSrcId);
                CCallsign rpt1 = CCallsign("", uiSrcId);
                rpt1.SetModule('B');
                CCallsign rpt2 = m_ReflectorCallsign;
                rpt2.SetModule(DmrDstIdToModule(uiDstId));
                
                // and packet
                *header = new CDvHeaderPacket(uiSrcId, CCallsign("CQCQCQ"), rpt1, rpt2, uiStreamId, 0, 0);
                valid = (*header)->IsValid();
                if ( !valid )
                {
                    delete *header;
                    *header = NULL;
                }
            }
        }
    }
    // done
    return valid;
}

bool CDmrmmdvmProtocol::IsValidDvFramePacket(const CBuffer &Buffer, CDvFramePacket **frames)
{
    uint8 tag[] = { 'D','M','R','D' };
    
    bool valid = false;
    frames[0] = NULL;
    frames[1] = NULL;
    frames[2] = NULL;
    
    if ( (Buffer.size() == 55) && (Buffer.Compare(tag, sizeof(tag)) == 0) )
    {
        // frame details
        uint8 uiFrameType = (Buffer.data()[15] & 0x30) >> 4;
        uint8 uiSlot = (Buffer.data()[15] & 0x80) ? DMR_SLOT2 : DMR_SLOT1;
        uint8 uiCallType = (Buffer.data()[15] & 0x40) ? DMR_PRIVATE_CALL : DMR_GROUP_CALL;
        if ( ((uiFrameType == DMRMMDVM_FRAMETYPE_VOICE) || (uiFrameType == DMRMMDVM_FRAMETYPE_VOICESYNC)) &&
              (uiSlot == DMRMMDVM_REFLECTOR_SLOT) && (uiCallType == DMR_GROUP_CALL) )
        {
            // crack DMR header
            //uint8 uiSeqId = Buffer.data()[4];
            //uint32 uiRptrId = MAKEDWORD(MAKEWORD(Buffer.data()[14],Buffer.data()[13]),MAKEWORD(Buffer.data()[12],Buffer.data()[11]));
            uint8 uiVoiceSeq = (Buffer.data()[15] & 0x0F);
            uint32 uiStreamId32; ::memcpy(&uiStreamId32, &Buffer.data()[16], sizeof(uint32));
            uint16 uiStreamId = (uint16)(uiStreamId32 ^ (uiStreamId32 >> 16));
            if ( uiStreamId == 0 ) uiStreamId = 1;

            // crack payload
            uint8 dmrframe[33];
            uint8 dmr3ambe[27];
            uint8 dmrambe[9];
            uint8 dmrsync[7];
            // get the 33 bytes ambe
            memcpy(dmrframe, &(Buffer.data()[20]), 33);
            // extract the 3 ambe frames
            memcpy(dmr3ambe, dmrframe, 14);
            dmr3ambe[13] &= 0xF0;
            dmr3ambe[13] |= (dmrframe[19] & 0x0F);
            memcpy(&dmr3ambe[14], &dmrframe[20], 13);
            // extract sync
            dmrsync[0] = dmrframe[13] & 0x0F;
            ::memcpy(&dmrsync[1], &dmrframe[14], 5);
            dmrsync[6] = dmrframe[19] & 0xF0;
            
            // debug
            //CBuffer dump;
            //dump.Set(dmrsync, 6);
            //dump.DebugDump(g_Reflector.m_DebugFile);
            
            // and create 3 dv frames
            // frame1
            memcpy(dmrambe, &dmr3ambe[0], 9);
            frames[0] = new CDvFramePacket(dmrambe, dmrsync, uiStreamId, uiVoiceSeq, 1);
            
            // frame2
            memcpy(dmrambe, &dmr3ambe[9], 9);
            frames[1] = new CDvFramePacket(dmrambe, dmrsync, uiStreamId, uiVoiceSeq, 2);
            
            // frame3
            memcpy(dmrambe, &dmr3ambe[18], 9);
            frames[2] = new CDvFramePacket(dmrambe, dmrsync, uiStreamId, uiVoiceSeq, 3);
            
            // check
            valid = true;
        }
    }
    // done
    return valid;
}

bool CDmrmmdvmProtocol::IsValidDvLastFramePacket(const CBuffer &Buffer, CDvLastFramePacket **frame)
{
    uint8 tag[] = { 'D','M','R','D' };
    
    bool valid = false;
    *frame = NULL;
    
    if ( (Buffer.size() == 55) && (Buffer.Compare(tag, sizeof(tag)) == 0) )
    {
        // frame details
        uint8 uiFrameType = (Buffer.data()[15] & 0x30) >> 4;
        uint8 uiSlot = (Buffer.data()[15] & 0x80) ? DMR_SLOT2 : DMR_SLOT1;
        //uint8 uiCallType = (Buffer.data()[15] & 0x40) ? DMR_PRIVATE_CALL : DMR_GROUP_CALL;
        uint8 uiSlotType = Buffer.data()[15] & 0x0F;
        //std::cout << (int)uiSlot << std::endl;
        if ( (uiFrameType == DMRMMDVM_FRAMETYPE_DATASYNC) &&
            (uiSlot == DMRMMDVM_REFLECTOR_SLOT) &&
            (uiSlotType == MMDVM_SLOTTYPE_TERMINATOR) )
        {
            // extract sync
            uint8 dmrsync[7];
            dmrsync[0] = Buffer.data()[33] & 0x0F;
            ::memcpy(&dmrsync[1], &Buffer.data()[34], 5);
            dmrsync[6] = Buffer.data()[39] & 0xF0;
            // and check
            if ( (::memcmp(dmrsync, g_DmrSyncMSData, sizeof(dmrsync)) == 0) ||
                (::memcmp(dmrsync, g_DmrSyncBSData, sizeof(dmrsync)) == 0))
            {
                // get payload
                //CBPTC19696 bptc;
                //uint8 lcdata[12];
                //bptc.decode(&(Buffer.data()[20]), lcdata);
                
                // crack DMR header
                //uint32 uiSrcId = MAKEDWORD(MAKEWORD(Buffer.data()[7],Buffer.data()[6]),MAKEWORD(Buffer.data()[5],0));
                //uint32 uiDstId = MAKEDWORD(MAKEWORD(Buffer.data()[10],Buffer.data()[9]),MAKEWORD(Buffer.data()[8],0));
                //uint32 uiRptrId = MAKEDWORD(MAKEWORD(Buffer.data()[14],Buffer.data()[13]),MAKEWORD(Buffer.data()[12],Buffer.data()[11]));
                //uint8 uiVoiceSeq = (Buffer.data()[15] & 0x0F);
                uint32 uiStreamId32; ::memcpy(&uiStreamId32, &Buffer.data()[16], sizeof(uint32));
                uint16 uiStreamId = (uint16)(uiStreamId32 ^ (uiStreamId32 >> 16));
                if ( uiStreamId == 0 ) uiStreamId = 1;

                // dummy ambe
                uint8 ambe[9];
                ::memset(ambe, 0, sizeof(ambe));
                
                
                // and packet
                *frame = new CDvLastFramePacket(ambe, dmrsync, uiStreamId, 0, 0);
                
                // check
                valid = true;
            }
        }
    }
    // done
    return valid;
}

////////////////////////////////////////////////////////////////////////////////////////
// packet encoding helpers

void CDmrmmdvmProtocol::EncodeKeepAlivePacket(CBuffer *Buffer, CClient *Client)
{
    uint8 tag[] = { 'M','S','T','P','O','N','G' };
    
    Buffer->Set(tag, sizeof(tag));
    uint32 uiDmrId = Client->GetCallsign().GetDmrid();
    Buffer->Append((uint8 *)&uiDmrId, 4);
}

void CDmrmmdvmProtocol::EncodeAckPacket(CBuffer *Buffer, const CCallsign &Callsign)
{
    uint8 tag[] = { 'R','P','T','A','C','K' };
    
    Buffer->Set(tag, sizeof(tag));
}

void CDmrmmdvmProtocol::EncodeConnectAckPacket(CBuffer *Buffer, const CCallsign &Callsign, uint32 AuthSeed)
{
    uint8 tag[] = { 'R','P','T','A','C','K' };
    
    Buffer->Set(tag, sizeof(tag));
    Buffer->Append(AuthSeed);
}

void CDmrmmdvmProtocol::EncodeNackPacket(CBuffer *Buffer, const CCallsign &Callsign)
{
    uint8 tag[] = { 'M','S','T','N','A','K' };
    
    Buffer->Set(tag, sizeof(tag));
}

void CDmrmmdvmProtocol::EncodeClosePacket(CBuffer *Buffer, CClient *Client)
{
    uint8 tag[] = { 'M','S','T','C','L' };
    
    Buffer->Set(tag, sizeof(tag));
}


bool CDmrmmdvmProtocol::EncodeDvHeaderPacket(const CDvHeaderPacket &Packet, uint8 seqid, CBuffer *Buffer) const
{
    uint8 tag[] = { 'D','M','R','D' };
    
    Buffer->Set(tag, sizeof(tag));
    
    // DMR header
    // uiSeqId
    Buffer->Append((uint8)seqid);
    // uiSrcId
    uint32 uiSrcId = Packet.GetMyCallsign().GetDmrid();
    AppendDmrIdToBuffer(Buffer, uiSrcId);
    // uiDstId = TG9
    uint32 uiDstId = 9; // ModuleToDmrDestId(Packet.GetRpt2Module());
    AppendDmrIdToBuffer(Buffer, uiDstId);
    // uiRptrId
    uint32 uiRptrId = Packet.GetRpt1Callsign().GetDmrid();
    AppendDmrRptrIdToBuffer(Buffer, uiRptrId);
    // uiBitField
    uint8 uiBitField =
        (DMRMMDVM_FRAMETYPE_DATASYNC << 4) |
        ((DMRMMDVM_REFLECTOR_SLOT == DMR_SLOT2) ? 0x80 : 0x00) |
        MMDVM_SLOTTYPE_HEADER;
    Buffer->Append((uint8)uiBitField);
    // uiStreamId
    uint32 uiStreamId = Packet.GetStreamId();
    Buffer->Append((uint32)uiStreamId);
    
    // Payload
    AppendVoiceLCToBuffer(Buffer, uiSrcId);
    
    // BER
    Buffer->Append((uint8)0);
    
    // RSSI
    Buffer->Append((uint8)0);
    
    // done
    return true;
}

void CDmrmmdvmProtocol::EncodeDvPacket(
      const CDvHeaderPacket &Header,
      const CDvFramePacket &DvFrame0, const CDvFramePacket &DvFrame1, const CDvFramePacket &DvFrame2,
      uint8 seqid, CBuffer *Buffer) const
{
    uint8 tag[] = { 'D','M','R','D' };
    Buffer->Set(tag, sizeof(tag));
    
    // DMR header
    // uiSeqId
    Buffer->Append((uint8)seqid);
    // uiSrcId
    uint32 uiSrcId = Header.GetMyCallsign().GetDmrid();
    AppendDmrIdToBuffer(Buffer, uiSrcId);
    // uiDstId = TG9
    uint32 uiDstId = 9; // ModuleToDmrDestId(Header.GetRpt2Module());
    AppendDmrIdToBuffer(Buffer, uiDstId);
    // uiRptrId
    uint32 uiRptrId = Header.GetRpt1Callsign().GetDmrid();
    AppendDmrRptrIdToBuffer(Buffer, uiRptrId);
    // uiBitField
    uint8 uiBitField =
        ((DMRMMDVM_REFLECTOR_SLOT == DMR_SLOT2) ? 0x80 : 0x00);
    if ( DvFrame0.GetDmrPacketId() == 0 )
    {
        uiBitField |= (DMRMMDVM_FRAMETYPE_VOICESYNC << 4);
    }
    else
    {
        uiBitField |= (DMRMMDVM_FRAMETYPE_VOICE << 4);
    }
    uiBitField |= (DvFrame0.GetDmrPacketId() & 0x0F);
    Buffer->Append((uint8)uiBitField);
    
    // uiStreamId
    uint32 uiStreamId = Header.GetStreamId();
    Buffer->Append((uint32)uiStreamId);
    
    // Payload
    // frame0
    Buffer->ReplaceAt(20, DvFrame0.GetAmbePlus(), 9);
    // 1/2 frame1
    Buffer->ReplaceAt(29, DvFrame1.GetAmbePlus(), 5);
    Buffer->ReplaceAt(33, (uint8)(Buffer->at(33) & 0xF0));
    // 1/2 frame1
    Buffer->ReplaceAt(39, DvFrame1.GetAmbePlus()+4, 5);
    Buffer->ReplaceAt(39, (uint8)(Buffer->at(39) & 0x0F));
    // frame2
    Buffer->ReplaceAt(44, DvFrame2.GetAmbePlus(), 9);
    
    int iModId = g_Reflector.GetModuleIndex(Header.GetModuleId());
    
    // sync or embedded signaling
    ReplaceEMBInBuffer(Buffer, DvFrame0.GetDmrPacketId(), m_StreamsCache[iModId].m_embeddedLC);

    // debug
    //CBuffer dump;
    //dump.Set(&(Buffer->data()[33]), 7);
    //dump.DebugDump(g_Reflector.m_DebugFile);

    // BER
    Buffer->Append((uint8)0);
    
    // RSSI
    Buffer->Append((uint8)0);    
}


void CDmrmmdvmProtocol::EncodeDvLastPacket(const CDvHeaderPacket &Packet, uint8 seqid, CBuffer *Buffer) const
{
    uint8 tag[] = { 'D','M','R','D' };
    
    Buffer->Set(tag, sizeof(tag));
    
    // DMR header
    // uiSeqId
    Buffer->Append((uint8)seqid);
    // uiSrcId
    uint32 uiSrcId = Packet.GetMyCallsign().GetDmrid();
    AppendDmrIdToBuffer(Buffer, uiSrcId);
    // uiDstId
    uint32 uiDstId = 9; //ModuleToDmrDestId(Packet.GetRpt2Module());
    AppendDmrIdToBuffer(Buffer, uiDstId);
    // uiRptrId
    uint32 uiRptrId = Packet.GetRpt1Callsign().GetDmrid();
    AppendDmrRptrIdToBuffer(Buffer, uiRptrId);
    // uiBitField
    uint8 uiBitField =
        (DMRMMDVM_FRAMETYPE_DATASYNC << 4) |
        ((DMRMMDVM_REFLECTOR_SLOT == DMR_SLOT2) ? 0x80 : 0x00) |
        MMDVM_SLOTTYPE_TERMINATOR;
    Buffer->Append((uint8)uiBitField);
    // uiStreamId
    uint32 uiStreamId = Packet.GetStreamId();
    Buffer->Append((uint32)uiStreamId);
    
    // Payload
    AppendTerminatorLCToBuffer(Buffer, uiSrcId);
    
    // BER
    Buffer->Append((uint8)0);
    
    // RSSI
    Buffer->Append((uint8)0);
}


////////////////////////////////////////////////////////////////////////////////////////
// DestId to Module helper

char CDmrmmdvmProtocol::DmrDstIdToModule(uint32 tg) const
{
    // is it a 4xxx ?
    if ( (tg >= 4001) && (tg <= (4000 + NB_OF_MODULES)) )
    {
        return ((char)(tg - 4001) + 'A');
    }
    return ' ';
}

uint32 CDmrmmdvmProtocol::ModuleToDmrDestId(char m) const
{
    return (uint32)(m - 'A')+4001;
}

////////////////////////////////////////////////////////////////////////////////////////
// Buffer & LC helpers

void CDmrmmdvmProtocol::AppendVoiceLCToBuffer(CBuffer *buffer, uint32 uiSrcId) const
{
    uint8 payload[33];
    
    // fill payload
    CBPTC19696 bptc;
    ::memset(payload, 0, sizeof(payload));
    // LC data
    uint8 lc[12];
    {
        ::memset(lc, 0, sizeof(lc));
        // uiDstId = TG9
        lc[5] = 9;
        // uiSrcId
        lc[6] = (uint8)LOBYTE(HIWORD(uiSrcId));
        lc[7] = (uint8)HIBYTE(LOWORD(uiSrcId));
        lc[8] = (uint8)LOBYTE(LOWORD(uiSrcId));
        // parity
        uint8 parity[4];
        CRS129::encode(lc, 9, parity);
        lc[9]  = parity[2] ^ DMR_VOICE_LC_HEADER_CRC_MASK;
        lc[10] = parity[1] ^ DMR_VOICE_LC_HEADER_CRC_MASK;
        lc[11] = parity[0] ^ DMR_VOICE_LC_HEADER_CRC_MASK;
    }
    // sync
    ::memcpy(payload+13, g_DmrSyncBSData, sizeof(g_DmrSyncBSData));
    // slot type
    {
        // slot type
        uint8 slottype[3];
        ::memset(slottype, 0, sizeof(slottype));
        slottype[0]  = (DMRMMDVM_REFLECTOR_COLOUR << 4) & 0xF0;
        slottype[0] |= (DMR_DT_VOICE_LC_HEADER  << 0) & 0x0FU;
        CGolay2087::encode(slottype);
        payload[12U] = (payload[12U] & 0xC0U) | ((slottype[0U] >> 2) & 0x3FU);
        payload[13U] = (payload[13U] & 0x0FU) | ((slottype[0U] << 6) & 0xC0U) | ((slottype[1U] >> 2) & 0x30U);
        payload[19U] = (payload[19U] & 0xF0U) | ((slottype[1U] >> 2) & 0x0FU);
        payload[20U] = (payload[20U] & 0x03U) | ((slottype[1U] << 6) & 0xC0U) | ((slottype[2U] >> 2) & 0x3CU);
        
    }
    // and encode
    bptc.encode(lc, payload);
    
    // and append
    buffer->Append(payload, sizeof(payload));
}

void CDmrmmdvmProtocol::AppendTerminatorLCToBuffer(CBuffer *buffer, uint32 uiSrcId) const
{
    uint8 payload[33];
    
    // fill payload
    CBPTC19696 bptc;
    ::memset(payload, 0, sizeof(payload));
    // LC data
    uint8 lc[12];
    {
        ::memset(lc, 0, sizeof(lc));
        // uiDstId = TG9
        lc[5] = 9;
        // uiSrcId
        lc[6] = (uint8)LOBYTE(HIWORD(uiSrcId));
        lc[7] = (uint8)HIBYTE(LOWORD(uiSrcId));
        lc[8] = (uint8)LOBYTE(LOWORD(uiSrcId));
        // parity
        uint8 parity[4];
        CRS129::encode(lc, 9, parity);
        lc[9]  = parity[2] ^ DMR_TERMINATOR_WITH_LC_CRC_MASK;
        lc[10] = parity[1] ^ DMR_TERMINATOR_WITH_LC_CRC_MASK;
        lc[11] = parity[0] ^ DMR_TERMINATOR_WITH_LC_CRC_MASK;
    }
    // sync
    ::memcpy(payload+13, g_DmrSyncBSData, sizeof(g_DmrSyncBSData));
    // slot type
    {
        // slot type
        uint8 slottype[3];
        ::memset(slottype, 0, sizeof(slottype));
        slottype[0]  = (DMRMMDVM_REFLECTOR_COLOUR << 4) & 0xF0;
        slottype[0] |= (DMR_DT_TERMINATOR_WITH_LC  << 0) & 0x0FU;
        CGolay2087::encode(slottype);
        payload[12U] = (payload[12U] & 0xC0U) | ((slottype[0U] >> 2) & 0x3FU);
        payload[13U] = (payload[13U] & 0x0FU) | ((slottype[0U] << 6) & 0xC0U) | ((slottype[1U] >> 2) & 0x30U);
        payload[19U] = (payload[19U] & 0xF0U) | ((slottype[1U] >> 2) & 0x0FU);
        payload[20U] = (payload[20U] & 0x03U) | ((slottype[1U] << 6) & 0xC0U) | ((slottype[2U] >> 2) & 0x3CU);
    }
    // and encode
    bptc.encode(lc, payload);
    
    // and append
    buffer->Append(payload, sizeof(payload));
}

void CDmrmmdvmProtocol::ReplaceEMBInBuffer(CBuffer *buffer, uint8 uiDmrPacketId, const uint8 *embeddedLC) const
{
    // voice packet A ?
    if ( uiDmrPacketId == 0 )
    {
        // sync
        buffer->ReplaceAt(33, (uint8)(buffer->at(33) | (g_DmrSyncBSVoice[0] & 0x0F)));
        buffer->ReplaceAt(34, g_DmrSyncBSVoice+1, 5);
        buffer->ReplaceAt(39, (uint8)(buffer->at(39) | (g_DmrSyncBSVoice[6] & 0xF0)));
    }
    // voice packet B,C,D,E ?
    else if ( (uiDmrPacketId >= 1) && (uiDmrPacketId <= 4 ) )
    {
        // EMB LC
        uint8 lcss;
        if (uiDmrPacketId == 1) {
          lcss = 1;
        } else if (uiDmrPacketId == 4) {
          lcss = 2;
        } else {
          lcss = 3;
        }

        const uint8 *eLC = embeddedLC + (uiDmrPacketId-1)*4; // point to the fragment to be sent

        uint8 emb[2];
        emb[0]  = (DMRMMDVM_REFLECTOR_COLOUR << 4) & 0xF0;
        //emb[0] |= PI ? 0x08U : 0x00;
        emb[0] |= (lcss << 1) & 0x06;
        emb[1]  = 0x00;
        // encode
        CQR1676::encode(emb);
        // and append
        buffer->ReplaceAt(33, (uint8)( (buffer->at(33) & 0xF0) | ((emb[0U] >> 4) & 0x0F) ));
        buffer->ReplaceAt(34, (uint8)( ((emb[0U] << 4) & 0xF0) | ((eLC[0U] >> 4) & 0x0F) ));
        buffer->ReplaceAt(35, (uint8)( ((eLC[0U] << 4) & 0xF0) | ((eLC[1U] >> 4) & 0x0F) ));
        buffer->ReplaceAt(36, (uint8)( ((eLC[1U] << 4) & 0xF0) | ((eLC[2U] >> 4) & 0x0F) ));
        buffer->ReplaceAt(37, (uint8)( ((eLC[2U] << 4) & 0xF0) | ((eLC[3U] >> 4) & 0x0F) ));
        buffer->ReplaceAt(38, (uint8)( ((eLC[3U] << 4) & 0xF0) | ((emb[1U] >> 4) & 0x0F) ));
        buffer->ReplaceAt(39, (uint8)( ((emb[1U] << 4) & 0xF0) | (buffer->at(39) & 0x0F) ));
    }
    // voice packet F
    else
    {
        // NULL
        uint8 emb[2];
        emb[0]  = (DMRMMDVM_REFLECTOR_COLOUR << 4) & 0xF0;
        //emb[0] |= PI ? 0x08U : 0x00;
        //emb[0] |= (LCSS << 1) & 0x06;
        emb[1]  = 0x00;
        // encode
        CQR1676::encode(emb);
        // and append
        buffer->ReplaceAt(33, (uint8)( (buffer->at(33) & 0xF0) | ((emb[0U] >> 4) & 0x0F) ));
        buffer->ReplaceAt(34, (uint8)( ((emb[0U] << 4) & 0xF0) ));
        buffer->ReplaceAt(35, (uint8)0);
        buffer->ReplaceAt(36, (uint8)0);
        buffer->ReplaceAt(37, (uint8)0);
        buffer->ReplaceAt(38, (uint8)( ((emb[1U] >> 4) & 0x0F) ));
        buffer->ReplaceAt(39, (uint8)( ((emb[1U] << 4) & 0xF0) | (buffer->at(39) & 0x0F) ));
    }
}

void CDmrmmdvmProtocol::EncodeEmbeddedLC(uint8 *embeddedLC, uint32 uiSrcId)
{
    uint8 lc[9];
    ::memset(lc, 0, sizeof(lc));
    // uiDstId = TG9
    lc[5] = 9;
    // uiSrcId
    lc[6] = (uint8)LOBYTE(HIWORD(uiSrcId));
    lc[7] = (uint8)HIBYTE(LOWORD(uiSrcId));
    lc[8] = (uint8)LOBYTE(LOWORD(uiSrcId));
    
    bool lc_b[72];
    for (uint32 a = 0; a < 9; a++)
        CUtils::byteToBitsBE(lc[a], lc_b + a*8);
    
    uint32 crc;
    CCRC::encodeFiveBit(lc_b, crc);
    
    bool data[128];
    ::memset(data, 0, 128 * sizeof(bool));
    
    data[106U] = (crc & 0x01U) == 0x01U;
    data[90U]  = (crc & 0x02U) == 0x02U;
    data[74U]  = (crc & 0x04U) == 0x04U;
    data[58U]  = (crc & 0x08U) == 0x08U;
    data[42U]  = (crc & 0x10U) == 0x10U;
    
    uint32 b = 0U;
    for (uint32 a = 0U; a < 11U; a++, b++)
        data[a] = lc_b[b];
    for (uint32 a = 16U; a < 27U; a++, b++)
        data[a] = lc_b[b];
    for (uint32 a = 32U; a < 42U; a++, b++)
        data[a] = lc_b[b];
    for (uint32 a = 48U; a < 58U; a++, b++)
        data[a] = lc_b[b];
    for (uint32 a = 64U; a < 74U; a++, b++)
        data[a] = lc_b[b];
    for (uint32 a = 80U; a < 90U; a++, b++)
        data[a] = lc_b[b];
    for (uint32 a = 96U; a < 106U; a++, b++)
        data[a] = lc_b[b];
    
    for (uint32 a = 0U; a < 112U; a += 16U)
        CHamming::encode16114(data + a);
    
    for (uint32 a = 0U; a < 16U; a++)
        data[a + 112U] = data[a + 0U] ^ data[a + 16U] ^ data[a + 32U] ^ data[a + 48U] ^ data[a + 64U] ^ data[a + 80U] ^ data[a + 96U];
    
    bool raw[128];
    
    b = 0U;
    for (uint32 a = 0U; a < 128U; a++) {
        raw[a] = data[b];
        b += 16U;
        if (b > 127U)
            b -= 127U;
    }
    
    for (uint32 a = 0; a < 16; a++)
        CUtils::bitsToByteBE(raw + a*8, embeddedLC[a]);
}

void CDmrmmdvmProtocol::AppendDmrIdToBuffer(CBuffer *buffer, uint32 id) const
{
    buffer->Append((uint8)LOBYTE(HIWORD(id)));
    buffer->Append((uint8)HIBYTE(LOWORD(id)));
    buffer->Append((uint8)LOBYTE(LOWORD(id)));
}

void CDmrmmdvmProtocol::AppendDmrRptrIdToBuffer(CBuffer *buffer, uint32 id) const
{
    buffer->Append((uint8)HIBYTE(HIWORD(id)));
    buffer->Append((uint8)LOBYTE(HIWORD(id)));
    buffer->Append((uint8)HIBYTE(LOWORD(id)));
    buffer->Append((uint8)LOBYTE(LOWORD(id)));
}
