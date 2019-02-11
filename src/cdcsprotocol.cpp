//
//  cdcsprotocol.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 07/11/2015.
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
#include "cdcsclient.h"
#include "cdcsprotocol.h"
#include "creflector.h"
#include "cgatekeeper.h"

////////////////////////////////////////////////////////////////////////////////////////
// operation

bool CDcsProtocol::Init(void)
{
    bool ok;
    
    // base class
    ok = CProtocol::Init();
    
    // update the reflector callsign
    m_ReflectorCallsign.PatchCallsign(0, (const uint8 *)"DCS", 3);
    
    // create our socket
    ok &= m_Socket.Open(DCS_PORT);
    if ( !ok )
    {
        std::cout << "Error opening socket on port UDP" << DCS_PORT << " on ip " << g_Reflector.GetListenIp() << std::endl;
    }
    
    // update time
    m_LastKeepaliveTime.Now();
    
    // done
    return ok;
}



////////////////////////////////////////////////////////////////////////////////////////
// task

void CDcsProtocol::Task(void)
{
    CBuffer             Buffer;
    CIp                 Ip;
    CCallsign           Callsign;
    char                ToLinkModule;
    CDvHeaderPacket     *Header;
    CDvFramePacket      *Frame;
    
    // handle incoming packets
    if ( m_Socket.Receive(&Buffer, &Ip, 20) != -1 )
    {
        // crack the packet
        if ( IsValidDvPacket(Buffer, &Header, &Frame) )
        {
            //std::cout << "DCS DV packet" << std::endl;
            
            // callsign muted?
            if ( g_GateKeeper.MayTransmit(Header->GetMyCallsign(), Ip, PROTOCOL_DCS, Header->GetRpt2Module()) )
            {
                // handle it
                OnDvHeaderPacketIn(Header, Ip);
                
                if ( !Frame->IsLastPacket() )
                {
                    //std::cout << "DCS DV frame" << std::endl;
                    OnDvFramePacketIn(Frame, &Ip);
                }
                else
                {
                    //std::cout << "DCS DV last frame" << std::endl;
                    OnDvLastFramePacketIn((CDvLastFramePacket *)Frame, &Ip);
                }
            }
            else
            {
                delete Header;
                delete Frame;
            }
        }
        else if ( IsValidConnectPacket(Buffer, &Callsign, &ToLinkModule) )
        {
            std::cout << "DCS connect packet for module " << ToLinkModule << " from " << Callsign << " at " << Ip << std::endl;
            
            // callsign authorized?
            if ( g_GateKeeper.MayLink(Callsign, Ip, PROTOCOL_DCS) && g_Reflector.IsValidModule(ToLinkModule) )
            {
                // valid module ?
                if ( g_Reflector.IsValidModule(ToLinkModule) )
                {
                    // acknowledge the request
                    EncodeConnectAckPacket(Callsign, ToLinkModule, &Buffer);
                    m_Socket.Send(Buffer, Ip);
                    
                    // create the client
                    CDcsClient *client = new CDcsClient(Callsign, Ip, ToLinkModule);
                    
                    // and append
                    g_Reflector.GetClients()->AddClient(client);
                    g_Reflector.ReleaseClients();
                }
                else
                {
                    std::cout << "DCS node " << Callsign << " connect attempt on non-existing module" << std::endl;
                    
                    // deny the request
                    EncodeConnectNackPacket(Callsign, ToLinkModule, &Buffer);
                    m_Socket.Send(Buffer, Ip);
                }
            }
            else
            {
                // deny the request
                EncodeConnectNackPacket(Callsign, ToLinkModule, &Buffer);
                m_Socket.Send(Buffer, Ip);
            }
         
        }
        else if ( IsValidDisconnectPacket(Buffer, &Callsign) )
        {
            std::cout << "DCS disconnect packet from " << Callsign << " at " << Ip << std::endl;
            
            // find client
            CClients *clients = g_Reflector.GetClients();
            CClient *client = clients->FindClient(Ip, PROTOCOL_DCS);
            if ( client != NULL )
            {
                // remove it
                clients->RemoveClient(client);
                // and acknowledge the disconnect
                EncodeConnectNackPacket(Callsign, ' ', &Buffer);
                m_Socket.Send(Buffer, Ip);
            }
            g_Reflector.ReleaseClients();
        }
        else if ( IsValidKeepAlivePacket(Buffer, &Callsign) )
        {
            //std::cout << "DCS keepalive packet from " << Callsign << " at " << Ip << std::endl;
            
            // find all clients with that callsign & ip and keep them alive
            CClients *clients = g_Reflector.GetClients();
            int index = -1;
            CClient *client = NULL;
            while ( (client = clients->FindNextClient(Callsign, Ip, PROTOCOL_DCS, &index)) != NULL )
            {
                client->Alive();
            }
            g_Reflector.ReleaseClients();
        }
        else if ( IsIgnorePacket(Buffer) )
        {
            // valid but ignore packet
            //std::cout << "DCS ignored packet from " << Ip << std::endl;
        }
        else
        {
            // invalid packet
            std::cout << "DCS packet (" << Buffer.size() << ") from " << Ip << std::endl;
        }
    }
    
    // handle end of streaming timeout
    CheckStreamsTimeout();
    
    // handle queue from reflector
    HandleQueue();
    
    // keep client alive
    if ( m_LastKeepaliveTime.DurationSinceNow() > DCS_KEEPALIVE_PERIOD )
    {
        //
        HandleKeepalives();
        
        // update time
        m_LastKeepaliveTime.Now();
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// streams helpers

bool CDcsProtocol::OnDvHeaderPacketIn(CDvHeaderPacket *Header, const CIp &Ip)
{
    bool newstream = false;
    
    // find the stream
    CPacketStream *stream = GetStream(Header->GetStreamId());
    if ( stream == NULL )
    {
        // no stream open yet, open a new one
        CCallsign via(Header->GetRpt1Callsign());
        
        // find this client
        CClient *client = g_Reflector.GetClients()->FindClient(Ip, PROTOCOL_DCS);
        if ( client != NULL )
        {
            // get client callsign
            via = client->GetCallsign();
            // and try to open the stream
            if ( (stream = g_Reflector.OpenStream(Header, client, Header->GetCodec())) != NULL )
            {
                // keep the handle
                m_Streams.push_back(stream);
                newstream = true;
            }
        }
        // release
        g_Reflector.ReleaseClients();
        
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
// queue helper

void CDcsProtocol::HandleQueue(void)
{
    m_Queue.Lock();
    while ( !m_Queue.empty() )
    {
        // get the packet
        CPacket *packet = m_Queue.front();
        m_Queue.pop();
        
        // get our sender's id
        int iModId = g_Reflector.GetModuleIndex(packet->GetModuleId());
        
        // check if it's header and update cache
        if ( packet->IsDvHeader() )
        {
            // this relies on queue feeder setting valid module id
            m_StreamsCache[iModId].m_dvHeader = CDvHeaderPacket((const CDvHeaderPacket &)*packet);
            m_StreamsCache[iModId].m_iSeqCounter = 0;
        }
        else
        {
            // encode it
            CBuffer buffer;
            if ( packet->IsLastPacket() )
            {
                EncodeDvLastPacket(
                                   m_StreamsCache[iModId].m_dvHeader,
                                   (const CDvFramePacket &)*packet,
                                   m_StreamsCache[iModId].m_iSeqCounter++,
                                   &buffer);
            }
            else if ( packet->IsDvFrame() )
            {
                EncodeDvPacket(
                               m_StreamsCache[iModId].m_dvHeader,
                               (const CDvFramePacket &)*packet,
                               m_StreamsCache[iModId].m_iSeqCounter++,
                               &buffer);
            }
            
            // send it
            if ( buffer.size() > 0 )
            {
                // and push it to all our clients linked to the module and who are not streaming in
                CClients *clients = g_Reflector.GetClients();
                int index = -1;
                CClient *client = NULL;
                while ( (client = clients->FindNextClient(PROTOCOL_DCS, &index)) != NULL )
                {
                    // is this client busy ?
                    if ( !client->IsAMaster() && (client->GetReflectorModule() == packet->GetModuleId()) )
                    {
                        // no, send the packet
                        m_Socket.Send(buffer, client->GetIp());
                        
                    }
                }
                g_Reflector.ReleaseClients();
            }
        }
        
        // done
        delete packet;
    }
    m_Queue.Unlock();
}

////////////////////////////////////////////////////////////////////////////////////////
// keepalive helpers

void CDcsProtocol::HandleKeepalives(void)
{
    // DCS protocol sends and monitors keepalives packets
    // event if the client is currently streaming
    // so, send keepalives to all
    CBuffer keepalive1;
    EncodeKeepAlivePacket(&keepalive1);
    
    // iterate on clients
    CClients *clients = g_Reflector.GetClients();
    int index = -1;
    CClient *client = NULL;
    while ( (client = clients->FindNextClient(PROTOCOL_DCS, &index)) != NULL )
    {
        // encode client's specific keepalive packet
        CBuffer keepalive2;
        EncodeKeepAlivePacket(&keepalive2, client);
        
        // send keepalive
        m_Socket.Send(keepalive1, client->GetIp());
        m_Socket.Send(keepalive2, client->GetIp());
        
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
            EncodeDisconnectPacket(&disconnect, client);
            m_Socket.Send(disconnect, client->GetIp());
            
            // remove it
            std::cout << "DCS client " << client->GetCallsign() << " keepalive timeout" << std::endl;
            clients->RemoveClient(client);
        }
        
    }
    g_Reflector.ReleaseClients();
}

////////////////////////////////////////////////////////////////////////////////////////
// packet decoding helpers

bool CDcsProtocol::IsValidConnectPacket(const CBuffer &Buffer, CCallsign *callsign, char *reflectormodule)
{
    bool valid = false;
    if ( Buffer.size() == 519 )
    {
        callsign->SetCallsign(Buffer.data(), 8);
        callsign->SetModule(Buffer.data()[8]);
        *reflectormodule = Buffer.data()[9];
        valid = (callsign->IsValid() && IsLetter(*reflectormodule));
    }
    return valid;
}

bool CDcsProtocol::IsValidDisconnectPacket(const CBuffer &Buffer, CCallsign *callsign)
{
    bool valid = false;
    if ((Buffer.size() == 11) && (Buffer.data()[9] == ' '))
    {
        callsign->SetCallsign(Buffer.data(), 8);
        callsign->SetModule(Buffer.data()[8]);
        valid = callsign->IsValid();
    }
    else if ((Buffer.size() == 19) && (Buffer.data()[9] == ' ') && (Buffer.data()[10] == 0x00))
    {
        callsign->SetCallsign(Buffer.data(), 8);
        callsign->SetModule(Buffer.data()[8]);
        valid = callsign->IsValid();
    }
   return valid;
}

bool CDcsProtocol::IsValidKeepAlivePacket(const CBuffer &Buffer, CCallsign *callsign)
{
    bool valid = false;
    if ( (Buffer.size() == 17) || (Buffer.size() == 15) || (Buffer.size() == 22) )
    {
        callsign->SetCallsign(Buffer.data(), 8);
        valid = callsign->IsValid();
    }
    return valid;
}

bool CDcsProtocol::IsValidDvPacket(const CBuffer &Buffer, CDvHeaderPacket **header, CDvFramePacket **frame)
{
    uint8 tag[] = { '0','0','0','1' };
    
    bool valid = false;
    *header = NULL;
    *frame = NULL;
    
    if ( (Buffer.size() >= 100) && (Buffer.Compare(tag, sizeof(tag)) == 0) )
    {
        // get the header
        *header = new CDvHeaderPacket((struct dstar_header *)&(Buffer.data()[4]),
                                     *((uint16 *)&(Buffer.data()[43])), 0x80);
        
        // get the frame
        if ( ((Buffer.data()[45]) & 0x40) != 0 )
        {
            // it's the last frame
            *frame = new CDvLastFramePacket((struct dstar_dvframe *)&(Buffer.data()[46]),
                                             *((uint16 *)&(Buffer.data()[43])), Buffer.data()[45]);
        }
        else
        {
            // it's a regular DV frame
            *frame = new CDvFramePacket((struct dstar_dvframe *)&(Buffer.data()[46]),
                                         *((uint16 *)&(Buffer.data()[43])), Buffer.data()[45]);
        }
        
        // check validity of packets
        if ( !((*header)->IsValid() && (*frame)->IsValid()) )
        {
            delete *header;
            delete *frame;
            *header = NULL;
            *frame = NULL;
        }
        else
        {
            valid = true;
        }
    }
    // done
    return valid;
}

bool CDcsProtocol::IsIgnorePacket(const CBuffer &Buffer)
{
    bool valid = false;
    uint8 tag[] = { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, };
    
    if ( Buffer.size() == 15 )
    {
        valid = (Buffer.Compare(tag, sizeof(tag)) == 0);
    }
    return valid;
}


////////////////////////////////////////////////////////////////////////////////////////
// packet encoding helpers

void CDcsProtocol::EncodeKeepAlivePacket(CBuffer *Buffer)
{
    Buffer->Set(GetReflectorCallsign());
}

void CDcsProtocol::EncodeKeepAlivePacket(CBuffer *Buffer, CClient *Client)
{
    uint8 tag[] = { 0x0A,0x00,0x20,0x20 };
    
    Buffer->Set((uint8 *)(const char *)GetReflectorCallsign(), CALLSIGN_LEN-1);
    Buffer->Append((uint8)Client->GetReflectorModule());
    Buffer->Append((uint8)' ');
    Buffer->Append((uint8 *)(const char *)Client->GetCallsign(), CALLSIGN_LEN-1);
    Buffer->Append((uint8)Client->GetModule());
    Buffer->Append((uint8)Client->GetModule());
    Buffer->Append(tag, sizeof(tag));
}

void CDcsProtocol::EncodeConnectAckPacket(const CCallsign &Callsign, char ReflectorModule, CBuffer *Buffer)
{
    uint8 tag[] = { 'A','C','K',0x00 };
    uint8 cs[CALLSIGN_LEN];
    
    Callsign.GetCallsign(cs);
    Buffer->Set(cs, CALLSIGN_LEN-1);
    Buffer->Append((uint8)' ');
    Buffer->Append((uint8)Callsign.GetModule());
    Buffer->Append((uint8)ReflectorModule);
    Buffer->Append(tag, sizeof(tag));
}

void CDcsProtocol::EncodeConnectNackPacket(const CCallsign &Callsign, char ReflectorModule, CBuffer *Buffer)
{
    uint8 tag[] = { 'N','A','K',0x00 };
    uint8 cs[CALLSIGN_LEN];
    
    Callsign.GetCallsign(cs);
    Buffer->Set(cs, CALLSIGN_LEN-1);
    Buffer->Append((uint8)' ');
    Buffer->Append((uint8)Callsign.GetModule());
    Buffer->Append((uint8)ReflectorModule);
    Buffer->Append(tag, sizeof(tag));
}

void CDcsProtocol::EncodeDisconnectPacket(CBuffer *Buffer, CClient *Client)
{
    Buffer->Set((uint8 *)(const char *)Client->GetCallsign(), CALLSIGN_LEN-1);
    Buffer->Append((uint8)' ');
    Buffer->Append((uint8)Client->GetModule());
    Buffer->Append((uint8)0x00);
    Buffer->Append((uint8 *)(const char *)GetReflectorCallsign(), CALLSIGN_LEN-1);
    Buffer->Append((uint8)' ');
    Buffer->Append((uint8)0x00);
}

void CDcsProtocol::EncodeDvPacket(const CDvHeaderPacket &Header, const CDvFramePacket &DvFrame, uint32 iSeq, CBuffer *Buffer) const
{
    uint8 tag[] = { '0','0','0','1' };
    struct dstar_header DstarHeader;

    Header.ConvertToDstarStruct(&DstarHeader, CODEC_AMBEPLUS);

    Buffer->Set(tag, sizeof(tag));
    Buffer->Append((uint8 *)&DstarHeader, sizeof(struct dstar_header) - sizeof(uint16));
    Buffer->Append(DvFrame.GetStreamId());
    Buffer->Append((uint8)(DvFrame.GetPacketId() % 21));
    Buffer->Append((uint8 *)DvFrame.GetAmbe(), AMBE_SIZE);
    Buffer->Append((uint8 *)DvFrame.GetDvData(), DVDATA_SIZE);
    Buffer->Append((uint8)((iSeq >> 0) & 0xFF));
    Buffer->Append((uint8)((iSeq >> 8) & 0xFF));
    Buffer->Append((uint8)((iSeq >> 16) & 0xFF));
    Buffer->Append((uint8)0x01);
    Buffer->Append((uint8)0x00, 38);
}

void CDcsProtocol::EncodeDvLastPacket(const CDvHeaderPacket &Header, const CDvFramePacket &DvFrame, uint32 iSeq, CBuffer *Buffer) const
{
    EncodeDvPacket(Header, DvFrame, iSeq, Buffer);
    (Buffer->data())[45] |= 0x40;
}
