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
#include "cxlxdmrpeer.h"
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
                        // acknowledge the request
                        EncodeConnectAckPacket(&Buffer, Modules);
                        m_Socket.Send(Buffer, Ip);
                        break;
                    default:
                        // unknown protocol revision - still acknowledge for forward compatibility
                        std::cout << "XLX unknown protocol revision from " << Callsign << std::endl;
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
            std::cout << "XLX unknown packet (" << Buffer.size() << " bytes) from " << Ip << " [";
            int dumpLen = MIN((int)Buffer.size(), 16);
            for ( int i = 0; i < dumpLen; i++ )
            {
                if ( i > 0 ) std::cout << " ";
                std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)(unsigned char)Buffer.data()[i];
            }
            if ( (int)Buffer.size() > 16 ) std::cout << " ...";
            std::cout << std::dec << "]" << std::endl;
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
    // drain queue into local vector
    std::vector<CPacket *> packets;
    m_Queue.Lock();
    while ( !m_Queue.empty() )
    {
        packets.push_back(m_Queue.front());
        m_Queue.pop();
    }
    m_Queue.Unlock();

    // process packets without holding queue lock
    for ( size_t pi = 0; pi < packets.size(); pi++ )
    {
        CPacket *packet = packets[pi];

        // check if origin of packet is local
        // if not, do not stream it out as it will cause
        // network loop between linked XLX peers
        if ( packet->IsLocalOrigin() )
        {
            // encode it - full packet now includes Codec2
            CBuffer buffer;
            if ( EncodeDvPacket(*packet, &buffer) )
            {
                // encode revision dependent versions
                CBuffer bufferRev2 = buffer;
                CBuffer bufferLegacy = buffer;
                if ( packet->IsDvFrame() && (buffer.size() == XLX_DVFRAME_SIZE_REV3) )
                {
                    // Rev 2: D-Star + DMR, no Codec2
                    bufferRev2.resize(XLX_DVFRAME_SIZE_REV2);
                    // Rev 0/1: D-Star only
                    bufferLegacy.resize(XLX_DVFRAME_SIZE_REV01);
                }

                // push to all clients linked to the module and who are not streaming in
                CClients *clients = g_Reflector.GetClients();
                int index = -1;
                CClient *client = NULL;
                while ( (client = clients->FindNextClient(PROTOCOL_XLX, &index)) != NULL )
                {
                    if ( !client->IsAMaster() && (client->GetReflectorModule() == packet->GetModuleId()) )
                    {
                        // protocol revision dependent encoding
                        switch ( client->GetProtocolRevision() )
                        {
                            case XLX_PROTOCOL_REVISION_0:
                            case XLX_PROTOCOL_REVISION_1:
                                m_Socket.SendVoice(bufferLegacy, client->GetIp());
                                break;
                            case XLX_PROTOCOL_REVISION_2:
                                m_Socket.SendVoice(bufferRev2, client->GetIp());
                                break;
                            case XLX_PROTOCOL_REVISION_3:
                            default:
                                m_Socket.SendVoice(buffer, client->GetIp());
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
    
    // iterate on peers, collect timed-out ones for removal
    std::vector<CPeer *> toRemove;
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

            // collect for removal after loop
            std::cout << "XLX peer " << peer->GetCallsign() << " keepalive timeout" << std::endl;
            toRemove.push_back(peer);
        }
    }
    for ( size_t i = 0; i < toRemove.size(); i++ )
    {
        peers->RemovePeer(toRemove[i]);
    }
    g_Reflector.ReleasePeers();
}

////////////////////////////////////////////////////////////////////////////////////////
// Peers helpers

void CXlxProtocol::HandlePeerLinks(void)
{
    CBuffer buffer;

    // snapshot GK peer list, then release before acquiring Peers lock
    // to prevent GK+Peers lock-order inversion
    std::vector<CCallsignListItem> peerListSnapshot;
    {
        CPeerCallsignList *list = g_GateKeeper.GetPeerList();
        peerListSnapshot.assign(list->begin(), list->end());
        g_GateKeeper.ReleasePeerList();
    }

    // now work with Peers lock only
    CPeers *peers = g_Reflector.GetPeers();

    // collect peers to disconnect (collect-then-remove to avoid iterator invalidation)
    std::vector<CPeer *> toRemove;
    int index = -1;
    CPeer *peer = NULL;
    while ( (peer = peers->FindNextPeer(PROTOCOL_XLX, &index)) != NULL )
    {
        bool foundInList = false;
        for ( size_t i = 0; i < peerListSnapshot.size(); i++ )
        {
            if ( peerListSnapshot[i].GetCallsign().HasSameCallsign(peer->GetCallsign()) )
            {
                foundInList = true;
                break;
            }
        }
        if ( !foundInList )
        {
            // send disconnect packet while we still have the peer pointer
            EncodeDisconnectPacket(&buffer);
            m_Socket.Send(buffer, peer->GetIp());
            std::cout << "Sending disconnect packet to XLX peer " << peer->GetCallsign() << std::endl;
            toRemove.push_back(peer);
        }
    }
    for ( size_t i = 0; i < toRemove.size(); i++ )
    {
        peers->RemovePeer(toRemove[i]);
    }

    // check if all XLX peers listed by gatekeeper are connected
    // if not, connect or reconnect
    for ( size_t i = 0; i < peerListSnapshot.size(); i++ )
    {
        CCallsignListItem *item = &peerListSnapshot[i];

        // Skip peers handled by other protocols
        char csStr[CALLSIGN_LEN + 1];
        item->GetCallsign().GetCallsignString(csStr);
        if ( (::strncmp(csStr, "YSF", 3) == 0) ||
             ((::strncmp(csStr, "NX", 2) == 0) && (csStr[2] >= '0') && (csStr[2] <= '9')) ||
             ((::strncmp(csStr, "P25", 3) == 0) && (csStr[3] >= '0') && (csStr[3] <= '9')) ||
             ((::strncmp(csStr, "REF", 3) == 0) && (csStr[3] >= '0') && (csStr[3] <= '9')) ||
             ((::strncmp(csStr, "XRF", 3) == 0) && (csStr[3] >= '0') && (csStr[3] <= '9')) ||
             ((::strncmp(csStr, "DCS", 3) == 0) && (csStr[3] >= '0') && (csStr[3] <= '9')) )
        {
            continue;
        }

        if ( peers->FindPeer(item->GetCallsign(), PROTOCOL_XLX) == NULL )
        {
            // resolve peer's IP in case it's dynamic
            item->ResolveIp();
            // send connect packet to re-initiate peer link
            EncodeConnectPacket(&buffer, item->GetModules());
            m_Socket.Send(buffer, item->GetIp(), XLX_PORT);
            std::cout << "Sending connect packet to XLX peer " << item->GetCallsign() << " @ " << item->GetIp() << " for modules " << item->GetModules() << std::endl;
        }
    }

    // done
    g_Reflector.ReleasePeers();
}


////////////////////////////////////////////////////////////////////////////////////////
// streams helpers

bool CXlxProtocol::OnDvHeaderPacketIn(CDvHeaderPacket *Header, const CIp &Ip)
{
    bool newstream = false;
    bool lastheard = false;
    CCallsign peer;

    // Snapshot header fields for the Hearing() call at the end. OpenStream
    // consumes the Header pointer on success (deletes it after copying into
    // the stream), so we cannot safely dereference `Header` after that call.
    // Every other protocol uses the same local-snapshot pattern — see
    // e.g. cdmrmmdvmprotocol.cpp:365-370.
    CCallsign myCallsign   = Header->GetMyCallsign();
    CCallsign rpt1Callsign = Header->GetRpt1Callsign();
    CCallsign rpt2Callsign = Header->GetRpt2Callsign();

    // todo: verify Packet.GetModuleId() is in authorized list of XLX of origin
    // todo: do the same for DVFrame and DVLAstFrame packets

    // tag packet as remote peer origin
    Header->SetRemotePeerOrigin();

    // find the stream (match on both StreamId and IP to avoid false matches)
    CPacketStream *stream = GetStream(Header->GetStreamId(), &Ip);
    if ( stream == NULL )
    {
        // no stream open yet, open a new one
        // find this client
        CClient *client = g_Reflector.GetClients()->FindClient(Ip, PROTOCOL_XLX, Header->GetRpt2Module());
        if ( client != NULL )
        {
            // Snapshot the peer callsign BEFORE OpenStream — OpenStream may
            // release and re-acquire the Clients lock during its transcoder
            // handshake, after which `client` is not safe to dereference.
            peer = client->GetCallsign();

            // and try to open the stream
            if ( (stream = g_Reflector.OpenStream(Header, client)) != NULL )
            {
                // keep the handle
                m_Streams.push_back(stream);
                newstream = true;
                lastheard = true;
                // OpenStream consumes the caller's pointer on success — null
                // our local so the end-of-function cleanup doesn't double-free.
                Header = NULL;
            }
            else if ( g_Reflector.TryLateEntry(Header, client) )
            {
                Header = NULL;  // ownership transferred
                // NOT setting lastheard here — matches pre-squash behavior,
                // which skipped Hearing() on late-entry because its
                // `if (Header != NULL)` guard was already false after the
                // ownership transfer.
            }
            else
            {
                // open and late-entry both failed — still counted as heard
                // in the pre-squash code (Header still non-NULL at guard).
                lastheard = true;
            }
        }
        else
        {
            // no client found — pre-squash counted this as heard too
            // (Header still non-NULL, peer default/empty).
            lastheard = true;
        }
        // release
        g_Reflector.ReleaseClients();
    }
    else
    {
        // stream already open — duplicate header, tickle the stream.
        // Pre-squash counted this as heard (Header still non-NULL, peer
        // default/empty because client lookup is skipped in this branch).
        // Note this diverges from CDextraProtocol (parent) which skips
        // Hearing() on duplicate headers — intentional XLX behavior so
        // long transmissions through XLX peers keep their heard-list
        // timestamp fresh. Hearing() deduplicates via CUser::operator==
        // and falls through to HeardNow() on the existing entry.
        stream->Tickle();
        lastheard = true;
    }

    // update last heard
    if ( lastheard )
    {
        g_Reflector.GetUsers()->Hearing(myCallsign, rpt1Callsign, rpt2Callsign, peer);
        g_Reflector.ReleaseUsers();
    }

    // delete header if needed
    if ( !newstream && Header != NULL )
    {
        delete Header;
    }

    // done
    return newstream;
}

// Reconcile a freshly-parsed XLX wire-frame packet's Has-flags with
// what the source actually provides. The wire-frame parser memcpys
// all reserved slots blindly; only the source client knows whether
// those slots are authoritative. Two source types need different
// treatment:
//   - Native CXlxPeer (rev 0/1/2/3): all reserved slots authoritative,
//     matching CXlxClient::GetAvailableCodecs().
//   - CXlxDmrPeer (BM/FD/FS/PH): only AMBE+2 authoritative; the dstar
//     slot is reserved by the rev-2 wire format but empty/stale.
// GetClient(0) is sufficient because every client on the same peer
// shares the same protocol revision (CXlxClient) or codec assignment
// (CXlxDmrClient), so GetAvailableCodecs() returns the same mask
// regardless of which module's client we read.
// If the peer or its first client cannot be found (race during
// disconnect, never seen in practice), Conform is skipped and the
// default-false Has-flags from the constructor stand — the codec
// stream will then fill every slot from the transcoder. Harmless
// extra work, never silence.
static void ConformXlxPacketToSource(CDvFramePacket *DvFrame, const CIp *Ip)
{
    if ( Ip == NULL )
        return;
    uint32 codecMask = 0U;
    CPeers *peers = g_Reflector.GetPeers();
    CPeer *peer = peers->FindPeer(*Ip, PROTOCOL_XLX);
    if ( (peer != NULL) && (peer->GetNbClients() > 0) )
    {
        CClient *c = peer->GetClient(0);
        if ( c != NULL )
            codecMask = c->GetAvailableCodecs();
    }
    g_Reflector.ReleasePeers();
    if ( codecMask != 0U )
        DvFrame->ConformAvailableCodecs(codecMask);
}

void CXlxProtocol::OnDvFramePacketIn(CDvFramePacket *DvFrame, const CIp *Ip)
{
    // tag packet as remote peer origin
    DvFrame->SetRemotePeerOrigin();

    // align Has-flags with the source's actual codec set
    ConformXlxPacketToSource(DvFrame, Ip);

    // anc call base class
    CDextraProtocol::OnDvFramePacketIn(DvFrame, Ip);
}

void CXlxProtocol::OnDvLastFramePacketIn(CDvLastFramePacket *DvFrame, const CIp *Ip)
{
    // tag packet as remote peer origin
    DvFrame->SetRemotePeerOrigin();

    // align Has-flags with the source's actual codec set (CDvLastFramePacket
    // inherits from CDvFramePacket so the same method applies)
    ConformXlxPacketToSource(DvFrame, Ip);

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

    // otherwise try protocol revision 3 (with Codec2)
    if ( (dvframe == NULL) &&
         (Buffer.size() == XLX_DVFRAME_SIZE_REV3) && (Buffer.Compare((uint8 *)"DSVT", 4) == 0) &&
         (Buffer.data()[4] == 0x20) && (Buffer.data()[8] == 0x20) &&
         ((Buffer.data()[14] & 0x40) == 0) )
    {
        // create packet
        dvframe = new CDvFramePacket(
            // sid
            *((uint16 *)&(Buffer.data()[12])),
            // dstar
            Buffer.data()[14], &(Buffer.data()[15]), &(Buffer.data()[24]),
            // dmr
            Buffer.data()[27], Buffer.data()[28], &(Buffer.data()[29]), &(Buffer.data()[38]));

        // set Codec2 data (bytes 45-52)
        dvframe->SetCodec2(&(Buffer.data()[XLX_DVFRAME_SIZE_REV2]));

        // check validity of packet
        if ( !dvframe->IsValid() )
        {
            delete dvframe;
            dvframe = NULL;
        }
    }
    // otherwise try protocol revision 2 (without Codec2)
    else if ( (dvframe == NULL) &&
         (Buffer.size() == XLX_DVFRAME_SIZE_REV2) && (Buffer.Compare((uint8 *)"DSVT", 4) == 0) &&
         (Buffer.data()[4] == 0x20) && (Buffer.data()[8] == 0x20) &&
         ((Buffer.data()[14] & 0x40) == 0) )
    {
        // create packet
        dvframe = new CDvFramePacket(
            // sid
            *((uint16 *)&(Buffer.data()[12])),
            // dstar
            Buffer.data()[14], &(Buffer.data()[15]), &(Buffer.data()[24]),
            // dmr
            Buffer.data()[27], Buffer.data()[28], &(Buffer.data()[29]), &(Buffer.data()[38]));

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

    // otherwise try protocol revision 3 (with Codec2)
    if ( (dvframe == NULL) &&
         (Buffer.size() == XLX_DVFRAME_SIZE_REV3) && (Buffer.Compare((uint8 *)"DSVT", 4) == 0) &&
         (Buffer.data()[4] == 0x20) && (Buffer.data()[8] == 0x20) &&
         ((Buffer.data()[14] & 0x40) != 0) )
    {
        // create packet
        dvframe = new CDvLastFramePacket(
                                     // sid
                                     *((uint16 *)&(Buffer.data()[12])),
                                     // dstar
                                     Buffer.data()[14] & 0x1F, &(Buffer.data()[15]), &(Buffer.data()[24]),
                                     // dmr
                                     Buffer.data()[27], Buffer.data()[28], &(Buffer.data()[29]), &(Buffer.data()[38]));

        // set Codec2 data (bytes 45-52)
        dvframe->SetCodec2(&(Buffer.data()[XLX_DVFRAME_SIZE_REV2]));

        // check validity of packet
        if ( !dvframe->IsValid() )
        {
            delete dvframe;
            dvframe = NULL;
        }
    }
    // otherwise try protocol revision 2 (without Codec2)
    else if ( (dvframe == NULL) &&
         (Buffer.size() == XLX_DVFRAME_SIZE_REV2) && (Buffer.Compare((uint8 *)"DSVT", 4) == 0) &&
         (Buffer.data()[4] == 0x20) && (Buffer.data()[8] == 0x20) &&
         ((Buffer.data()[14] & 0x40) != 0) )
    {
        // create packet
        dvframe = new CDvLastFramePacket(
                                     // sid
                                     *((uint16 *)&(Buffer.data()[12])),
                                     // dstar
                                     Buffer.data()[14] & 0x1F, &(Buffer.data()[15]), &(Buffer.data()[24]),
                                     // dmr
                                     Buffer.data()[27], Buffer.data()[28], &(Buffer.data()[29]), &(Buffer.data()[38]));

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
    Buffer->Append((uint8 *)Packet.GetDvData(), DVDATA_SIZE);

    Buffer->Append((uint8)Packet.GetDmrPacketId());
    Buffer->Append((uint8)Packet.GetDmrPacketSubid());
    Buffer->Append((uint8 *)Packet.GetAmbePlus(), AMBEPLUS_SIZE);
    Buffer->Append((uint8 *)Packet.GetDvSync(), DVSYNC_SIZE);

    // Codec2 for M17 (revision 3)
    Buffer->Append((uint8 *)Packet.GetCodec2(), CODEC2_SIZE);

    return true;

}

bool CXlxProtocol::EncodeDvLastFramePacket(const CDvLastFramePacket &Packet, CBuffer *Buffer) const
{
    uint8 tag[]         = { 'D','S','V','T',0x20,0x00,0x00,0x00,0x20,0x00,0x01,0x02 };
    uint8 dstarambe[]   = { 0x55,0xC8,0x7A,0x00,0x00,0x00,0x00,0x00,0x00 };
    uint8 dstardvdata[] = { 0x25,0x1A,0xC6 };
    // Codec2 silence pattern (all zeros = no audio)
    static const uint8 codec2_silence[CODEC2_SIZE] = { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 };

    Buffer->Set(tag, sizeof(tag));
    Buffer->Append(Packet.GetStreamId());
    Buffer->Append((uint8)((Packet.GetPacketId() % 21) | 0x40));
    Buffer->Append(dstarambe, sizeof(dstarambe));
    Buffer->Append(dstardvdata, sizeof(dstardvdata));

    Buffer->Append((uint8)Packet.GetDmrPacketId());
    Buffer->Append((uint8)Packet.GetDmrPacketSubid());
    Buffer->Append((uint8 *)Packet.GetAmbePlus(), AMBEPLUS_SIZE);
    Buffer->Append((uint8 *)Packet.GetDvSync(), DVSYNC_SIZE);

    // Codec2 for M17 (revision 3) - use packet data if available, otherwise silence
    if ( Packet.HasCodec2Data() )
    {
        Buffer->Append((uint8 *)Packet.GetCodec2(), CODEC2_SIZE);
    }
    else
    {
        Buffer->Append(codec2_silence, CODEC2_SIZE);
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
// protocol revision helper

int CXlxProtocol::GetConnectingPeerProtocolRevision(const CCallsign &Callsign, const CVersion &Version)
{
    int protrev;
    
    // DMR peer (BM, FreeDMR, FreeStar, DVSPH) ?
    if ( Callsign.HasSameCallsignWithWildcard(CCallsign("BM*")) ||
         Callsign.HasSameCallsignWithWildcard(CCallsign("FD*")) ||
         Callsign.HasSameCallsignWithWildcard(CCallsign("FS*")) ||
         Callsign.HasSameCallsignWithWildcard(CCallsign("PH*")) )
    {
        protrev = CXlxDmrPeer::GetProtocolRevision(Version);
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
    
    // DMR peer (BM, FreeDMR, FreeStar, DVSPH) ?
    if ( Callsign.HasSameCallsignWithWildcard(CCallsign("BM*")) ||
         Callsign.HasSameCallsignWithWildcard(CCallsign("FD*")) ||
         Callsign.HasSameCallsignWithWildcard(CCallsign("FS*")) ||
         Callsign.HasSameCallsignWithWildcard(CCallsign("PH*")) )
    {
        peer = new CXlxDmrPeer(Callsign, Ip, Modules, Version);
    }
    else
    {
        peer = new CXlxPeer(Callsign, Ip, Modules, Version);
    }
   
    // done
    return peer;
}

