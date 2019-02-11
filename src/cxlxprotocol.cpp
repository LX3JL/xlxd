//
//  cxlxprotocol.cpp
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

#include "main.h"
#include <string.h>
#include "cxlxpeer.h"
#include "cbmpeer.h"
#include "cxlxprotocol.h"
#include "creflector.h"
#include "cgatekeeper.h"


////////////////////////////////////////////////////////////////////////////////////////
// operation

bool CXlxProtocol::Init(void)
{
    bool ok;
    
    // base class
    ok = CProtocol::Init();
    
    // update the reflector callsign
    m_ReflectorCallsign.PatchCallsign(0, (const uint8 *)"XLX", 3);
    
    // create our socket
    ok &= m_Socket.Open(XLX_PORT);
    if ( !ok )
    {
        std::cout << "Error opening socket on port UDP" << XLX_PORT << " on ip " << g_Reflector.GetListenIp() << std::endl;
    }
    
    // update time
    m_LastKeepaliveTime.Now();
    m_LastPeersLinkTime.Now();
    
    // done
    return ok;
}

////////////////////////////////////////////////////////////////////////////////////////
// task

void CXlxProtocol::Task(void)
{
    CBuffer             Buffer;
    CIp                 Ip;
    CCallsign           Callsign;
    char                Modules[NB_MODULES_MAX+1];
    CVersion            Version;
    CDvHeaderPacket     *Header;
    CDvFramePacket      *Frame;
    CDvLastFramePacket  *LastFrame;
    
    // any incoming packet ?
    if ( m_Socket.Receive(&Buffer, &Ip, 20) != -1 )
    {
        // crack the packet
        if ( (Frame = IsValidDvFramePacket(Buffer)) != NULL )
        {
            //std::cout << "XLX (DExtra) DV frame"  << std::endl;
            
            // handle it
            OnDvFramePacketIn(Frame, &Ip);
        }
        else if ( (Header = IsValidDvHeaderPacket(Buffer)) != NULL )
        {
            //std::cout << "XLX (DExtra) DV header:"  << std::endl << *Header << std::endl;
            //std::cout << "XLX (DExtra) DV header on module " << Header->GetRpt2Module() << std::endl;
            
            // callsign muted?
            if ( g_GateKeeper.MayTransmit(Header->GetMyCallsign(), Ip) )
            {
                // handle it
                OnDvHeaderPacketIn(Header, Ip);
            }
            else
            {
                delete Header;
            }
        }
        else if ( (LastFrame = IsValidDvLastFramePacket(Buffer)) != NULL )
        {
            //std::cout << "XLX (DExtra) DV last frame" << std::endl;
            
            // handle it
            OnDvLastFramePacketIn(LastFrame, &Ip);
        }
        else if ( IsValidConnectPacket(Buffer, &Callsign, Modules, &Version) )
        {
            std::cout << "XLX ("
                      << Version.GetMajor() << "." << Version.GetMinor() << "." << Version.GetRevision()
                      << ") connect packet for modules " << Modules
                      << " from " << Callsign <<  " at " << Ip << std::endl;
            
            // callsign authorized?
            if ( g_GateKeeper.MayLink(Callsign, Ip, PROTOCOL_XLX, Modules) )
            {
                // acknowledge connecting request
                // following is version dependent
                switch ( GetConnectingPeerProtocolRevision(Callsign, Version) )
                {
                    case XLX_PROTOCOL_REVISION_0:
                        {
                            // already connected ?
                            CPeers *peers = g_Reflector.GetPeers();
                            if ( peers->FindPeer(Callsign, Ip, PROTOCOL_XLX) == NULL )
                            {
                                // acknowledge the request
                                EncodeConnectAckPacket(&Buffer, Modules);
                                m_Socket.Send(Buffer, Ip);
                            }
                            g_Reflector.ReleasePeers();
                            
                        }
                        break;
                    case XLX_PROTOCOL_REVISION_1:
                    case XLX_PROTOCOL_REVISION_2:
                    case XLX_PROTOCOL_REVISION_3:
                    default:
                        // acknowledge the request
                        EncodeConnectAckPacket(&Buffer, Modules);
                        m_Socket.Send(Buffer, Ip);
                        break;
                }
            }
            else
            {
                // deny the request
                EncodeConnectNackPacket(&Buffer);
                m_Socket.Send(Buffer, Ip);
            }
        }
        else if ( IsValidAckPacket(Buffer, &Callsign, Modules, &Version)  )
        {
            std::cout << "XLX ack packet for modules " << Modules << " from " << Callsign << " at " << Ip << std::endl;
            
            // callsign authorized?
            if ( g_GateKeeper.MayLink(Callsign, Ip, PROTOCOL_XLX, Modules) )
            {
                // already connected ?
                CPeers *peers = g_Reflector.GetPeers();
                if ( peers->FindPeer(Callsign, Ip, PROTOCOL_XLX) == NULL )
                {
                    // create the new peer
                    // this also create one client per module
                    CPeer *peer = CreateNewPeer(Callsign, Ip, Modules, Version);

                    // append the peer to reflector peer list
                    // this also add all new clients to reflector client list
                    peers->AddPeer(peer);
                }
                g_Reflector.ReleasePeers();
            }
        }
        else if ( IsValidDisconnectPacket(Buffer, &Callsign) )
        {
            std::cout << "XLX disconnect packet from " << Callsign << " at " << Ip << std::endl;
            
            // find peer
            CPeers *peers = g_Reflector.GetPeers();
            CPeer *peer = peers->FindPeer(Ip, PROTOCOL_XLX);
            if ( peer != NULL )
            {
                // remove it from reflector peer list
                // this also remove all concerned clients from reflector client list
                // and delete them
                peers->RemovePeer(peer);
            }
            g_Reflector.ReleasePeers();
        }
        else if ( IsValidNackPacket(Buffer, &Callsign) )
        {
            std::cout << "XLX nack packet from " << Callsign << " at " << Ip << std::endl;
        }
        else if ( IsValidKeepAlivePacket(Buffer, &Callsign) )
        {
            //std::cout << "XLX keepalive packet from " << Callsign << " at " << Ip << std::endl;
            
            // find peer
            CPeers *peers = g_Reflector.GetPeers();
            CPeer *peer = peers->FindPeer(Ip, PROTOCOL_XLX);
            if ( peer != NULL )
            {
                // keep it alive
                peer->Alive();
            }
            g_Reflector.ReleasePeers();
        }
        else
        {
            std::cout << "XLX packet (" << Buffer.size() << ")" << std::endl;
        }
    }
    
    // handle end of streaming timeout
    CheckStreamsTimeout();
    
    // handle queue from reflector
    HandleQueue();
    
    // keep alive
    if ( m_LastKeepaliveTime.DurationSinceNow() > XLX_KEEPALIVE_PERIOD )
    {
        // handle keep alives
        HandleKeepalives();
        
        // update time
        m_LastKeepaliveTime.Now();
    }
    
    // peer connections
    if ( m_LastPeersLinkTime.DurationSinceNow() > XLX_RECONNECT_PERIOD )
    {
        // handle remote peers connections
        HandlePeerLinks();
        
        // update time
        m_LastPeersLinkTime.Now();
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// queue helper

void CXlxProtocol::HandleQueue(void)
{
    m_Queue.Lock();
    while ( !m_Queue.empty() )
    {
        // get the packet
        CPacket *packet = m_Queue.front();
        m_Queue.pop();
        
        // check if origin of packet is local
        // if not, do not stream it out as it will cause
        // network loop between linked XLX peers
        if ( packet->IsLocalOrigin() )
        {
            // encode it
            CBuffer buffer;
            if ( EncodeDvPacket(*packet, &buffer) )
            {
                // encode revision dependent version
                CBuffer bufferLegacy = buffer;
                if ( packet->IsDvFrame() && (bufferLegacy.size() == 45) )
                {
                    bufferLegacy.resize(27);
                }
                
                // and push it to all our clients linked to the module and who are not streaming in
                CClients *clients = g_Reflector.GetClients();
                int index = -1;
                CClient *client = NULL;
                while ( (client = clients->FindNextClient(PROTOCOL_XLX, &index)) != NULL )
                {
                    // is this client busy ?
                    if ( !client->IsAMaster() && (client->GetReflectorModule() == packet->GetModuleId()) )
                    {
                        // no, send the packet
                        // this is protocol revision dependent
                        switch ( client->GetProtocolRevision() )
                        {
                            case XLX_PROTOCOL_REVISION_0:
                            case XLX_PROTOCOL_REVISION_1:
                                m_Socket.Send(bufferLegacy, client->GetIp());
                                break;
                            case XLX_PROTOCOL_REVISION_2:
                            default:
                                if ( g_Transcoder.IsConnected() )
                                {
                                    m_Socket.Send(buffer, client->GetIp());
                                }
                                else
                                {
                                    m_Socket.Send(bufferLegacy, client->GetIp());
                                }
                                break;
                        }
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

void CXlxProtocol::HandleKeepalives(void)
{
    // DExtra protocol sends and monitors keepalives packets
    // event if the client is currently streaming
    // so, send keepalives to all
    CBuffer keepalive;
    EncodeKeepAlivePacket(&keepalive);
    
    // iterate on peers
    CPeers *peers = g_Reflector.GetPeers();
    int index = -1;
    CPeer *peer = NULL;
    while ( (peer = peers->FindNextPeer(PROTOCOL_XLX, &index)) != NULL )
    {
        // send keepalive
        m_Socket.Send(keepalive, peer->GetIp());
        
        // client busy ?
        if ( peer->IsAMaster() )
        {
            // yes, just tickle it
            peer->Alive();
        }
        // otherwise check if still with us
        else if ( !peer->IsAlive() )
        {
            // no, disconnect
            CBuffer disconnect;
            EncodeDisconnectPacket(&disconnect);
            m_Socket.Send(disconnect, peer->GetIp());
            
            // remove it
            std::cout << "XLX peer " << peer->GetCallsign() << " keepalive timeout" << std::endl;
            peers->RemovePeer(peer);
        }        
    }
    g_Reflector.ReleasePeers();
}

////////////////////////////////////////////////////////////////////////////////////////
// Peers helpers

void CXlxProtocol::HandlePeerLinks(void)
{
    CBuffer buffer;
    
    // get the list of peers
    CPeerCallsignList *list = g_GateKeeper.GetPeerList();
    CPeers *peers = g_Reflector.GetPeers();

    // check if all our connected peers are still listed by gatekeeper
    // if not, disconnect
    int index = -1;
    CPeer *peer = NULL;
    while ( (peer = peers->FindNextPeer(PROTOCOL_XLX, &index)) != NULL )
    {
        if ( list->FindListItem(peer->GetCallsign()) == NULL )
        {
            // send disconnect packet
            EncodeDisconnectPacket(&buffer);
            m_Socket.Send(buffer, peer->GetIp());
            std::cout << "Sending disconnect packet to XLX peer " << peer->GetCallsign() << std::endl;
            // remove client
            peers->RemovePeer(peer);
        }
    }
    
    // check if all ours peers listed by gatekeeper are connected
    // if not, connect or reconnect
    for ( int i = 0; i < list->size(); i++ )
    {
        CCallsignListItem *item = &((list->data())[i]);
        if ( peers->FindPeer(item->GetCallsign(), PROTOCOL_XLX) == NULL )
        {
            // resolve again peer's IP in case it's a dynamic IP
            item->ResolveIp();
            // send connect packet to re-initiate peer link
            EncodeConnectPacket(&buffer, item->GetModules());
            m_Socket.Send(buffer, item->GetIp(), XLX_PORT);
            std::cout << "Sending connect packet to XLX peer " << item->GetCallsign() << " @ " << item->GetIp() << " for modules " << item->GetModules() << std::endl;
        }
    }
    
    // done
    g_Reflector.ReleasePeers();
    g_GateKeeper.ReleasePeerList();
}


////////////////////////////////////////////////////////////////////////////////////////
// streams helpers

bool CXlxProtocol::OnDvHeaderPacketIn(CDvHeaderPacket *Header, const CIp &Ip)
{
    bool newstream = false;
    CCallsign peer;
    
    // todo: verify Packet.GetModuleId() is in authorized list of XLX of origin
    // todo: do the same for DVFrame and DVLAstFrame packets

    // tag packet as remote peer origin
    Header->SetRemotePeerOrigin();
    
    // find the stream
    CPacketStream *stream = GetStream(Header->GetStreamId());
    if ( stream == NULL )
    {
        // no stream open yet, open a new one
        // find this client
        CClient *client = g_Reflector.GetClients()->FindClient(Ip, PROTOCOL_XLX, Header->GetRpt2Module());
        if ( client != NULL )
        {
            // and try to open the stream
            if ( (stream = g_Reflector.OpenStream(Header, client, ((CXlxClient *)client)->GetCodec())) != NULL )
            {
                // keep the handle
                m_Streams.push_back(stream);
                newstream = true;
            }
            // get origin
            peer = client->GetCallsign();
        }
        // release
        g_Reflector.ReleaseClients();
    }
    else
    {
        // stream already open
        // skip packet, but tickle the stream
        stream->Tickle();
    }
    
    // update last heard
    g_Reflector.GetUsers()->Hearing(Header->GetMyCallsign(), Header->GetRpt1Callsign(), Header->GetRpt2Callsign(), peer);
    g_Reflector.ReleaseUsers();
    
    // delete header if needed
    if ( !newstream )
    {
        delete Header;
    }
    
    // done
    return newstream;
}

void CXlxProtocol::OnDvFramePacketIn(CDvFramePacket *DvFrame, const CIp *Ip)
{
    // tag packet as remote peer origin
    DvFrame->SetRemotePeerOrigin();
    
    // anc call base class
    CDextraProtocol::OnDvFramePacketIn(DvFrame, Ip);
}

void CXlxProtocol::OnDvLastFramePacketIn(CDvLastFramePacket *DvFrame, const CIp *Ip)
{
    // tag packet as remote peer origin
    DvFrame->SetRemotePeerOrigin();
    
    // anc call base class
    CDextraProtocol::OnDvLastFramePacketIn(DvFrame, Ip);
}


////////////////////////////////////////////////////////////////////////////////////////
// packet decoding helpers

bool CXlxProtocol::IsValidKeepAlivePacket(const CBuffer &Buffer, CCallsign *callsign)
{
    bool valid = false;
    if (Buffer.size() == 9)
    {
        callsign->SetCallsign(Buffer.data(), 8);
        valid = callsign->IsValid();
    }
    return valid;
}


bool CXlxProtocol::IsValidConnectPacket(const CBuffer &Buffer, CCallsign *callsign, char *modules, CVersion *version)
{
    bool valid = false;
    if ((Buffer.size() == 39) && (Buffer.data()[0] == 'L') && (Buffer.data()[38] == 0))
    {
        callsign->SetCallsign((const uint8 *)&(Buffer.data()[1]), 8);
        ::strcpy(modules, (const char *)&(Buffer.data()[12]));
        valid = callsign->IsValid();
        *version = CVersion(Buffer.data()[9], Buffer.data()[10], Buffer.data()[11]);
        for ( int i = 0; i < ::strlen(modules); i++ )
        {
            valid &= IsLetter(modules[i]);
        }
    }
    return valid;
}

bool CXlxProtocol::IsValidDisconnectPacket(const CBuffer &Buffer, CCallsign *callsign)
{
    bool valid = false;
    if ((Buffer.size() == 10) && (Buffer.data()[0] == 'U') && (Buffer.data()[9] == 0))
    {
        callsign->SetCallsign((const uint8 *)&(Buffer.data()[1]), 8);
        valid = callsign->IsValid();
    }
    return valid;
}

bool CXlxProtocol::IsValidAckPacket(const CBuffer &Buffer, CCallsign *callsign, char *modules, CVersion *version)
{
    bool valid = false;
    if ((Buffer.size() == 39) && (Buffer.data()[0] == 'A') && (Buffer.data()[38] == 0))
    {
        callsign->SetCallsign((const uint8 *)&(Buffer.data()[1]), 8);
        ::strcpy(modules, (const char *)&(Buffer.data()[12]));
        valid = callsign->IsValid();
        *version = CVersion(Buffer.data()[9], Buffer.data()[10], Buffer.data()[11]);
        for ( int i = 0; i < ::strlen(modules); i++ )
        {
            valid &= IsLetter(modules[i]);
        }
    }
    return valid;
}

bool CXlxProtocol::IsValidNackPacket(const CBuffer &Buffer, CCallsign *callsign)
{
    bool valid = false;
    if ((Buffer.size() == 10) && (Buffer.data()[0] == 'N') && (Buffer.data()[9] == 0))
    {
        callsign->SetCallsign((const uint8 *)&(Buffer.data()[1]), 8);
        valid = callsign->IsValid();
    }
    return valid;
}

CDvFramePacket *CXlxProtocol::IsValidDvFramePacket(const CBuffer &Buffer)
{
    CDvFramePacket *dvframe = NULL;
    
    // base class first (protocol revision 1 and lower)
    dvframe = CDextraProtocol::IsValidDvFramePacket(Buffer);
    
    // otherwise try protocol revision 2
    if ( (dvframe == NULL) &&
         (Buffer.size() == 45) && (Buffer.Compare((uint8 *)"DSVT", 4) == 0) &&
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

    // otherwise try protocol revision 3
    if ( (dvframe == NULL) &&
         (Buffer.size() == 54) && (Buffer.Compare((uint8 *)"DSVT", 4) == 0) &&
         (Buffer.data()[4] == 0x20) && (Buffer.data()[8] == 0x20) &&
         ((Buffer.data()[14] & 0x40) == 0) )
    {
        // create packet
        dvframe = new CDvFramePacket(
            // sid
            *((uint16 *)&(Buffer.data()[12])),
            // dstar
            Buffer.data()[14], &(Buffer.data()[15]), &(Buffer.data()[24]), &(Buffer.data()[33]),
            // dmr
            Buffer.data()[36], Buffer.data()[37], &(Buffer.data()[38]), &(Buffer.data()[47]));
        
        // check validity of packet
        if ( !dvframe->IsValid() )
        {
            delete dvframe;
            dvframe = NULL;
        }
    }
    
    // done
    return dvframe;
}

CDvLastFramePacket *CXlxProtocol::IsValidDvLastFramePacket(const CBuffer &Buffer)
{
    CDvLastFramePacket *dvframe = NULL;
    
    // base class first (protocol revision 1 and lower)
    dvframe = CDextraProtocol::IsValidDvLastFramePacket(Buffer);
    
    // otherwise try protocol revision 2
    if ( (dvframe == NULL) &&
         (Buffer.size() == 45) && (Buffer.Compare((uint8 *)"DSVT", 4) == 0) &&
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

    // otherwise try protocol revision 3
    if ( (dvframe == NULL) &&
         (Buffer.size() == 54) && (Buffer.Compare((uint8 *)"DSVT", 4) == 0) &&
         (Buffer.data()[4] == 0x20) && (Buffer.data()[8] == 0x20) &&
         ((Buffer.data()[14] & 0x40) != 0) )
    {
        // create packet
        dvframe = new CDvLastFramePacket(
                                     // sid
                                     *((uint16 *)&(Buffer.data()[12])),
                                     // dstar
                                     Buffer.data()[14], &(Buffer.data()[15]), &(Buffer.data()[24]), &(Buffer.data()[33]),
                                     // dmr
                                     Buffer.data()[36], Buffer.data()[37], &(Buffer.data()[38]), &(Buffer.data()[47]));
        
        // check validity of packet
        if ( !dvframe->IsValid() )
        {
            delete dvframe;
            dvframe = NULL;
        }
    }
    
    // done
    return dvframe;
}

////////////////////////////////////////////////////////////////////////////////////////
// packet encoding helpers

void CXlxProtocol::EncodeKeepAlivePacket(CBuffer *Buffer)
{
    Buffer->Set(GetReflectorCallsign());
}

void CXlxProtocol::EncodeConnectPacket(CBuffer *Buffer, const char *Modules)
{
    uint8 tag[] = { 'L' };
    
    // tag
    Buffer->Set(tag, sizeof(tag));
    // our callsign
    Buffer->resize(Buffer->size()+8);
    g_Reflector.GetCallsign().GetCallsign(Buffer->data()+1);
    // our version
    Buffer->Append((uint8)VERSION_MAJOR);
    Buffer->Append((uint8)VERSION_MINOR);
    Buffer->Append((uint8)VERSION_REVISION);
    // the modules we share
    Buffer->Append(Modules);
    Buffer->resize(39);
}

void CXlxProtocol::EncodeDisconnectPacket(CBuffer *Buffer)
{
    uint8 tag[] = { 'U' };
    
    // tag
    Buffer->Set(tag, sizeof(tag));
    // our callsign
    Buffer->resize(Buffer->size()+8);
    g_Reflector.GetCallsign().GetCallsign(Buffer->data()+1);
    Buffer->Append((uint8)0);
}

void CXlxProtocol::EncodeConnectAckPacket(CBuffer *Buffer, const char *Modules)
{
    uint8 tag[] = { 'A' };
    
    // tag
    Buffer->Set(tag, sizeof(tag));
    // our callsign
    Buffer->resize(Buffer->size()+8);
    g_Reflector.GetCallsign().GetCallsign(Buffer->data()+1);
    // our version
    Buffer->Append((uint8)VERSION_MAJOR);
    Buffer->Append((uint8)VERSION_MINOR);
    Buffer->Append((uint8)VERSION_REVISION);
    // the modules we share
    Buffer->Append(Modules);
    Buffer->resize(39);
}

void CXlxProtocol::EncodeConnectNackPacket(CBuffer *Buffer)
{
    uint8 tag[] = { 'N' };
    
    // tag
    Buffer->Set(tag, sizeof(tag));
    // our callsign
    Buffer->resize(Buffer->size()+8);
    g_Reflector.GetCallsign().GetCallsign(Buffer->data()+1);
    Buffer->Append((uint8)0);
}

bool CXlxProtocol::EncodeDvFramePacket(const CDvFramePacket &Packet, CBuffer *Buffer) const
{
    uint8 tag[] = { 'D','S','V','T',0x20,0x00,0x00,0x00,0x20,0x00,0x01,0x02 };
    
    Buffer->Set(tag, sizeof(tag));
    Buffer->Append(Packet.GetStreamId());
    Buffer->Append((uint8)(Packet.GetDstarPacketId() % 21));
    Buffer->Append((uint8 *)Packet.GetAmbe(), AMBE_SIZE);
    Buffer->Append((uint8 *)Packet.GetCodec2(), AMBE_SIZE);
    Buffer->Append((uint8 *)Packet.GetDvData(), DVDATA_SIZE);
    
    Buffer->Append((uint8)Packet.GetDmrPacketId());
    Buffer->Append((uint8)Packet.GetDmrPacketSubid());
    Buffer->Append((uint8 *)Packet.GetAmbePlus(), AMBEPLUS_SIZE);
    Buffer->Append((uint8 *)Packet.GetDvSync(), DVSYNC_SIZE);
    
    return true;
   
}

bool CXlxProtocol::EncodeDvLastFramePacket(const CDvLastFramePacket &Packet, CBuffer *Buffer) const
{
    uint8 tag[]         = { 'D','S','V','T',0x20,0x00,0x00,0x00,0x20,0x00,0x01,0x02 };
    uint8 dstarambe[]   = { 0x55,0xC8,0x7A,0x00,0x00,0x00,0x00,0x00,0x00 };
    uint8 dstardvdata[] = { 0x25,0x1A,0xC6 };
    
    Buffer->Set(tag, sizeof(tag));
    Buffer->Append(Packet.GetStreamId());
    Buffer->Append((uint8)((Packet.GetPacketId() % 21) | 0x40));
    Buffer->Append(dstarambe, sizeof(dstarambe));
    Buffer->Append(dstarambe, sizeof(dstarambe));
    Buffer->Append(dstardvdata, sizeof(dstardvdata));
    
    
    Buffer->Append((uint8)Packet.GetDmrPacketId());
    Buffer->Append((uint8)Packet.GetDmrPacketSubid());
    Buffer->Append((uint8 *)Packet.GetAmbePlus(), AMBEPLUS_SIZE);
    Buffer->Append((uint8 *)Packet.GetDvSync(), DVSYNC_SIZE);
    
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
// protocol revision helper

int CXlxProtocol::GetConnectingPeerProtocolRevision(const CCallsign &Callsign, const CVersion &Version)
{
    int protrev;
    
    // BM ?
    if ( Callsign.HasSameCallsignWithWildcard(CCallsign("BM*")) )
    {
        protrev = CBmPeer::GetProtocolRevision(Version);
    }
    // otherwise, assume native xlx
    else
    {
        protrev = CXlxPeer::GetProtocolRevision(Version);
    }
    
    // done
    return protrev;
}

CPeer *CXlxProtocol::CreateNewPeer(const CCallsign &Callsign, const CIp &Ip, char *Modules, const CVersion &Version)
{
    CPeer *peer = NULL;
    
    // BM ?
    if ( Callsign.HasSameCallsignWithWildcard(CCallsign("BM*")) )
    {
        peer = new CBmPeer(Callsign, Ip, Modules, Version);
    }
    else
    {
        peer = new CXlxPeer(Callsign, Ip, Modules, Version);
    }
   
    // done
    return peer;
}

