//
//  cwiresxcmdhandler.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 09/10/2019.
//  Copyright © 2019 Jean-Luc Deltombe (LX3JL). All rights reserved.
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

#include <string.h>
#include "main.h"
#include "ccrc.h"
#include "cysffich.h"
#include "cysfpayload.h"
#include "cysfclient.h"
#include "cysfnodedirfile.h"
#include "cysfnodedirhttp.h"
#include "cysfutils.h"
#include "creflector.h"
#include "cwiresxcmdhandler.h"

////////////////////////////////////////////////////////////////////////////////////////
// constructor

CWiresxCmdHandler::CWiresxCmdHandler()
{
    m_seqNo = 0;
    m_bStopThread = false;
    m_pThread = NULL;
}


////////////////////////////////////////////////////////////////////////////////////////
// destructor

CWiresxCmdHandler::~CWiresxCmdHandler()
{
    // kill threads
    m_bStopThread = true;
    if ( m_pThread != NULL )
    {
        m_pThread->join();
        delete m_pThread;
    }
    
    // empty queue
    m_CmdQueue.Lock();
    while ( !m_CmdQueue.empty() )
    {
        m_CmdQueue.pop();
    }
    m_CmdQueue.Unlock();
}

////////////////////////////////////////////////////////////////////////////////////////
// initialization

bool CWiresxCmdHandler::Init(void)
{
    // fill our wiresx info
    m_ReflectorWiresxInfo.SetCallsign(g_Reflector.GetCallsign());
    m_ReflectorWiresxInfo.SetNode(g_Reflector.GetCallsign());
    m_ReflectorWiresxInfo.SetName("Reflector");

    // reset stop flag
    m_bStopThread = false;

    // start  thread;
    m_pThread = new std::thread(CWiresxCmdHandler::Thread, this);
    
    // done
    return true;
}

void CWiresxCmdHandler::Close(void)
{
    m_bStopThread = true;
    if ( m_pThread != NULL )
    {
        m_pThread->join();
        delete m_pThread;
        m_pThread = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// thread

void CWiresxCmdHandler::Thread(CWiresxCmdHandler *This)
{
    while ( !This->m_bStopThread )
    {
        This->Task();
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// task

void CWiresxCmdHandler::Task(void)
{
    CWiresxInfo Info;
    CWiresxCmd  Cmd;
    uint32  uiNodeTxFreq;
    uint32  uiNodeRxFreq;
    char cModule;
    bool bCmd;
    
    // anything to do ?
    bCmd = false;
    m_CmdQueue.Lock();
    {
        // any cmd in our queue ?
        if ( !m_CmdQueue.empty() )
        {
            // yes, get the command
            Cmd = m_CmdQueue.front();
            // check that the command is at least one second old
            // so that the so delayed processing of the command
            // introduce the delay the radio needs to switch
            // from tx to rx
            if ( Cmd.GetTime().DurationSinceNow() >= WIRESX_REPLY_DELAY )
            {
                m_CmdQueue.pop();
                bCmd = true;
            }
        }
    }
    m_CmdQueue.Unlock();
    
    
    // handle it
    if ( bCmd )
    {
        // fill our info object
        Info = m_ReflectorWiresxInfo;
        g_YsfNodeDir.FindFrequencies(Cmd.GetCallsign(), &uiNodeTxFreq, &uiNodeRxFreq);
        Info.SetFrequencies(uiNodeTxFreq, uiNodeRxFreq);

        // find our client and the module it's currentlink linked to
        cModule = ' ';
        CClients *clients = g_Reflector.GetClients();
        CClient *client = clients->FindClient(Cmd.GetCallsign(), Cmd.GetIp(), PROTOCOL_YSF);
        if ( client != NULL )
        {
            cModule = client->GetReflectorModule();
        }
        g_Reflector.ReleaseClients();
        
        // and crack the cmd
        switch ( Cmd.GetCmd() )
        {
            case WIRESX_CMD_DX_REQ:
                std::cout << "Wires-X DX_REQ command from " << Cmd.GetCallsign() << " at " << Cmd.GetIp() << std::endl;
                // reply
                ReplyToWiresxDxReqPacket(Cmd.GetIp(), Info, cModule);
                break;
            case WIRESX_CMD_ALL_REQ:
            case WIRESX_CMD_SEARCH_REQ:
                std::cout << "Wires-X ALL_REQ command from " << Cmd.GetCallsign() << " at " << Cmd.GetIp() << std::endl;
                // reply
                ReplyToWiresxAllReqPacket(Cmd.GetIp(), Info, Cmd.GetArg());
                break;
            case WIRESX_CMD_CONN_REQ:
                if ( (Cmd.GetArg() >= 1) && (Cmd.GetArg() <= NB_OF_MODULES) )
                {
                    cModule = 'A' + (char)(Cmd.GetArg() - 1);
                    std::cout << "Wires-X CONN_REQ command to link on module " << cModule << " from " << Cmd.GetCallsign() << " at " << Cmd.GetIp() << std::endl;
                    // acknowledge
                    ReplyToWiresxConnReqPacket(Cmd.GetIp(), Info, cModule);
                    // change client's module
                    CClients *clients = g_Reflector.GetClients();
                    CClient *client = clients->FindClient(Cmd.GetCallsign(), Cmd.GetIp(), PROTOCOL_YSF);
                    if ( client != NULL )
                    {
                        client->SetReflectorModule(cModule);
                    }
                    g_Reflector.ReleaseClients();
                }
                else
                {
                    std::cout << "Wires-X CONN_REQ command with illegal argument from " << Cmd.GetCallsign() << " at " << Cmd.GetIp() << std::endl;
                }
                break;
            case WIRESX_CMD_DISC_REQ:
                std::cout << "Wires-X DISC_REQ command from " << Cmd.GetCallsign() << " at " << Cmd.GetIp() << std::endl;
                // and reply
                ReplyToWiresxDiscReqPacket(Cmd.GetIp(), Info);
                // change client's module
                {
                    CClients *clients = g_Reflector.GetClients();
                    CClient *client = clients->FindClient(Cmd.GetCallsign(), Cmd.GetIp(), PROTOCOL_YSF);
                    if ( client != NULL )
                    {
                        client->SetReflectorModule(' ');
                    }
                    g_Reflector.ReleaseClients();
                }
                    break;
            case WIRESX_CMD_UNKNOWN:
            default:
                std::cout << "Wires-X non implemented command from " << Cmd.GetCallsign() << " at " << Cmd.GetIp() << std::endl;
                break;
        }
    }
    else
    {
        // if nothing to do, sleep a bit
        CTimePoint::TaskSleepFor(10);
    }

}

////////////////////////////////////////////////////////////////////////////////////////
// packet encoding

bool CWiresxCmdHandler::ReplyToWiresxDxReqPacket(const CIp &Ip, const CWiresxInfo &WiresxInfo, char Module)
{
    uint8 DX_RESP[] = {0x5DU, 0x51U, 0x5FU, 0x26U};
    bool ok;
    uint8 data[150U];
    uint8 RoomId;
    bool IsLinked;
    
    // linked module
    // module A == 0
    IsLinked = (Module != ' ');
    RoomId = (uint8)(Module - 'A');
    
    // fill data buffer
    ::memset(data, 0x00U, 150U);
    ::memset(data, ' ', 128U);
    // seq no
    data[0U] = m_seqNo;
    // command
    ::memcpy(data +  1U, DX_RESP, 4U);
    // node info
    ::memcpy(data +  5U, WiresxInfo.GetId(), 5U);
    ::memcpy(data + 10U, WiresxInfo.GetNode(), 10U);
    ::memcpy(data + 20U, WiresxInfo.GetName(), 14U);
    // linked room
    if (!IsLinked)
    {
        data[34U] = '1';
        data[35U] = '2';
        // no room linked
        data[57U] = '0';
        data[58U] = '0';
        data[59U] = '0';
    }
    else
    {
        data[34U] = '1';
        data[35U] = '5';
        // linked room
        char item[16U];
        // refl->m_id
        ::sprintf(item, "%05d", 4001U + RoomId);
        ::memcpy(data + 36U, item, 5U);
        // refl->name
        ::memset(item, ' ', 16U);
        ::memcpy(item, "Module", 6U);
        item[7] = 'A' + RoomId;
        ::memcpy(data + 41U, item, 16U);
        // refl->count
        ::sprintf(item, "%03d", RoomId + 1);
        ::memcpy(data + 57U, item, 3U);
        // other
        ::memset(data + 60U, ' ', 10U);
        // refl->m_desc
        ::memcpy(data + 70U, "Description   ", 14U);
    }

    // frequencies
    {
        unsigned int offset;
        char sign;
        if (WiresxInfo.GetTxFrequency() >= WiresxInfo.GetRxFrequency())
        {
            offset = WiresxInfo.GetTxFrequency() - WiresxInfo.GetRxFrequency();
            sign = '-';
        }
        else
        {
            offset = WiresxInfo.GetRxFrequency() - WiresxInfo.GetTxFrequency();
            sign = '+';
        }
        unsigned int freqHz = WiresxInfo.GetTxFrequency() % 1000000U;
        unsigned int freqkHz = (freqHz + 500U) / 1000U;

        char freq[30U];
        ::sprintf(freq, "%05u.%03u000%c%03u.%06u",
               WiresxInfo.GetTxFrequency() / 1000000U,
               freqkHz, sign, offset / 1000000U, offset % 1000000U);
        
        ::memcpy(data + 84U, freq, 23U);
    }

    // EOD & CRC
    data[127U] = 0x03U;
    data[128U] = CCRC::addCRC(data, 128U);

    // and encode the reply
    CBuffer Data;
    Data.Set(data, 129U);
    ok = EncodeAndSendWiresxPacket(Ip, Data, WiresxInfo);

    // done
    m_seqNo++;
    return ok;
}

bool CWiresxCmdHandler::ReplyToWiresxAllReqPacket(const CIp &Ip, const CWiresxInfo &WiresxInfo, int Start)
{
    bool ok = false;
    uint8 ALL_RESP[]  = {0x5DU, 0x46U, 0x5FU, 0x29U};
    uint8 data[1100U];
    
    // fill data buffer
    ::memset(data, 0x00U, 1100U);
    // seq no
    data[0U] = m_seqNo;
    // command
    ::memcpy(data + 1U, ALL_RESP, 4U);
    data[5U] = '2';
    data[6U] = '1';
    // node info
    ::memcpy(data +  7U, WiresxInfo.GetId(), 5U);
    ::memcpy(data + 12U, WiresxInfo.GetNode(), 10U);

    // number of entries
    uint total = NB_OF_MODULES;
    uint n = NB_OF_MODULES - Start;
    if (n > 20U)
        n = 20U;
    ::sprintf((char*)(data + 22U), "%03u%03u", n, total);
    data[28U] = 0x0DU;

    // entries
    uint offset = 29U;
    for ( uint i = 0; i < n; i++ )
    {
        char item[16U];
        // module A == 0
        int RoomId = i + Start;
        
        // prepare
        ::memset(data + offset, ' ', 50U);
        data[offset + 0U] = '5';

        // refl->m_id
        ::sprintf(item, "%05d", 4001U + RoomId);
        ::memcpy(data + offset + 1U, item, 5U);
        // refl->name
        ::memset(item, ' ', 16U);
        ::memcpy(item, "Module", 6U);
        item[7] = 'A' + RoomId;
        ::memcpy(data + offset + 6U, item, 16U);
        // refl->count
        ::sprintf(item, "%03d", RoomId + 1);
        ::memcpy(data + offset + 22U, item, 3U);
        // other
        ::memset(data + offset + 25U, ' ', 10U);
        // refl->m_desc
        ::memcpy(data + offset + 35U, "Description   ", 14U);
        data[offset + 49U] = 0x0DU;
        // next
        offset += 50U;
    }

    // the following is a workaround for FT2D which
    // do not accept less than 20 items frames.
    // first send a 'patched' frame
    if ( n < 20 )
    {
        // FT2D workaround
        uint offset2 = offset;
        // patch the remaining
        uint k = 1029U - offset2;
        ::memset(data+offset2, ' ', k);
        offset2 += k;
        
        // EOD + CRC
        data[offset2 + 0U] = 0x03U;
        data[offset2 + 1U] = CCRC::addCRC(data, offset2 + 1U);
        offset2 += 2U;
        
        // and encode the reply
        CBuffer Data;
        Data.Set(data, offset2 + 2U);
        ok = EncodeAndSendWiresxPacket(Ip, Data, WiresxInfo);
    }
    

    // and next repeat with normal frame
    {
        // EOD + CRC
        data[offset + 0U] = 0x03U;
        data[offset + 1U] = CCRC::addCRC(data, offset + 1U);
        offset += 2U;

        // patch the remaining
        //uint k = 1031U - offset;
        //::memset(data+offset, ' ', k);
        //offset += k;
        
        // and encode the reply
        CBuffer Data;
        Data.Set(data, offset + 2U);
        ok = EncodeAndSendWiresxPacket(Ip, Data, WiresxInfo);
    }


    // done
    m_seqNo++;
    return ok;
}

bool CWiresxCmdHandler::ReplyToWiresxConnReqPacket(const CIp &Ip, const CWiresxInfo &WiresxInfo, char Module)
{
    uint8 CONN_RESP[] = {0x5DU, 0x41U, 0x5FU, 0x26U};
    bool ok = false;
    uint8 data[110U];
    uint RoomId;

    // linked room
    // Module A == 0
    RoomId = (uint8)(Module - 'A');
    
    // prepare buffer
    ::memset(data, 0x00U, 110U);
    ::memset(data, ' ', 90U);
    
    // seq no
    data[0U] = m_seqNo;
    // command
    ::memcpy(data + 1U, CONN_RESP, 4U);
    // node info
    ::memcpy(data +  5U, WiresxInfo.GetId(), 5U);
    ::memcpy(data + 10U, WiresxInfo.GetNode(), 10U);
    ::memcpy(data + 20U, WiresxInfo.GetName(), 14U);
    data[34U] = '1';
    data[35U] = '5';
    // entry info
    {
        char item[16U];

        // refl->m_id
        ::sprintf(item, "%05d", 4001U + RoomId);
        ::memcpy(data + 36U, item, 5U);
        // refl->name
        ::memset(item, ' ', 16U);
        ::memcpy(item, "Module", 6U);
        item[7] = 'A' + RoomId;
        ::memcpy(data + 41U, item, 16U);
        // refl->count
        ::sprintf(item, "%03d", RoomId + 1);
        ::memcpy(data + 57U, item, 3U);
        // refl->m_desc
        ::memcpy(data + 70U, "Description   ", 14U);
    }
    data[84U] = '0';
    data[85U] = '0';
    data[86U] = '0';
    data[87U] = '0';
    data[88U] = '0';

    // EOD + CRC
    data[89U] = 0x03U;
    data[90U] = CCRC::addCRC(data, 90U);

    // and encode the reply
    CBuffer Data;
    Data.Set(data, 91U);
    ok = EncodeAndSendWiresxPacket(Ip, Data, WiresxInfo);

    // done
    m_seqNo++;
    return ok;
}

bool CWiresxCmdHandler::ReplyToWiresxDiscReqPacket(const CIp &Ip, const CWiresxInfo &WiresxInfo)
{
    uint8 DISC_RESP[] = {0x5DU, 0x41U, 0x5FU, 0x26U};
    bool ok = false;
    uint8 data[110U];
    
    // prepare buffer
    ::memset(data, 0x00U, 110U);
    ::memset(data, ' ', 90U);
    
    // seq no
    data[0U] = m_seqNo;
    // command
    ::memcpy(data + 1U, DISC_RESP, 4U);
    // node info
    ::memcpy(data +  5U, WiresxInfo.GetId(), 5U);
    ::memcpy(data + 10U, WiresxInfo.GetNode(), 10U);
    ::memcpy(data + 20U, WiresxInfo.GetName(), 14U);
    // data
    data[34U] = '1';
    data[35U] = '2';

    data[57U] = '0';
    data[58U] = '0';
    data[59U] = '0';

    // EOD + CRC
    data[89U] = 0x03U;
    data[90U] = CCRC::addCRC(data, 90U);

    // and encode the reply
    CBuffer Data;
    Data.Set(data, 91U);
    ok = EncodeAndSendWiresxPacket(Ip, Data, WiresxInfo);

    // done
    m_seqNo++;
    return ok;
}



////////////////////////////////////////////////////////////////////////////////////////
///// packet encoding helpers

bool CWiresxCmdHandler::EncodeAndSendWiresxPacket(const CIp &Ip, const CBuffer &DataOrg, const CWiresxInfo &WiresxInfo)
{
    uint8 DEFAULT_FICH[] = {0x20U, 0x00U, 0x01U, 0x00U};
    uint8 NET_HEADER[] = "YSFD                    ALL      ";
    CYSFFICH fich;
    CYSFPayload payload;
    uint8 buffer[200U];

    
    CBuffer Data(DataOrg);
    
    // seq no
    uint8 seqNo = 0U;
    
    // calculate bt and adjust length
    uint length = (uint)Data.size();
    uint8 bt = 0;
    if (length > 260U)
    {
        bt = 1U;
        bt += (length - 260U) / 259U;
        length += bt;
    }
    if (length > 20U)
    {
        uint blocks = (length - 20U) / 40U;
        if ((length % 40U) > 0U)
            blocks++;
        length = blocks * 40U + 20U;
    }
    else
    {
        length = 20U;
    }
    if ( length > (uint)Data.size() )
    {
        Data.Append((uint8)0x20U, (int)(length - (uint)Data.size()));
    }
    
    // ft
    uint8 ft = WiresxCalcFt(length, 0U);
    
    // Write the header
    {
        //header
        ::memcpy(buffer, NET_HEADER, 34U);
        ::memcpy(buffer + 4U, WiresxInfo.GetCallsign(), 10U);
        ::memcpy(buffer + 14U, WiresxInfo.GetNode(), 10U);
        // sync
        ::memcpy(buffer + 35U, YSF_SYNC_BYTES, YSF_SYNC_LENGTH_BYTES);
        // Fich
        fich.load(DEFAULT_FICH);
        fich.setFI(YSF_FI_HEADER);
        fich.setBT(bt);
        fich.setFT(ft);
        fich.encode(buffer + 35U + 5U);
        // payload
        payload.writeDataFRModeData1(WiresxInfo.GetCsd1(), buffer + 35U);
        payload.writeDataFRModeData2(WiresxInfo.GetCsd2(), buffer + 35U);
        // seqno
        buffer[34U] = seqNo;
        seqNo += 2U;
        // and post it
        SendPacket(Ip, buffer);
    }

    // write the payload
    fich.setFI(YSF_FI_COMMUNICATIONS);
    uint offset = 0;
    for ( uint8 bn = 0; bn <= bt; bn++ )
    {
        for ( uint8 fn = 0; fn <= ft; fn++ )
        {
            // fich
            fich.setFT(ft);
            fich.setFN(fn);
            fich.setBT(bt);
            fich.setBN(bn);
            fich.encode(buffer + 35U + 5U);
            // seq no
            buffer[34U] = seqNo;
            seqNo += 2U;
            // data
            //if ( (bn == 0) && (fn == 0) )
            if ( fn == 0 )
            {
                payload.writeDataFRModeData1(WiresxInfo.GetCsd1(), buffer + 35);
                payload.writeDataFRModeData2(WiresxInfo.GetCsd2(), buffer + 35);
            }
            //else if ( (bn == 0) && (fn == 1) )
            else if ( fn == 1 )
            {
                if ( bn == 0 )
                {
                    payload.writeDataFRModeData1(WiresxInfo.GetCsd3(), buffer + 35);
                    payload.writeDataFRModeData2(Data.data() + offset, buffer + 35);
                    offset += 20;
                }
                else
                {
                   uint8 temp[20U];
                   temp[0U] = 0x00U;
                   ::memcpy(temp + 1U, Data.data() + offset, 19U);
                   payload.writeDataFRModeData2(temp, buffer + 35U);
                   offset += 19U;
                }
            }
            else
            {
                payload.writeDataFRModeData1(Data.data() + offset, buffer + 35);
                offset += 20;
                payload.writeDataFRModeData2(Data.data() + offset, buffer + 35);
                offset += 20;
            }
            // and post it
            SendPacket(Ip, buffer);
            // and some delay before next packet
            CTimePoint::TaskSleepFor(100);
        }
    }

    // Write the trailer
    {
        // fich
        fich.setFI(YSF_FI_TERMINATOR);
        fich.setFN(ft);
        fich.setBN(bt);
        fich.encode(buffer + 35U + 5U);
        // payload
        payload.writeDataFRModeData1(WiresxInfo.GetCsd1(), buffer + 35U);
        payload.writeDataFRModeData2(WiresxInfo.GetCsd2(), buffer + 35U);
        // seq no
        buffer[34U] = seqNo | 0x01U;
        // and post it
        SendPacket(Ip, buffer);
    }
    
    // done
    return true;
}


uint8 CWiresxCmdHandler::WiresxCalcFt(uint length, uint offset) const
{
    length -= offset;
    if (length > 220U) return 7U;
    if (length > 180U) return 6U;
    if (length > 140U) return 5U;
    if (length > 100U) return 4U;
    if (length > 60U)  return 3U;
    if (length > 20U)  return 2U;
    return 1U;
}


void CWiresxCmdHandler::SendPacket(const CIp &Ip, uint8 *Buffer)
{
    //CBuffer packet(Buffer, 155);
    //DebugTestDecodePacket(packet);

    // and push in queue
    m_PacketQueue.Lock();
    {
        m_PacketQueue.push(CWiresxPacket(CBuffer(Buffer, 155), Ip));
    }
    m_PacketQueue.Unlock();
}


////////////////////////////////////////////////////////////////////////////////////////
// debug

#ifdef DEBUG_DUMPFILE
bool CWiresxCmdHandler::DebugTestDecodePacket(const CBuffer &Buffer)
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
