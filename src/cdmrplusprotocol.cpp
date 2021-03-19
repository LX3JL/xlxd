//
//  cdmrplusprotocol.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 10/01/2016.
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
#include "cdmrplusclient.h"
#include "cdmrplusprotocol.h"
#include "creflector.h"
#include "cgatekeeper.h"
#include "cdmriddir.h"
#include "cbptc19696.h"
#include "crs129.h"
#include "cgolay2087.h"
#include "cqr1676.h"

////////////////////////////////////////////////////////////////////////////////////////
// constants

static uint8 g_DmrSyncBSVoice[]    = { 0x07,0x55,0xFD,0x7D,0xF7,0x5F,0x70 };
static uint8 g_DmrSyncBSData[]     = { 0x0D,0xFF,0x57,0xD7,0x5D,0xF5,0xD0 };
static uint8 g_DmrSyncMSVoice[]    = { 0x07,0xF7,0xD5,0xDD,0x57,0xDF,0xD0 };
static uint8 g_DmrSyncMSData[]     = { 0x0D,0x5D,0x7F,0x77,0xFD,0x75,0x70 };


////////////////////////////////////////////////////////////////////////////////////////
// operation

bool CDmrplusProtocol::Init(void)
{
    bool ok;
    
    // base class
    ok = CProtocol::Init();
    
    // update the reflector callsign
    //m_ReflectorCallsign.PatchCallsign(0, (const uint8 *)"DMR", 3);
    
    // create our socket
    ok &= m_Socket.Open(DMRPLUS_PORT);
    
    // update time
    m_LastKeepaliveTime.Now();
    
    // random number generator
    time_t t;
    ::srand((unsigned) time(&t));
    
    // done
    return ok;
}



////////////////////////////////////////////////////////////////////////////////////////
// task

void CDmrplusProtocol::Task(void)
{
    CBuffer             Buffer;
    CIp                 Ip;
    CCallsign           Callsign;
    char                ToLinkModule;
    CDvHeaderPacket     *Header;
    CDvFramePacket      *Frames[3];
    
    // handle incoming packets
    if ( m_Socket.Receive(&Buffer, &Ip, 20) != -1 )
    {
       // crack the packet
        if ( IsValidDvFramePacket(Ip, Buffer, Frames) )
        {
            //std::cout << "DMRplus DV frame" << std::endl;
            //Buffer.DebugDump(g_Reflector.m_DebugFile);

            for ( int i = 0; i < 3; i++ )
            {
                OnDvFramePacketIn(Frames[i], &Ip);
                /*if ( !Frames[i]->IsLastPacket() )
                {
                    //std::cout << "DMRplus DV frame" << std::endl;
                    OnDvFramePacketIn(Frames[i], &Ip);
                }
                else
                {
                    //std::cout << "DMRplus DV last frame" << std::endl;
                    OnDvLastFramePacketIn((CDvLastFramePacket *)Frames[i], &Ip);
                }*/
            }
        }
        else if ( IsValidDvHeaderPacket(Ip, Buffer, &Header) )
        {
            //std::cout << "DMRplus DV header:"  << std::endl;
            //std::cout << "DMRplus DV header:"  << std::endl <<  *Header << std::endl;
            //Buffer.DebugDump(g_Reflector.m_DebugFile);
            
            // callsign muted?
            if ( g_GateKeeper.MayTransmit(Header->GetMyCallsign(), Ip, PROTOCOL_DMRPLUS) )
            {
                // handle it
                OnDvHeaderPacketIn(Header, Ip);
            }
            else
            {
                delete Header;
            }
        }
        else if ( IsValidConnectPacket(Buffer, &Callsign, &ToLinkModule, Ip) )
        {
            //std::cout << "DMRplus keepalive/connect packet for module " << ToLinkModule << " from " << Callsign << " at " << Ip << std::endl;
            
            // callsign authorized?
            if ( g_GateKeeper.MayLink(Callsign, Ip, PROTOCOL_DMRPLUS) )
            {
                // acknowledge the request
                EncodeConnectAckPacket(&Buffer);
                m_Socket.Send(Buffer, Ip);
                
                // add client if needed
                CClients *clients = g_Reflector.GetClients();
                CClient *client = clients->FindClient(Callsign, Ip, PROTOCOL_DMRPLUS);
                // client already connected ?
                if ( client == NULL )
                {
                    std::cout << "DMRplus connect packet for module " << ToLinkModule << " from " << Callsign << " at " << Ip << std::endl;
                    
                    // create the client
                    CDmrplusClient *newclient = new CDmrplusClient(Callsign, Ip, ToLinkModule);
                    
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
                EncodeConnectNackPacket(&Buffer);
                m_Socket.Send(Buffer, Ip);
            }
            
        }
        else if ( IsValidDisconnectPacket(Buffer, &Callsign, &ToLinkModule) )
        {
            std::cout << "DMRplus disconnect packet for module " << ToLinkModule << " from " << Callsign << " at " << Ip << std::endl;
            
            // find client & remove it
            CClients *clients = g_Reflector.GetClients();
            CClient *client = clients->FindClient(Ip, PROTOCOL_DMRPLUS);
            if ( client != NULL )
            {
                clients->RemoveClient(client);
            }
            g_Reflector.ReleaseClients();
        }
        else
        {
            //std::cout << "DMRPlus packet (" << Buffer.size() << ")"  <<  " at " << Ip << std::endl;
        }
    }
    
    // handle end of streaming timeout
    CheckStreamsTimeout();
    
    // handle queue from reflector
    HandleQueue();
    
    
    // keep client alive
    if ( m_LastKeepaliveTime.DurationSinceNow() > DMRPLUS_KEEPALIVE_PERIOD )
    {
        //
        HandleKeepalives();
        
        // update time
        m_LastKeepaliveTime.Now();
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// streams helpers

bool CDmrplusProtocol::OnDvHeaderPacketIn(CDvHeaderPacket *Header, const CIp &Ip)
{
    bool newstream = false;
    
    // find the stream
    CPacketStream *stream = GetStream(Header->GetStreamId());
    if ( stream == NULL )
    {
        // no stream open yet, open a new one
        // find this client
        CClient *client = g_Reflector.GetClients()->FindClient(Ip, PROTOCOL_DMRPLUS);
        if ( client != NULL )
        {
            // and try to open the stream
            if ( (stream = g_Reflector.OpenStream(Header, client)) != NULL )
            {
                // keep the handle
                m_Streams.push_back(stream);
                newstream = true;
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
    
    // update last heard
    g_Reflector.GetUsers()->Hearing(Header->GetMyCallsign(), Header->GetRpt1Callsign(), Header->GetRpt2Callsign());
    g_Reflector.ReleaseUsers();
    
    // delete header if needed
    if ( !newstream )
    {
        delete Header;
    }
    
    
    // done
    return newstream;
}

////////////////////////////////////////////////////////////////////////////////////////
// queue helper

void CDmrplusProtocol::HandleQueue(void)
{
    
    m_Queue.Lock();
    while ( !m_Queue.empty() )
    {
        // get the packet
        CPacket *packet = m_Queue.front();
        m_Queue.pop();
        
        // get our sender's id
        int iModId = g_Reflector.GetModuleIndex(packet->GetModuleId());
        
        // encode
        CBuffer buffer;

        // check if it's header
        if ( packet->IsDvHeader() )
        {
            // update local stream cache
            // this relies on queue feeder setting valid module id
            m_StreamsCache[iModId].m_dvHeader = CDvHeaderPacket((const CDvHeaderPacket &)*packet);
            m_StreamsCache[iModId].m_uiSeqId = 4;
            
            // encode it
            EncodeDvHeaderPacket((const CDvHeaderPacket &)*packet, &buffer);
        }
        else
        {
            // update local stream cache or send triplet when needed
            switch ( packet->GetDmrPacketSubid() )
            {
                case 1:
                    m_StreamsCache[iModId].m_dvFrame0 = CDvFramePacket((const CDvFramePacket &)*packet);
                    break;
                case 2:
                    m_StreamsCache[iModId].m_dvFrame1 = CDvFramePacket((const CDvFramePacket &)*packet);
                    break;
                case 3:
                    EncodeDvPacket(
                                   m_StreamsCache[iModId].m_dvHeader,
                                   m_StreamsCache[iModId].m_dvFrame0,
                                   m_StreamsCache[iModId].m_dvFrame1,
                                   (const CDvFramePacket &)*packet,
                                   m_StreamsCache[iModId].m_uiSeqId,
                                   &buffer);
                    m_StreamsCache[iModId].m_uiSeqId = GetNextSeqId(m_StreamsCache[iModId].m_uiSeqId);
                    break;
                default:
                    break;
            }
            
        }
        
        // send it
        if ( buffer.size() > 0 )
        {
            // and push it to all our clients linked to the module and who are not streaming in
            CClients *clients = g_Reflector.GetClients();
            int index = -1;
            CClient *client = NULL;
            while ( (client = clients->FindNextClient(PROTOCOL_DMRPLUS, &index)) != NULL )
            {
                // is this client busy ?
                if ( !client->IsAMaster() && (client->GetReflectorModule() == packet->GetModuleId()) )
                {
                    // no, send the packet
                    m_Socket.Send(buffer, client->GetIp());
                }
            }
            g_Reflector.ReleaseClients();
            
            // debug
            //buffer.DebugDump(g_Reflector.m_DebugFile);
        }

        // done
        delete packet;
    }
    m_Queue.Unlock();
}

void CDmrplusProtocol::SendBufferToClients(const CBuffer &buffer, uint8 module)
{
    if ( buffer.size() > 0 )
    {
        // and push it to all our clients linked to the module and who are not streaming in
        CClients *clients = g_Reflector.GetClients();
        int index = -1;
        CClient *client = NULL;
        while ( (client = clients->FindNextClient(PROTOCOL_DMRPLUS, &index)) != NULL )
        {
            // is this client busy ?
            if ( !client->IsAMaster() && (client->GetReflectorModule() == module) )
            {
                // no, send the packet
                m_Socket.Send(buffer, client->GetIp());
            }
        }
        g_Reflector.ReleaseClients();
        
        // debug
        //buffer.DebugDump(g_Reflector.m_DebugFile);
    }
}


////////////////////////////////////////////////////////////////////////////////////////
// keepalive helpers

void CDmrplusProtocol::HandleKeepalives(void)
{
    // DMRplus protocol keepalive request is client tasks
    // here, just check that all clients are still alive
    // and disconnect them if not
    
    // iterate on clients
    CClients *clients = g_Reflector.GetClients();
    int index = -1;
    CClient *client = NULL;
    while ( (client = clients->FindNextClient(PROTOCOL_DMRPLUS, &index)) != NULL )
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
            //CBuffer disconnect;
            //EncodeDisconnectPacket(&disconnect, client);
            //m_Socket.Send(disconnect, client->GetIp());
            
            // remove it
            std::cout << "DMRplus client " << client->GetCallsign() << " keepalive timeout" << std::endl;
            clients->RemoveClient(client);
        }
        
    }
    g_Reflector.ReleaseClients();
}

////////////////////////////////////////////////////////////////////////////////////////
// packet decoding helpers

bool CDmrplusProtocol::IsValidConnectPacket(const CBuffer &Buffer, CCallsign *callsign, char *reflectormodule, const CIp &Ip)
{
    bool valid = false;
    if ( Buffer.size() == 31 )
    {
        char sz[9];
        ::memcpy(sz, Buffer.data(), 8);
        sz[8] = 0;
        uint32 dmrid = atoi(sz);
        callsign->SetDmrid(dmrid, true);
        callsign->SetModule(DMRPLUS_MODULE_ID);
        ::memcpy(sz, &Buffer.data()[8], 4);
        sz[4] = 0;
        *reflectormodule = DmrDstIdToModule(atoi(sz));
        valid = (callsign->IsValid() && (std::isupper(*reflectormodule) || (*reflectormodule == ' ')) );
        if ( !valid)
        {
            std::cout << "DMRplus connect packet from IP address " << Ip << " / unrecognized id " << (int)callsign->GetDmrid()  << std::endl;
        }
    }
    return valid;
}

bool CDmrplusProtocol::IsValidDisconnectPacket(const CBuffer &Buffer, CCallsign *callsign, char *reflectormodule)
{
    bool valid = false;
    if ( Buffer.size() == 32 )
    {
        char sz[9];
        ::memcpy(sz, Buffer.data(), 8);
        sz[8] = 0;
        uint32 dmrid = atoi(sz);
        callsign->SetDmrid(dmrid, true);
        callsign->SetModule(DMRPLUS_MODULE_ID);
        *reflectormodule = Buffer.data()[11] - '0' + 'A';
        valid = (callsign->IsValid() && std::isupper(*reflectormodule));
    }
    return valid;
}

bool CDmrplusProtocol::IsValidDvHeaderPacket(const CIp &Ip, const CBuffer &Buffer, CDvHeaderPacket **Header)
{
    bool valid = false;
    *Header = NULL;

    uint8 uiPacketType = Buffer.data()[8];
    if ( (Buffer.size() == 72)  && ( uiPacketType == 2 ) )
    {
        // frame details
        uint8 uiSlot = (Buffer.data()[16] == 0x22) ? DMR_SLOT2 : DMR_SLOT1;
        uint8 uiCallType = (Buffer.data()[62] == 1) ? DMR_GROUP_CALL : DMR_PRIVATE_CALL;
        uint8 uiColourCode = Buffer.data()[20] & 0x0F;
        if ( (uiSlot == DMRPLUS_REFLECTOR_SLOT) && (uiCallType == DMR_GROUP_CALL) && (uiColourCode == DMRPLUS_REFLECTOR_COLOUR) )
        {
            // more frames details
            //uint8 uiSeqId = Buffer.data()[4];
            //uint8 uiVoiceSeq = (Buffer.data()[18] & 0x0F) - 7; // aka slot type
            uint32 uiDstId = *(uint32 *)(&Buffer.data()[64]) & 0x00FFFFFF;
            uint32 uiSrcId = *(uint32 *)(&Buffer.data()[68]) & 0x00FFFFFF;

            // build DVHeader
            CCallsign csMY =  CCallsign("", uiSrcId);
            CCallsign rpt1 = CCallsign("", uiSrcId);
            rpt1.SetModule(DMRPLUS_MODULE_ID);
            CCallsign rpt2 = m_ReflectorCallsign;
            rpt2.SetModule(DmrDstIdToModule(uiDstId));
            uint32 uiStreamId = IpToStreamId(Ip);
            
            // and packet
            *Header = new CDvHeaderPacket(uiSrcId, CCallsign("CQCQCQ"), rpt1, rpt2, uiStreamId, 0, 0);
            valid = (*Header)->IsValid();
            if ( !valid )
            {
                delete *Header;
                *Header = NULL;
            }
        }
    }
    // done
    return valid;
}

bool CDmrplusProtocol::IsValidDvFramePacket(const CIp &Ip, const CBuffer &Buffer, CDvFramePacket **frames)
{
    bool valid = false;
    frames[0] = NULL;
    frames[1] = NULL;
    frames[2] = NULL;
    
    uint8 uiPacketType = Buffer.data()[8];
    if ( (Buffer.size() == 72)  && ((uiPacketType == 1) || (uiPacketType == 3)) )
    {
        // frame details
        uint8 uiSlot = (Buffer.data()[16] == 0x22) ? DMR_SLOT2 : DMR_SLOT1;
        uint8 uiCallType = (Buffer.data()[62] == 1) ? DMR_GROUP_CALL : DMR_PRIVATE_CALL;
        uint8 uiColourCode = Buffer.data()[20] & 0x0F;
        if ( (uiSlot == DMRPLUS_REFLECTOR_SLOT) && (uiCallType == DMR_GROUP_CALL) && (uiColourCode == DMRPLUS_REFLECTOR_COLOUR) )
        {
            // more frames details
            //uint8 uiSeqId = Buffer.data()[4];
            uint8 uiVoiceSeq = (Buffer.data()[18] & 0x0F) - 7; // aka slot type
            //uint32 uiDstId = *(uint32 *)(&Buffer.data()[64]) & 0x00FFFFFF;
            //uint32 uiSrcId = *(uint32 *)(&Buffer.data()[68]) & 0x00FFFFFF;
        
            // crack payload
            uint8 dmrframe[33];
            uint8 dmr3ambe[27];
            uint8 dmrambe[9];
            uint8 dmrsync[7];
            // get the 33 bytes ambe
            memcpy(dmrframe, &(Buffer.data()[26]), 33);
            // handle endianess
            SwapEndianess(dmrframe, sizeof(dmrframe));
            // extract the 3 ambe frames
            memcpy(dmr3ambe, dmrframe, 14);
            dmr3ambe[13] &= 0xF0;
            dmr3ambe[13] |= (dmrframe[19] & 0x0F);
            memcpy(&dmr3ambe[14], &dmrframe[20], 13);
            // extract sync
            dmrsync[0] = dmrframe[13] & 0x0F;
            ::memcpy(&dmrsync[1], &dmrframe[14], 5);
            dmrsync[6] = dmrframe[19] & 0xF0;
            
            // and create 3 dv frames
            uint32 uiStreamId = IpToStreamId(Ip);
            // frame1
            memcpy(dmrambe, &dmr3ambe[0], 9);
            frames[0] = new CDvFramePacket(dmrambe, dmrsync, uiStreamId, uiVoiceSeq, 1);
            
            // frame2
            memcpy(dmrambe, &dmr3ambe[9], 9);
            frames[1] = new CDvFramePacket(dmrambe, dmrsync, uiStreamId, uiVoiceSeq, 2);
            
            // frame3
            memcpy(dmrambe, &dmr3ambe[18], 9);
            if ( uiPacketType == 3 )
            {
                frames[2] = new CDvLastFramePacket(dmrambe, dmrsync, uiStreamId, uiVoiceSeq, 3);
            }
            else
            {
                frames[2] = new CDvFramePacket(dmrambe, dmrsync, uiStreamId, uiVoiceSeq, 3);
            }
            
            // check
            valid = true;
        }
    }
    
    // done
    return valid;
}


////////////////////////////////////////////////////////////////////////////////////////
// packet encoding helpers

void CDmrplusProtocol::EncodeConnectAckPacket(CBuffer *Buffer)
{
    uint8 tag[] = { 'A','C','K',' ','O','K',0x0A,0x00 };
    Buffer->Set(tag, sizeof(tag));
}

void CDmrplusProtocol::EncodeConnectNackPacket(CBuffer *Buffer)
{
    uint8 tag[] = { 'N','A','K',' ','O','K',0x0A,0x00 };
    Buffer->Set(tag, sizeof(tag));
}

bool CDmrplusProtocol::EncodeDvHeaderPacket(const CDvHeaderPacket &Packet, CBuffer *Buffer) const
{
    uint8 tag[]	= { 0x00,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x02,
        0x00,0x05,0x01,0x02,0x00,0x00,0x00  } ;
    Buffer->Set(tag, sizeof(tag));
    
    // uiSeqId
    //Buffer->ReplaceAt(4, 2);
    // uiPktType
    //Buffer->ReplaceAt(8, 2);
    // uiSlot
    Buffer->Append((uint16)((DMRPLUS_REFLECTOR_SLOT == DMR_SLOT1) ? 0x1111 : 0x2222));
    // uiSlotType
    Buffer->Append((uint16)0xEEEE);
    // uiColourCode
    uint8 uiColourCode = DMRPLUS_REFLECTOR_COLOUR | (DMRPLUS_REFLECTOR_COLOUR << 4);
    Buffer->Append((uint8)uiColourCode);
    Buffer->Append((uint8)uiColourCode);
    // uiFrameType
    Buffer->Append((uint16)0x1111);
    // reserved
    Buffer->Append((uint16)0x0000);
    // payload
    uint32 uiSrcId = Packet.GetMyCallsign().GetDmrid()  & 0x00FFFFFF;
    uint32 uiDstId = ModuleToDmrDestId(Packet.GetRpt2Module()) & 0x00FFFFFF;
    Buffer->Append((uint8)0x00, 34);
    Buffer->ReplaceAt(36, HIBYTE(HIWORD(uiSrcId)));
    Buffer->ReplaceAt(38, LOBYTE(HIWORD(uiSrcId)));
    Buffer->ReplaceAt(40, HIBYTE(LOWORD(uiSrcId)));
    Buffer->ReplaceAt(42, LOBYTE(LOWORD(uiSrcId)));
    
    // reserved
    Buffer->Append((uint16)0x0000);
    // uiCallType
    Buffer->Append((uint8)0x01);
    // reserved
    Buffer->Append((uint8)0x00);
    // uiDstId
    Buffer->Append(uiDstId);
    // uiSrcId
    Buffer->Append(uiSrcId);
    
    // done
    return true;
}

void CDmrplusProtocol::EncodeDvPacket
    (const CDvHeaderPacket &Header,
     const CDvFramePacket &DvFrame0, const CDvFramePacket &DvFrame1, const CDvFramePacket &DvFrame2,
     uint8 seqid, CBuffer *Buffer) const
 {
     
     uint8 tag[]	= { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,
                        0x00,0x05,0x01,0x02,0x00,0x00,0x00  } ;
     Buffer->Set(tag, sizeof(tag));
     
     // uiSeqId
     Buffer->ReplaceAt(4, seqid);
     // uiPktType
     //Buffer->ReplaceAt(8, 1);
     // uiSlot
     Buffer->Append((uint16)((DMRPLUS_REFLECTOR_SLOT == DMR_SLOT1) ? 0x1111 : 0x2222));
     // uiVoiceSeq
     uint8 uiVoiceSeq = (DvFrame0.GetDmrPacketId() + 7) | ((DvFrame0.GetDmrPacketId() + 7) << 4);
     Buffer->Append((uint8)uiVoiceSeq);
     Buffer->Append((uint8)uiVoiceSeq);
     // uiColourCode
     uint8 uiColourCode = DMRPLUS_REFLECTOR_COLOUR | (DMRPLUS_REFLECTOR_COLOUR << 4);
     Buffer->Append((uint8)uiColourCode);
     Buffer->Append((uint8)uiColourCode);
     // uiFrameType
     Buffer->Append((uint16)0x1111);
     // reserved
     Buffer->Append((uint16)0x0000);
     
     // payload
     uint32 uiSrcId = Header.GetMyCallsign().GetDmrid()  & 0x00FFFFFF;
     uint32 uiDstId = ModuleToDmrDestId(Header.GetRpt2Module()) & 0x00FFFFFF;
     // frame0
     Buffer->ReplaceAt(26, DvFrame0.GetAmbePlus(), 9);
     // 1/2 frame1
     Buffer->ReplaceAt(35, DvFrame1.GetAmbePlus(), 5);
     Buffer->ReplaceAt(39, (uint8)(Buffer->at(39) & 0xF0));
     // 1/2 frame1
     Buffer->ReplaceAt(45, DvFrame1.GetAmbePlus()+4, 5);
     Buffer->ReplaceAt(45, (uint8)(Buffer->at(45) & 0x0F));
     // frame2
     Buffer->ReplaceAt(50, DvFrame2.GetAmbePlus(), 9);

     // sync or embedded signaling
     ReplaceEMBInBuffer(Buffer, DvFrame0.GetDmrPacketId());

     // reserved
     Buffer->Append((uint16)0x0000);
     Buffer->Append((uint8)0x00);
     // uiCallType
     Buffer->Append((uint8)0x01);
     // reserved
     Buffer->Append((uint8)0x00);
     // uiDstId
     Buffer->Append(uiDstId);
     // uiSrcId
     Buffer->Append(uiSrcId);

     // handle indianess
     SwapEndianess(&(Buffer->data()[26]), 34);
}


void CDmrplusProtocol::EncodeDvLastPacket
    (const CDvHeaderPacket &Header,
     const CDvFramePacket &DvFrame0, const CDvFramePacket &DvFrame1, const CDvFramePacket &DvFrame2,
     uint8 seqid, CBuffer *Buffer) const
 {
     EncodeDvPacket(Header, DvFrame0, DvFrame1, DvFrame2, seqid, Buffer);
     Buffer->ReplaceAt(8, (uint8)3);
     Buffer->ReplaceAt(18, (uint16)0x2222);
 }

void CDmrplusProtocol::SwapEndianess(uint8 *buffer, int len) const
{
    for ( int i = 0; i < len; i += 2 )
    {
        uint8 t = buffer[i];
        buffer[i] = buffer[i+1];
        buffer[i+1] = t;
    }
}


////////////////////////////////////////////////////////////////////////////////////////
// SeqId helper

uint8 CDmrplusProtocol::GetNextSeqId(uint8 uiSeqId) const
{
    return (uiSeqId + 1) & 0xFF;
}

////////////////////////////////////////////////////////////////////////////////////////
// DestId to Module helper

char CDmrplusProtocol::DmrDstIdToModule(uint32 tg) const
{
    // is it a 4xxx ?
    if ( (tg >= 4001) && (tg <= (4000 + NB_OF_MODULES)) )
    {
        return ((char)(tg - 4001) + 'A');
    }
    return ' ';
}

uint32 CDmrplusProtocol::ModuleToDmrDestId(char m) const
{
    return (uint32)(m - 'A')+4001;
}

////////////////////////////////////////////////////////////////////////////////////////
// Buffer & LC helpers

void CDmrplusProtocol::AppendVoiceLCToBuffer(CBuffer *buffer, uint32 uiSrcId) const
{
    uint8 payload[34];
    
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
        slottype[0]  = (DMRPLUS_REFLECTOR_COLOUR << 4) & 0xF0;
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

void CDmrplusProtocol::ReplaceEMBInBuffer(CBuffer *buffer, uint8 uiDmrPacketId) const
{
    // voice packet A ?
    if ( uiDmrPacketId == 0 )
    {
        // sync
        buffer->ReplaceAt(39, (uint8)(buffer->at(39) | (g_DmrSyncBSVoice[0] & 0x0F)));
        buffer->ReplaceAt(40, g_DmrSyncBSVoice+1, 5);
        buffer->ReplaceAt(45, (uint8)(buffer->at(45) | (g_DmrSyncBSVoice[6] & 0xF0)));
    }
    // voice packet B,C,D,E ?
    else if ( (uiDmrPacketId >= 1) && (uiDmrPacketId <= 4 ) )
    {
        // EMB LC
        uint8 emb[2];
        emb[0]  = (DMRMMDVM_REFLECTOR_COLOUR << 4) & 0xF0;
        //emb[0] |= PI ? 0x08U : 0x00;
        //emb[0] |= (LCSS << 1) & 0x06;
        emb[1]  = 0x00;
        // encode
        CQR1676::encode(emb);
        // and append
        buffer->ReplaceAt(39, (uint8)((buffer->at(39) & 0xF0) | ((emb[0U] >> 4) & 0x0F)));
        buffer->ReplaceAt(40, (uint8)((buffer->at(40) & 0x0F) | ((emb[0U] << 4) & 0xF0)));
        buffer->ReplaceAt(40, (uint8)(buffer->at(40) & 0xF0));
        buffer->ReplaceAt(41, (uint8)0);
        buffer->ReplaceAt(42, (uint8)0);
        buffer->ReplaceAt(43, (uint8)0);
        buffer->ReplaceAt(44, (uint8)(buffer->at(44) & 0x0F));
        buffer->ReplaceAt(44, (uint8)((buffer->at(44) & 0xF0) | ((emb[1U] >> 4) & 0x0F)));
        buffer->ReplaceAt(45, (uint8)((buffer->at(45) & 0x0F) | ((emb[1U] << 4) & 0xF0)));
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
        buffer->ReplaceAt(39, (uint8)((buffer->at(39) & 0xF0) | ((emb[0U] >> 4) & 0x0F)));
        buffer->ReplaceAt(40, (uint8)((buffer->at(40) & 0x0F) | ((emb[0U] << 4) & 0xF0)));
        buffer->ReplaceAt(40, (uint8)(buffer->at(40) & 0xF0));
        buffer->ReplaceAt(41, (uint8)0);
        buffer->ReplaceAt(42, (uint8)0);
        buffer->ReplaceAt(43, (uint8)0);
        buffer->ReplaceAt(44, (uint8)(buffer->at(44) & 0x0F));
        buffer->ReplaceAt(44, (uint8)((buffer->at(44) & 0xF0) | ((emb[1U] >> 4) & 0x0F)));
        buffer->ReplaceAt(45, (uint8)((buffer->at(45) & 0x0F) | ((emb[1U] << 4) & 0xF0)));
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// uiStreamId helpers


// uiStreamId helpers
uint32 CDmrplusProtocol::IpToStreamId(const CIp &ip) const
{
    return ip.GetAddr() ^ (uint32)(MAKEDWORD(ip.GetPort(), ip.GetPort()));
}
