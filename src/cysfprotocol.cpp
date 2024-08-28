//
//  cysfprotocol.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 20/05/2018.
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
#include "ccrc.h"
#include "cysfpayload.h"
#include "cysfclient.h"
#include "cysfnodedirfile.h"
#include "cysfnodedirhttp.h"
#include "cysfutils.h"
#include "cysfprotocol.h"
#include "creflector.h"
#include "cgatekeeper.h"

////////////////////////////////////////////////////////////////////////////////////////
// constructor
CYsfProtocol::CYsfProtocol()
{
    m_seqNo = 0;
}

////////////////////////////////////////////////////////////////////////////////////////
// operation

bool CYsfProtocol::Init(void)
{
    bool ok;
    
    // base class
    ok = CProtocol::Init();
    
    // update the reflector callsign
    m_ReflectorCallsign.PatchCallsign(0, (const uint8 *)"YSF", 3);
    
    // create our socket
    ok &= m_Socket.Open(YSF_PORT);
    if ( !ok )
    {
        std::cout << "Error opening socket on port UDP" << YSF_PORT << " on ip " << g_Reflector.GetListenIp() << std::endl;
    }
    
    // init the wiresx cmd handler
    ok &= m_WiresxCmdHandler.Init();
    
    // update time
    m_LastKeepaliveTime.Now();
    
    // done
    return ok;
}

void CYsfProtocol::Close(void)
{
    // base class
    CProtocol::Close();
    
    // and close wiresx handler
    m_WiresxCmdHandler.Close();
}

////////////////////////////////////////////////////////////////////////////////////////
// task

void CYsfProtocol::Task(void)
{
    CBuffer             Buffer;
    CIp                 Ip;
    CCallsign           Callsign;
    CYSFFICH            Fich;
    CDvHeaderPacket     *Header;
    CDvFramePacket      *Frames[5];
    CWiresxCmd          WiresxCmd;
    
    int                 iWiresxCmd;
    int                 iWiresxArg;

    // handle outgoing packets
    {
        // any packet to go ?
        CWiresxPacketQueue *queue = m_WiresxCmdHandler.GetPacketQueue();
        while ( !queue->empty() )
        {
            CWiresxPacket packet = queue->front();
            queue->pop();
            m_Socket.Send(packet.GetBuffer(), packet.GetIp());
        }
        m_WiresxCmdHandler.ReleasePacketQueue();
    }
    
    // handle incoming packets
    if ( m_Socket.Receive(&Buffer, &Ip, 20) != -1 )
    {
        // crack the packet
        if ( IsValidDvPacket(Buffer, &Fich) )
        {
           //std::cout << "FN = " << (int)Fich.getFN() << "  FT = " << (int)Fich.getFT() << std::endl;
            if ( IsValidDvFramePacket(Ip, Fich, Buffer, Frames) )
            {
                //std::cout << "YSF DV frame"  << std::endl;
                
                // handle it
                OnDvFramePacketIn(Frames[0], &Ip);
                OnDvFramePacketIn(Frames[1], &Ip);
                OnDvFramePacketIn(Frames[2], &Ip);
                OnDvFramePacketIn(Frames[3], &Ip);
                OnDvFramePacketIn(Frames[4], &Ip);
            }
            else if ( IsValidDvHeaderPacket(Ip, Fich, Buffer, &Header, Frames) )
            {
                //std::cout << "YSF DV header:"  << std::endl << *Header << std::endl;
                //std::cout << "YSF DV header:"  << std::endl;
                
                // node linked and callsign muted?
                if ( g_GateKeeper.MayTransmit(Header->GetMyCallsign(), Ip, PROTOCOL_YSF, Header->GetRpt2Module())  )
                {
                    // handle it
                    OnDvHeaderPacketIn(Header, Ip);
                    //OnDvFramePacketIn(Frames[0], &Ip);
                    //OnDvFramePacketIn(Frames[1], &Ip);
                }
                else
                {
                    delete Header;
                }
            }
            else if ( IsValidDvLastFramePacket(Ip, Fich, Buffer, Frames) )
            {
                //std::cout << "YSF last DV frame"  << std::endl;

                // handle it
                OnDvFramePacketIn(Frames[0], &Ip);
                OnDvLastFramePacketIn((CDvLastFramePacket *)Frames[1], &Ip);
            }
        }
        else if ( IsValidConnectPacket(Buffer, &Callsign) )
        {
            //std::cout << "YSF keepalive/connect packet from " << Callsign << " at " << Ip << std::endl;
            
            // callsign authorized?
            if ( g_GateKeeper.MayLink(Callsign, Ip, PROTOCOL_YSF) )
            {
                // acknowledge the request
                EncodeConnectAckPacket(&Buffer);
                m_Socket.Send(Buffer, Ip);
                
                // add client if needed
                CClients *clients = g_Reflector.GetClients();
                CClient *client = clients->FindClient(Callsign, Ip, PROTOCOL_YSF);
                // client already connected ?
                if ( client == NULL )
                {
                    std::cout << "YSF connect packet from " << Callsign << " at " << Ip << std::endl;
                    
                    // create the client
                    CYsfClient *newclient = new CYsfClient(Callsign, Ip);
                    
                    // aautolink, if enabled
                    #if YSF_AUTOLINK_ENABLE
                        newclient->SetReflectorModule(YSF_AUTOLINK_MODULE);
                    #endif
                    
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
        }
        else if ( IsValidwirexPacket(Buffer, &Fich, &Callsign, &iWiresxCmd, &iWiresxArg) )
        {
            //std::cout << "YSF Wires-x frame"  << std::endl;
            // prepare the cmd object
            WiresxCmd = CWiresxCmd(Ip, Callsign, iWiresxCmd, iWiresxArg);
            // and post it to hadler's queue
            m_WiresxCmdHandler.GetCmdQueue()->push(WiresxCmd);
            m_WiresxCmdHandler.ReleaseCmdQueue();
        }
        else if ( IsValidServerStatusPacket(Buffer) )
        {
            std::cout << "YSF server status enquiry from " << Ip   << std::endl;
            // reply
            EncodeServerStatusPacket(&Buffer);
            m_Socket.Send(Buffer, Ip);
        }
        else
        {
            // invalid packet
            //std::cout << "YSF packet (" << Buffer.size() << ") from " << Callsign << " at " << Ip << std::endl;
            //Buffer.DebugDump(g_Reflector.m_DebugFile);
        }
    }
    
    // handle end of streaming timeout
    CheckStreamsTimeout();
    
    // handle queue from reflector
    HandleQueue();
    
    // keep client alive
    if ( m_LastKeepaliveTime.DurationSinceNow() > YSF_KEEPALIVE_PERIOD )
    {
        //
        HandleKeepalives();
        
        // update time
        m_LastKeepaliveTime.Now();
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// streams helpers

bool CYsfProtocol::OnDvHeaderPacketIn(CDvHeaderPacket *Header, const CIp &Ip)
{
    bool newstream = false;
    
    // find the stream
    CPacketStream *stream = GetStream(Header->GetStreamId());
    if ( stream == NULL )
    {
        // no stream open yet, open a new one
        CCallsign via(Header->GetRpt1Callsign());
        
        // find this client
        CClient *client = g_Reflector.GetClients()->FindClient(Ip, PROTOCOL_YSF);
        if ( client != NULL )
        {
            // get client callsign
            via = client->GetCallsign();

            if ( Header->GetRpt2Module() == ' ' ) {
                // module not filled, get module it's linked to
                Header->SetRpt2Module(client->GetReflectorModule());
            } else {
                // handle changing linked module to the one set on rpt2
                if ( client->GetReflectorModule() != Header->GetRpt2Module() ) {
                    std::cout << "YSF client " << client->GetCallsign() << " linking on module " << Header->GetRpt2Module() << std::endl;
                    client->SetReflectorModule(Header->GetRpt2Module());
                }
            }

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
        
        // update last heard
        if ( g_Reflector.IsValidModule(Header->GetRpt2Module()) )
        {
            g_Reflector.GetUsers()->Hearing(Header->GetMyCallsign(), via, Header->GetRpt2Callsign());
            g_Reflector.ReleaseUsers();
        }
        
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

void CYsfProtocol::HandleQueue(void)
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
            
            // encode it
            EncodeDvHeaderPacket((const CDvHeaderPacket &)*packet, &buffer);
        }
        // check if it's a last frame
        else if ( packet->IsLastPacket() )
        {
            // encode it
            EncodeDvLastPacket(m_StreamsCache[iModId].m_dvHeader, &buffer);
        }
        // otherwise, just a regular DV frame
        else
        {
            // update local stream cache or send triplet when needed
            uint8 sid = packet->GetYsfPacketSubId();
            if ( (sid >= 0) && (sid <= 4) )
            {
                //std::cout << (int)sid;
                m_StreamsCache[iModId].m_dvFrames[sid] = CDvFramePacket((const CDvFramePacket &)*packet);
                if ( sid == 4 )
                {
                    
                    EncodeDvPacket(m_StreamsCache[iModId].m_dvHeader, m_StreamsCache[iModId].m_dvFrames, &buffer);
                }
            }
        }
        
        // send it
        if ( buffer.size() > 0 )
        {
            // and push it to all our clients linked to the module and who are not streaming in
            CClients *clients = g_Reflector.GetClients();
            int index = -1;
            CClient *client = NULL;
            while ( (client = clients->FindNextClient(PROTOCOL_YSF, &index)) != NULL )
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
        
        // done
        delete packet;
    }
    m_Queue.Unlock();
}

////////////////////////////////////////////////////////////////////////////////////////
// keepalive helpers

void CYsfProtocol::HandleKeepalives(void)
{
    // YSF protocol keepalive request is client tasks
    // here, just check that all clients are still alive
    // and disconnect them if not
    
    // iterate on clients
    CClients *clients = g_Reflector.GetClients();
    int index = -1;
    CClient *client = NULL;
    while ( (client = clients->FindNextClient(PROTOCOL_YSF, &index)) != NULL )
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
            // no, remove it
            std::cout << "YSF client " << client->GetCallsign() << " keepalive timeout" << std::endl;
            clients->RemoveClient(client);
        }
        
    }
    g_Reflector.ReleaseClients();
}

////////////////////////////////////////////////////////////////////////////////////////
// DV packet decoding helpers

bool CYsfProtocol::IsValidConnectPacket(const CBuffer &Buffer, CCallsign *callsign)
{
    uint8 tag[] = { 'Y','S','F','P' };

    bool valid = false;
    if ( (Buffer.size() == 14) && (Buffer.Compare(tag, sizeof(tag)) == 0) )
    {
        callsign->SetCallsign(Buffer.data()+4, 8);
        callsign->SetModule(YSF_MODULE_ID);
        valid = (callsign->IsValid());
    }
    return valid;
}

bool CYsfProtocol::IsValidDvPacket(const CBuffer &Buffer, CYSFFICH *Fich)
{
    uint8 tag[] = { 'Y','S','F','D' };
    
    bool valid = false;

    if ( (Buffer.size() == 155) && (Buffer.Compare(tag, sizeof(tag)) == 0) )
    {
        // decode YSH fich
        if ( Fich->decode(&(Buffer.data()[40])) )
        {
            valid = (Fich->getDT() == YSF_DT_VD_MODE2);
        }
    }
    return valid;
}


bool CYsfProtocol::IsValidDvHeaderPacket(const CIp &Ip, const CYSFFICH &Fich, const CBuffer &Buffer, CDvHeaderPacket **header, CDvFramePacket **frames)
{
    bool valid = false;
    *header = NULL;
    frames[0] = NULL;
    frames[1] = NULL;

    // DV header ?
    if ( Fich.getFI() == YSF_FI_HEADER )
    {
        // get stream id
        uint32 uiStreamId = IpToStreamId(Ip);
        
        // get header data
        CYSFPayload ysfPayload;
        if ( ysfPayload.processHeaderData((unsigned char *)&(Buffer.data()[35])) )
        {
            // build DVHeader
             char sz[YSF_CALLSIGN_LENGTH+1];
            ::memcpy(sz, &(Buffer.data()[14]), YSF_CALLSIGN_LENGTH);
            sz[YSF_CALLSIGN_LENGTH] = 0;
            CCallsign csMY = CCallsign();
            csMY.SetYsfCallsign(sz);
            ::memcpy(sz, &(Buffer.data()[4]), YSF_CALLSIGN_LENGTH);
            sz[YSF_CALLSIGN_LENGTH] = 0;
            CCallsign rpt1 = CCallsign((const char *)sz);
            rpt1.SetModule(YSF_MODULE_ID);
            CCallsign rpt2 = m_ReflectorCallsign;

            if ( (Fich.getSQ() >= 10) && (Fich.getSQ() < 10+NB_OF_MODULES) )
            {
                // set module based on DG-ID value
                rpt2.SetModule( 'A' + (char)(Fich.getSQ() - 10) );
            }
            else
            {
                // as YSF protocol does not provide a module-tranlatable
                // destid, set module to none and rely on OnDvHeaderPacketIn()
                // to later fill it with proper value
                rpt2.SetModule(' ');
            }
            
            // and packet
            *header = new CDvHeaderPacket(csMY, CCallsign("CQCQCQ"), rpt1, rpt2, uiStreamId, Fich.getFN());
        }
        // and 2 DV Frames
        {
            uint8  uiAmbe[AMBE_SIZE];
            ::memset(uiAmbe, 0x00, sizeof(uiAmbe));
            frames[0] = new CDvFramePacket(uiAmbe, uiStreamId, Fich.getFN(), 0, (uint8)0);
            frames[1] = new CDvFramePacket(uiAmbe, uiStreamId, Fich.getFN(), 1, (uint8)0);
        }
        
        // check validity of packets
        if ( ((*header) == NULL) || !(*header)->IsValid() ||
             (frames[0] == NULL) || !(frames[0]->IsValid()) ||
             (frames[1] == NULL) || !(frames[1]->IsValid()) )

        {
            delete *header;
            *header = NULL;
            delete frames[0];
            delete frames[1];
            frames[0] = NULL;
            frames[1] = NULL;
        }
        else
        {
            valid = true;
         }

    }
    
    // done
    return valid;
}

bool CYsfProtocol::IsValidDvFramePacket(const CIp &Ip, const CYSFFICH &Fich, const CBuffer &Buffer, CDvFramePacket **frames)
{
    bool valid = false;
    frames[0] = NULL;
    frames[1] = NULL;
    frames[2] = NULL;
    frames[3] = NULL;
    frames[4] = NULL;
    
    // is it DV frame ?
    if ( Fich.getFI() == YSF_FI_COMMUNICATIONS )
    {
        // get stream id
        uint32 uiStreamId = IpToStreamId(Ip);
        
        // get DV frames
        uint8   ambe0[AMBEPLUS_SIZE];
        uint8   ambe1[AMBEPLUS_SIZE];
        uint8   ambe2[AMBEPLUS_SIZE];
        uint8   ambe3[AMBEPLUS_SIZE];
        uint8   ambe4[AMBEPLUS_SIZE];
        uint8 *ambes[5] = { ambe0, ambe1, ambe2, ambe3, ambe4 };
        CYsfUtils::DecodeVD2Vchs((unsigned char *)&(Buffer.data()[35]), ambes);

        // get DV frames
        uint8 fid = Buffer.data()[34];
        frames[0] = new CDvFramePacket(ambe0, uiStreamId, Fich.getFN(), 0, fid);
        frames[1] = new CDvFramePacket(ambe1, uiStreamId, Fich.getFN(), 1, fid);
        frames[2] = new CDvFramePacket(ambe2, uiStreamId, Fich.getFN(), 2, fid);
        frames[3] = new CDvFramePacket(ambe3, uiStreamId, Fich.getFN(), 3, fid);
        frames[4] = new CDvFramePacket(ambe4, uiStreamId, Fich.getFN(), 4, fid);
        
        // check validity of packets
        if ( (frames[0] == NULL) || !(frames[0]->IsValid()) ||
            (frames[1] == NULL) || !(frames[1]->IsValid()) ||
            (frames[2] == NULL) || !(frames[2]->IsValid()) ||
            (frames[3] == NULL) || !(frames[3]->IsValid()) ||
            (frames[4] == NULL) || !(frames[4]->IsValid()) )
        {
            delete frames[0];
            delete frames[1];
            delete frames[2];
            delete frames[3];
            delete frames[4];
            frames[0] = NULL;
            frames[1] = NULL;
            frames[2] = NULL;
            frames[3] = NULL;
            frames[4] = NULL;
        }
        else
        {
            valid = true;
        }
    }

    // done
    return valid;
}

bool CYsfProtocol::IsValidDvLastFramePacket(const CIp &Ip, const CYSFFICH &Fich, const CBuffer &Buffer, CDvFramePacket **frames)
{
    bool valid = false;
    frames[0] = NULL;
    frames[1] = NULL;
    
    // DV header ?
    if ( Fich.getFI() == YSF_FI_TERMINATOR )
    {
        // get stream id
        uint32 uiStreamId = IpToStreamId(Ip);
        
        // get DV frames
        {
            uint8  uiAmbe[AMBE_SIZE];
            ::memset(uiAmbe, 0x00, sizeof(uiAmbe));
            frames[0] = new CDvFramePacket(uiAmbe, uiStreamId, Fich.getFN(), 0, (uint8)0);
            frames[1] = new CDvLastFramePacket(uiAmbe, uiStreamId, Fich.getFN(), 1, (uint8)0);
        }
        
        // check validity of packets
        if ( (frames[0] == NULL) || !(frames[0]->IsValid()) ||
             (frames[1] == NULL) || !(frames[1]->IsValid()) )
            
        {
            delete frames[0];
            delete frames[1];
            frames[0] = NULL;
            frames[1] = NULL;
        }
        else
        {
            valid = true;
        }
    }
    
    // done
    return valid;
}

////////////////////////////////////////////////////////////////////////////////////////
// DV packet encoding helpers

void CYsfProtocol::EncodeConnectAckPacket(CBuffer *Buffer) const
{
    uint8 tag[] = { 'Y','S','F','P','R','E','F','L','E','C','T','O','R',0x20 };

    Buffer->Set(tag, sizeof(tag));
}

bool CYsfProtocol::EncodeDvHeaderPacket(const CDvHeaderPacket &Header, CBuffer *Buffer) const
{
    uint8 tag[]  = { 'Y','S','F','D' };
    uint8 dest[] = { 'A','L','L',' ',' ',' ',' ',' ',' ',' ' };
    char  sz[YSF_CALLSIGN_LENGTH];
    uint8 fichd[YSF_FICH_LENGTH_BYTES];

    // tag
    Buffer->Set(tag, sizeof(tag));
    // rpt1
    ::memset(sz, ' ', sizeof(sz));
    Header.GetRpt1Callsign().GetCallsignString(sz);
    sz[::strlen(sz)] = ' ';
    Buffer->Append((uint8 *)sz, YSF_CALLSIGN_LENGTH);
    // my
    ::memset(sz, ' ', sizeof(sz));
    Header.GetMyCallsign().GetCallsignString(sz);
    sz[::strlen(sz)] = ' ';
    Buffer->Append((uint8 *)sz, YSF_CALLSIGN_LENGTH);
    // dest
    Buffer->Append(dest, 10);
    // net frame counter
    Buffer->Append((uint8)0x00);
    // FS
    Buffer->Append((uint8 *)YSF_SYNC_BYTES, YSF_SYNC_LENGTH_BYTES);
    // FICH
    CYSFFICH fich;
    fich.setFI(YSF_FI_HEADER);
    fich.setCS(2U);
    //fich.setFN(Header.GetYsfPacketId());
    fich.setFN(0U);
    fich.setFT(7U);
    fich.setDev(0U);
    fich.setMR(YSF_MR_BUSY);
    fich.setDT(YSF_DT_VD_MODE2);
    fich.setSQL(0U);
    fich.setSQ(0U);
    fich.encode(fichd);
    Buffer->Append(fichd, YSF_FICH_LENGTH_BYTES);
    // payload
    unsigned char csd1[20U], csd2[20U];
    ::memset(csd1, '*', YSF_CALLSIGN_LENGTH);
    ::memset(csd1 + YSF_CALLSIGN_LENGTH, ' ', YSF_CALLSIGN_LENGTH);
    Header.GetMyCallsign().GetCallsignString(sz);
    ::memcpy(csd1 + YSF_CALLSIGN_LENGTH, sz, ::strlen(sz));
    ::memset(csd2, ' ', YSF_CALLSIGN_LENGTH + YSF_CALLSIGN_LENGTH);
    CYSFPayload payload;
    uint8 temp[120];
    payload.writeHeader(temp, csd1, csd2);
    Buffer->Append(temp+30, 120-30);
     
    // done
    return true;
}

bool CYsfProtocol::EncodeDvPacket(const CDvHeaderPacket &Header, const CDvFramePacket *DvFrames, CBuffer *Buffer) const
{
    uint8 tag[]  = { 'Y','S','F','D' };
    uint8 dest[] = { 'A','L','L',' ',' ',' ',' ',' ',' ',' ' };
    uint8 gps[]  = { 0x52,0x22,0x61,0x5F,0x27,0x03,0x5E,0x20,0x20,0x20 };
    char  sz[YSF_CALLSIGN_LENGTH];
    uint8 fichd[YSF_FICH_LENGTH_BYTES];
    
     // tag
    Buffer->Set(tag, sizeof(tag));
    // rpt1
    ::memset(sz, ' ', sizeof(sz));
    Header.GetRpt1Callsign().GetCallsignString(sz);
    sz[::strlen(sz)] = ' ';
    Buffer->Append((uint8 *)sz, YSF_CALLSIGN_LENGTH);
    // my
    ::memset(sz, ' ', sizeof(sz));
    Header.GetMyCallsign().GetCallsignString(sz);
    sz[::strlen(sz)] = ' ';
    Buffer->Append((uint8 *)sz, YSF_CALLSIGN_LENGTH);
    // dest
    Buffer->Append(dest, 10);
    // net frame counter
    Buffer->Append(DvFrames[0].GetYsfPacketFrameId());
     // FS
    Buffer->Append((uint8 *)YSF_SYNC_BYTES, YSF_SYNC_LENGTH_BYTES);
    // FICH
    CYSFFICH fich;
    fich.setFI(YSF_FI_COMMUNICATIONS);
    fich.setCS(2U);
    fich.setFN(DvFrames[0].GetYsfPacketId());
    fich.setFT(6U);
    fich.setDev(0U);
    fich.setMR(YSF_MR_BUSY);
    fich.setDT(YSF_DT_VD_MODE2);
    fich.setSQL(0U);
    fich.setSQ(0U);
    fich.encode(fichd);
    Buffer->Append(fichd, YSF_FICH_LENGTH_BYTES);
     // payload
    CYSFPayload payload;
    uint8 temp[120];
    ::memset(temp, 0x00, sizeof(temp));
    // DV
    for ( int i = 0; i < 5; i++ )
    {
        CYsfUtils::EncodeVD2Vch((unsigned char *)DvFrames[i].GetAmbePlus(), temp+35+(18*i));
    }
    // data
    switch (DvFrames[0].GetYsfPacketId())
    {
        case 0:
            // Dest
            payload.writeVDMode2Data(temp, (const unsigned char*)"**********");
            break;
        case 1:
            // Src
            ::memset(sz, ' ', sizeof(sz));
            Header.GetMyCallsign().GetCallsignString(sz);
            sz[::strlen(sz)] = ' ';
            payload.writeVDMode2Data(temp, (const unsigned char*)sz);
            break;
        case 2:
            // Down
            ::memset(sz, ' ', sizeof(sz));
            Header.GetRpt1Callsign().GetCallsignString(sz);
            sz[::strlen(sz)] = ' ';
            payload.writeVDMode2Data(temp, (const unsigned char*)sz);
            break;
        case 5:
            // Rem3+4
            // we need to provide a fake radioid for radios
            // to display src callsign
            payload.writeVDMode2Data(temp, (const unsigned char*)"     G0gBJ");
            break;
        case 6:
            // DT1
            // we need to issue a fake gps string with proper terminator
            // and crc for radios to display src callsign
            payload.writeVDMode2Data(temp, gps);
            break;
        default:
            payload.writeVDMode2Data(temp, (const unsigned char*)"          ");
            break;

    }
    Buffer->Append(temp+30, 120-30);

    // done
    return true;
}

bool CYsfProtocol::EncodeDvLastPacket(const CDvHeaderPacket &Header, CBuffer *Buffer) const
{
    uint8 tag[]  = { 'Y','S','F','D' };
    uint8 dest[] = { 'A','L','L',' ',' ',' ',' ',' ',' ',' ' };
    char  sz[YSF_CALLSIGN_LENGTH];
    uint8 fichd[YSF_FICH_LENGTH_BYTES];
    
    // tag
    Buffer->Set(tag, sizeof(tag));
    // rpt1
    ::memset(sz, ' ', sizeof(sz));
    Header.GetRpt1Callsign().GetCallsignString(sz);
    sz[::strlen(sz)] = ' ';
    Buffer->Append((uint8 *)sz, YSF_CALLSIGN_LENGTH);
    // my
    ::memset(sz, ' ', sizeof(sz));
    Header.GetMyCallsign().GetCallsignString(sz);
    sz[::strlen(sz)] = ' ';
    Buffer->Append((uint8 *)sz, YSF_CALLSIGN_LENGTH);
    // dest
    Buffer->Append(dest, 10);
    // eot status bit + net frame counter (<<1)
    Buffer->Append((uint8)0x01);
    // FS
    Buffer->Append((uint8 *)YSF_SYNC_BYTES, YSF_SYNC_LENGTH_BYTES);
    // FICH
    CYSFFICH fich;
    fich.setFI(YSF_FI_TERMINATOR);
    fich.setCS(2U);
    //fich.setFN(Header.GetYsfPacketId());
    fich.setFN(0U);
    fich.setFT(7U);
    fich.setDev(0U);
    fich.setMR(YSF_MR_BUSY);
    fich.setDT(YSF_DT_VD_MODE2);
    fich.setSQL(0U);
    fich.setSQ(0U);
    fich.encode(fichd);
    Buffer->Append(fichd, YSF_FICH_LENGTH_BYTES);
    // payload
    unsigned char csd1[20U], csd2[20U];
    ::memset(csd1, '*', YSF_CALLSIGN_LENGTH);
    ::memset(csd1 + YSF_CALLSIGN_LENGTH, ' ', YSF_CALLSIGN_LENGTH);
    Header.GetMyCallsign().GetCallsignString(sz);
    ::memcpy(csd1 + YSF_CALLSIGN_LENGTH, sz, ::strlen(sz));
    ::memset(csd2, ' ', YSF_CALLSIGN_LENGTH + YSF_CALLSIGN_LENGTH);
    CYSFPayload payload;
    uint8 temp[120];
    payload.writeHeader(temp, csd1, csd2);
    Buffer->Append(temp+30, 120-30);
    
    // done
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
// Wires-X packet decoding helpers

bool CYsfProtocol::IsValidwirexPacket(const CBuffer &Buffer, CYSFFICH *Fich, CCallsign *Callsign, int *Cmd, int *Arg)
{
    uint8 tag[] = { 'Y','S','F','D' };
    uint8 DX_REQ[]    = {0x5DU, 0x71U, 0x5FU};
    uint8 CONN_REQ[]  = {0x5DU, 0x23U, 0x5FU};
    uint8 DISC_REQ[]  = {0x5DU, 0x2AU, 0x5FU};
    uint8 ALL_REQ[]   = {0x5DU, 0x66U, 0x5FU};
    uint8 command[300];
    CYSFPayload payload;
    bool valid = false;

    if ( (Buffer.size() == 155) && (Buffer.Compare(tag, sizeof(tag)) == 0) )
    {
        // decode YSH fich
        if ( Fich->decode(&(Buffer.data()[40])) )
        {
            //std::cout << (int)Fich->getDT() << ","
            //          << (int)Fich->getFI() << ","
            //          << (int)Fich->getFN() << ","
            //          << (int)Fich->getFT()
            //          << std::endl;
            valid = (Fich->getDT() == YSF_DT_DATA_FR_MODE);
            valid &= (Fich->getFI() == YSF_FI_COMMUNICATIONS);
            if ( valid )
            {
                // get callsign
                Callsign->SetCallsign(&(Buffer.data()[4]), CALLSIGN_LEN, false);
                Callsign->SetModule(YSF_MODULE_ID);
                // decode payload
                if ( Fich->getFN() == 0U )
                {
                    valid = false;
                }
                else if ( Fich->getFN() == 1U )
                {
                    valid &= payload.readDataFRModeData2(&(Buffer.data()[35]), command + 0U);
                }
                else
                {
                    valid &= payload.readDataFRModeData1(&(Buffer.data()[35]), command + (Fich->getFN() - 1U) * 20U + 0U);
                    if ( valid )
                    {
                        valid &= payload.readDataFRModeData2(&(Buffer.data()[35]), command + (Fich->getFN() - 1U) * 20U + 20U);
                    }
                }
                // check crc if end found
                if ( Fich->getFN() == Fich->getFT() )
                {
                    valid = false;
                    // Find the end marker
                    for (unsigned int i = Fich->getFN() * 20U; i > 0U; i--)
                    {
                        if (command[i] == 0x03U)
                        {
                            unsigned char crc = CCRC::addCRC(command, i + 1U);
                            if (crc == command[i + 1U])
                                valid = true;
                            break;
                        }
                    }
                }
                // and crack the command
                if ( valid )
                {
                    // get argument
                    char buffer[4U];
                    ::memcpy(buffer, command + 5U + 2U, 3U);
                    buffer[3U] = 0x00U;
                    *Arg = ::atoi(buffer);
                    // and decode command
                    if (::memcmp(command + 1U, DX_REQ, 3U) == 0)
                    {
                        *Cmd = WIRESX_CMD_DX_REQ;
                        *Arg = 0;
                     }
                    else if (::memcmp(command + 1U, ALL_REQ, 3U) == 0)
                    {
                        // argument is start index of list
                       if ( *Arg > 0 )
                            (*Arg)--;
                        // check if all or search
                        if ( ::memcmp(command + 5U, "01", 2) == 0 )
                        {
                             *Cmd = WIRESX_CMD_ALL_REQ;
                        }
                        else if ( ::memcmp(command + 5U, "11", 2) == 0 )
                        {
                             *Cmd = WIRESX_CMD_SEARCH_REQ;
                        }
                     }
                    else if (::memcmp(command + 1U, CONN_REQ, 3U) == 0)
                    {
                        *Cmd = WIRESX_CMD_CONN_REQ;
                    }
                    else if (::memcmp(command + 1U, DISC_REQ, 3U) == 0)
                    {
                        *Cmd = WIRESX_CMD_DISC_REQ;
                        *Arg = 0;
                    }
                    else
                    {
                        std::cout << "Wires-X unknown command" << std::endl;
                        *Cmd = WIRESX_CMD_UNKNOWN;
                        *Arg = 0;
                        valid = false;
                    }
                }
            }
        }
    }
    return valid;
}

// server status packet decoding helpers

bool CYsfProtocol::IsValidServerStatusPacket(const CBuffer &Buffer) const
{
    uint8 tag[] = { 'Y','S','F','S' };
     
    return ( (Buffer.size() >= 4) && (Buffer.Compare(tag, sizeof(tag)) == 0) );
}

// server status packet encoding helpers

bool CYsfProtocol::EncodeServerStatusPacket(CBuffer *Buffer) const
{
    uint8 tag[] = { 'Y','S','F','S' };
    uint8 description[] = { 'X','L','X',' ','r','e','f','l','e','c','t','o','r',' ' };
    uint8 callsign[16];
     
    // tag
    Buffer->Set(tag, sizeof(tag));
    // hash
    ::memset(callsign, ' ', sizeof(callsign));
    g_Reflector.GetCallsign().GetCallsign(callsign);
    char sz[16];
    ::sprintf(sz, "%05u", CalcHash(callsign, 16) % 100000U);
    Buffer->Append((uint8 *)sz, 5);
    // name
    Buffer->Append(callsign, 16);
    // desscription
    Buffer->Append(description, 14);
    // connected clients
    CClients *clients = g_Reflector.GetClients();
    int count = MIN(999, clients->GetSize());
    g_Reflector.ReleaseClients();
    ::sprintf(sz, "%03u", count);
    Buffer->Append((uint8 *)sz, 3);
    
    // done
    return true;
}

uint32 CYsfProtocol::CalcHash(const uint8 *buffer, int len) const
{
    uint32 hash = 0U;

    for ( int i = 0; i < len; i++)
    {
        hash += buffer[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);

    return hash;
}


////////////////////////////////////////////////////////////////////////////////////////
// uiStreamId helpers


// uiStreamId helpers
uint32 CYsfProtocol::IpToStreamId(const CIp &ip) const
{
    return ip.GetAddr() ^ (uint32)(MAKEDWORD(ip.GetPort(), ip.GetPort()));
}

////////////////////////////////////////////////////////////////////////////////////////
// debug

#ifdef DEBUG_DUMPFILE
bool CYsfProtocol::DebugTestDecodePacket(const CBuffer &Buffer)
{
    uint8 tag[] = { 'Y','S','F','D' };
    static uint8 command[4098];
    static int len;
    CYSFFICH Fich;
    CYSFPayload payload;
    CBuffer dump;
    bool valid = false;
    
    if ( (Buffer.size() == 155) && (Buffer.Compare(tag, sizeof(tag)) == 0) )
    {
        // decode YSH fich
        if ( Fich.decode(&(Buffer.data()[40])) )
        {
            std::cout << (int)Fich.getDT() << ","
                      << (int)Fich.getFI() << ","
                      << (int)Fich.getBN() << ","
                      << (int)Fich.getBT() << ","
                      << (int)Fich.getFN() << ","
                      << (int)Fich.getFT() << " : ";
            
            switch ( Fich.getFI() )
            {
                case YSF_FI_HEADER:
                    len = 0;
                    ::memset(command, 0x00, sizeof(command));
                    std::cout << "Header" << std::endl;
                    break;
                case YSF_FI_TERMINATOR:
                    std::cout << "Trailer" << std::endl;
                    std::cout << "length of payload : " << len << std::endl;
                    dump.Set(command, len);
                    dump.DebugDump(g_Reflector.m_DebugFile);
                    dump.DebugDumpAscii(g_Reflector.m_DebugFile);
                    break;
                case YSF_FI_COMMUNICATIONS:
                    if ( Fich.getDT() == YSF_DT_DATA_FR_MODE )
                    {
                        valid = payload.readDataFRModeData1(&(Buffer.data()[35]), command + len);
                        len += 20;
                        valid &= payload.readDataFRModeData2(&(Buffer.data()[35]), command + len);
                        len += 20;
                        std::cout << "decoded ok" << std::endl;
                    }
                    break;
            }
        }
        else
        {
            std::cout << "invalid fich in packet" << std::endl;
        }
    }
    else
    {
        std::cout << "invalid size packet" << std::endl;
    }
    return valid;
}
#endif


bool CYsfProtocol::DebugDumpHeaderPacket(const CBuffer &Buffer)
{
    bool ok;
    CYSFFICH fich;
    CYSFPayload payload;
    uint8 data[200];

    :: memset(data, 0, sizeof(data));
    

    ok = IsValidDvPacket(Buffer, &fich);
    if ( ok && (fich.getFI() == YSF_FI_HEADER) )
    {
        ok &= payload.processHeaderData((unsigned char *)&(Buffer.data()[35]));
    }
    
    std::cout << "HD-" <<(ok ? "ok " : "xx ") << "src: " << payload.getSource() << "dest: " << payload.getDest() << std::endl;

    return ok;
}

bool CYsfProtocol::DebugDumpDvPacket(const CBuffer &Buffer)
{
    bool ok;
    CYSFFICH fich;
    CYSFPayload payload;
    uint8 data[200];

    :: memset(data, 0, sizeof(data));

    ok = IsValidDvPacket(Buffer, &fich);
    if ( ok && (fich.getFI() == YSF_FI_COMMUNICATIONS) )
    {
        ok &= payload.readVDMode2Data(&(Buffer.data()[35]), data);
    }

    std::cout << "DV-" <<(ok ? "ok " : "xx ") << "FN:" << (int)fich.getFN() << "  payload: " << (char *)data << std::endl;
 
    return ok;
}

bool CYsfProtocol::DebugDumpLastDvPacket(const CBuffer &Buffer)
{
    bool ok;
    CYSFFICH fich;
    CYSFPayload payload;
    uint8 data[200];

    :: memset(data, 0, sizeof(data));
    

    ok = IsValidDvPacket(Buffer, &fich);
    if ( ok && (fich.getFI() == YSF_FI_TERMINATOR) )
    {
        ok &= payload.processHeaderData((unsigned char *)&(Buffer.data()[35]));
    }
    
    std::cout << "TC-" <<(ok ? "ok " : "xx ") << "src: " << payload.getSource() << "dest: " << payload.getDest() << std::endl;

    return ok;
}
