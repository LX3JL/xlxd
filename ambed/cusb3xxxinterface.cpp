//
//  cusb3xxxinterface.cpp
//  ambed
//
//  Created by Jean-Luc Deltombe (LX3JL) on 26/04/2017.
//  Copyright © 2015 Jean-Luc Deltombe (LX3JL). All rights reserved.
//
// ----------------------------------------------------------------------------
//    This file is part of ambed.
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
#include "ctimepoint.h"
#include "cusb3xxxinterface.h"
#include "cvocodecchannel.h"
#include "cambeserver.h"

// queues ID
#define QUEUE_CHANNEL       0
#define QUEUE_SPEECH        1

////////////////////////////////////////////////////////////////////////////////////////
// constructor

CUsb3xxxInterface::CUsb3xxxInterface(uint32 uiVid, uint32 uiPid, const char *szDeviceName, const char *szDeviceSerial)
{
    m_FtdiHandle = NULL;
    m_iDeviceFifoLevel = 0;
    m_iActiveQueue = QUEUE_CHANNEL;
    m_uiVid = uiVid;
    m_uiPid = uiPid;
    ::strcpy(m_szDeviceName, szDeviceName);
    ::strcpy(m_szDeviceSerial, szDeviceSerial);
}

////////////////////////////////////////////////////////////////////////////////////////
// destructor

CUsb3xxxInterface::~CUsb3xxxInterface()
{
}

////////////////////////////////////////////////////////////////////////////////////////
// initialization

bool CUsb3xxxInterface::Init(void)
{
    bool ok = true;
    
    // open USB device
    std::cout << "Opening " << m_szDeviceName << ":" << m_szDeviceSerial << " device" << std::endl;
    if ( ok &= OpenDevice() )
    {
        // reset
    	//std::cout << "Reseting " << m_szDeviceName << "device" << std::endl;
        if ( ok &= ResetDevice() )
        {
            // read version
    		//std::cout << "Reading " << m_szDeviceName << " device version" << std::endl;
            if ( ok &= ReadDeviceVersion() )
            {
                // send configuration packet(s)
    			//std::cout << "Configuring " << m_szDeviceName << " device" << std::endl;
                ok &= DisableParity();
                ok &= ConfigureDevice();
            }
        }
    }
    std::cout << std::endl;
  
    // base class
    if ( ok )
    {
        ok &= CVocodecInterface::Init();
    }
    
    // done
    return ok;
}


////////////////////////////////////////////////////////////////////////////////////////
// task

void CUsb3xxxInterface::Task(void)
{
    CBuffer         Buffer;
    int             iCh;
    CPacketQueue    *Queue;
    CVocodecChannel *Channel;
    CAmbePacket     AmbePacket;
    CVoicePacket    VoicePacket;
    bool            done;
    
    // TODO :
    //      preserve packets PIDs, so the transcoded CAmbePacket returned
    //      to CStream client is garantied to have the same PID
    //      than the corresponding incoming packet
    
    // process the device incoming packet
    // get all packets from device and push them
    // to the relevant clients queues
    if ( ReadBuffer(&Buffer) )
    {
        if ( IsValidSpeechPacket(Buffer, &iCh, &VoicePacket) )
        {
#ifdef DEBUG_DUMPFILE
            g_AmbeServer.m_DebugFile << m_szDeviceName << "\t" << "->Sp" << iCh << std::endl; std::cout.flush();
#endif
            // update fifo status
            m_iDeviceFifoLevel = MAX(m_iDeviceFifoLevel-1, 0);
            
            // push back to relevant channel voice queue
            // our incoming channel packet has now been through the decoder
            // find the coupled channel encoder and push to it's queue
            // this is were the DVSI enc-dec channel crossover take place
            Channel = GetChannelWithChannelIn(iCh);
            if ( Channel != NULL )
            {
                Queue = Channel->GetVoiceQueue();
                CVoicePacket *clone = new CVoicePacket(VoicePacket);
                clone->ApplyGain(Channel->GetSpeechGain());
                Queue->push(clone);
                Channel->ReleaseVoiceQueue();
            }
        }
        else if ( IsValidChannelPacket(Buffer, &iCh, &AmbePacket) )
        {
#ifdef DEBUG_DUMPFILE
            g_AmbeServer.m_DebugFile << m_szDeviceName << "\t" << "->Ch" << iCh << std::endl; std::cout.flush();
#endif
            // update fifo status
            m_iDeviceFifoLevel = MAX(m_iDeviceFifoLevel-1, 0);
            
            // push back to relevant channel outcoming queue
            Channel = GetChannelWithChannelOut(iCh);
            if ( Channel != NULL )
            {
                Queue = Channel->GetPacketQueueOut();
                CAmbePacket *clone = new CAmbePacket(AmbePacket);
                Queue->push(clone);
                Channel->ReleasePacketQueueOut();
            }
        }
    }
    
    // process the streams (channels) incoming queue
    // make sure that packets from different channels
    // are interlaced so to keep the device fifo busy
    do
    {
        done = true;
        for ( int i = 0; i < m_Channels.size(); i++)
        {
            // get channel
            Channel = m_Channels[i];
            
            // any packet in voice queue ?
            if ( Channel->IsInterfaceOut(this) )
            {
                Queue = Channel->GetVoiceQueue();
                if ( !Queue->empty() )
                {
                    // get packet
                    CVoicePacket *Packet = (CVoicePacket *)Queue->front();
                    Queue->pop();
                    // post it to relevant channel encoder
                    Packet->SetChannel(Channel->GetChannelOut());
                    m_SpeechQueue.push(Packet);
                    // done
                    done = false;
                }
                Channel->ReleaseVoiceQueue();
            }
            
            // any packet in ambe queue for us ?
            if ( Channel->IsInterfaceIn(this) )
            {
                Queue = Channel->GetPacketQueueIn();
                if ( !Queue->empty() )
                {
                    // get packet
                    CAmbePacket *Packet = (CAmbePacket *)Queue->front();
                    Queue->pop();
                    // post it to relevant channel decoder
                    Packet->SetChannel(Channel->GetChannelIn());
                    m_ChannelQueue.push(Packet);
                    // done
                    done = false;
                }
                Channel->ReleasePacketQueueIn();
            }
        }
    } while (!done);
    
    // process device incoming queues
    // interlace speech and channels packets
    // and make sure that the fifo is always
    // fed.
    unsigned long iQueueLevel = m_SpeechQueue.size() + m_ChannelQueue.size();
    if ( ((m_iDeviceFifoLevel == 0) && (iQueueLevel >= 2)) || (m_iDeviceFifoLevel == 1) )
    {
        if ( m_iActiveQueue == QUEUE_CHANNEL )
        {
            // post next channel packet
            if ( !m_ChannelQueue.empty() )
            {
                // get packet
                CAmbePacket *Packet = (CAmbePacket *)m_ChannelQueue.front();
                m_ChannelQueue.pop();
                //Post it
                EncodeChannelPacket(&Buffer, Packet->GetChannel(), Packet);
                WriteBuffer(Buffer);
                m_iDeviceFifoLevel++;
                // and delete it
#ifdef DEBUG_DUMPFILE
                g_AmbeServer.m_DebugFile << m_szDeviceName << "\t" << "Ch" << Packet->GetChannel() << "->" << std::endl; std::cout.flush();
#endif
                delete Packet;
           }
            // and interlace
            m_iActiveQueue = QUEUE_SPEECH;
        }
        else
        {
            // post next speech packet
            if ( !m_SpeechQueue.empty() )
            {
                // get packet
                CVoicePacket *Packet = (CVoicePacket *)m_SpeechQueue.front();
                m_SpeechQueue.pop();
                //Post it
                EncodeSpeechPacket(&Buffer, Packet->GetChannel(), Packet);
                WriteBuffer(Buffer);
                m_iDeviceFifoLevel++;
                // and delete it
#ifdef DEBUG_DUMPFILE
                g_AmbeServer.m_DebugFile << m_szDeviceName << "\t" << "Sp" << Packet->GetChannel() << "->" << std::endl; std::cout.flush();
#endif
                delete Packet;
            }
            // and interlace
            m_iActiveQueue = QUEUE_CHANNEL;
        }
    }
    
    
    // and wait a bit
    CTimePoint::TaskSleepFor(2);
}


////////////////////////////////////////////////////////////////////////////////////////
// low level

bool CUsb3xxxInterface::ReadDeviceVersion(void)
{
    bool ok = false;
    int i, len;
    char rxpacket[128];
    char txpacket[8] =
    {
        PKT_HEADER,
        0,
        4,
        PKT_CONTROL,
        PKT_PRODID,
        PKT_VERSTRING,
        PKT_PARITYBYTE,
        4 ^ PKT_CONTROL ^ PKT_PRODID ^ PKT_VERSTRING ^ PKT_PARITYBYTE
    };

    // write packet
    if ( FTDI_write_packet(m_FtdiHandle, txpacket, sizeof(txpacket)) )
    {
        // read reply
        len = FTDI_read_packet( m_FtdiHandle, rxpacket, sizeof(rxpacket) ) - 4;
        ok = (len != 0);
        //we succeed in reading a packet, print it out
        std::cout << "ReadDeviceVersion : ";
        for ( i = 4; i < len+4 ; i++ )
        {
            std::cout << (char)(rxpacket[i] & 0x00ff);
        }
        std::cout << std::endl;
    }
    return ok;
}

bool CUsb3xxxInterface::DisableParity(void)
{
    bool ok = false;
    int len;
    char rxpacket[16];
    char txpacket[8] =
    {
        PKT_HEADER,
        0,
        4,
        PKT_CONTROL,
        PKT_PARITYMODE,0x00,
        PKT_PARITYBYTE,
        4 ^ PKT_CONTROL ^ PKT_PARITYMODE ^ 0x00 ^ PKT_PARITYBYTE
    };

    // write packet
    if ( FTDI_write_packet(m_FtdiHandle, txpacket, sizeof(txpacket)) )
    {
        // read reply
        len = FTDI_read_packet( m_FtdiHandle, rxpacket, sizeof(rxpacket) ) - 4;
        ok = ((len == 2) && (rxpacket[4] == PKT_PARITYMODE) &&(rxpacket[5] == 0x00) );
    }
    return ok;
}

bool CUsb3xxxInterface::ConfigureChannel(uint8 pkt_ch, const uint8 *pkt_ratep, int in_gain, int out_gain)
{
    bool ok = false;
    int len;
    char rxpacket[64];
    char txpacket[] =
    {
        PKT_HEADER,
        0,
        33,
        PKT_CONTROL,
        0x00,
        PKT_ECMODE, 0x00,0x00,
        PKT_DCMODE, 0x00,0x00,
        PKT_COMPAND,0x00,
        PKT_RATEP,  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        PKT_CHANFMT,0x00,0x00,
        PKT_SPCHFMT,0x00,0x00,
        PKT_GAIN,   0x00,0x00,
        PKT_INIT,   0x03
    };
    
    // update packet content
    txpacket[4] = pkt_ch;
    :: memcpy(&(txpacket[14]), pkt_ratep, 12);
    txpacket[33] = (uint8)(signed char)in_gain;
    txpacket[34] = (uint8)(signed char)out_gain;
    
    // write packet
    if ( FTDI_write_packet(m_FtdiHandle, txpacket, sizeof(txpacket)) )
    {
        // read reply
        len = FTDI_read_packet( m_FtdiHandle, rxpacket, sizeof(rxpacket) ) - 4;
        ok = ((len == 18) && (rxpacket[20] == PKT_INIT) &&(rxpacket[21] == 0x00) );
    }
    return ok;
    
}


////////////////////////////////////////////////////////////////////////////////////////
// io level

bool CUsb3xxxInterface::ReadBuffer(CBuffer *buffer)
{
    bool ok = false;
    int n;
   
    // any byte in tx queue ?
    if  ( FT_GetQueueStatus(m_FtdiHandle, (LPDWORD)&n) == FT_OK )
    {
        //if ( (FT_GetQueueStatus(m_FtdiHandle, (LPDWORD)&n) == FT_OK) && (n != 0) )
        if ( n != 0 )
        {
            buffer->clear();
            buffer->resize(USB3XXX_MAXPACKETSIZE);
            n = FTDI_read_packet(m_FtdiHandle, (char *)buffer->data(), USB3XXX_MAXPACKETSIZE);
            buffer->resize(n);
            ok = (n != 0);
        }
    }
    return ok;
}

bool CUsb3xxxInterface::WriteBuffer(const CBuffer &buffer)
{
    return FTDI_write_packet(m_FtdiHandle, (const char *)buffer.data(), (int)buffer.size());
}

int CUsb3xxxInterface::FTDI_read_packet(FT_HANDLE ftHandle, char *pkt, int maxlen)
{
    int plen;
    
    // first read 4 bytes header
    if ( FTDI_read_bytes(ftHandle, pkt, 4) )
    {
        // get payload length
        plen = (pkt[1] & 0x00ff);
        plen <<= 8;
        plen += (pkt[2] & 0x00ff);
        // check buffer length
        if (plen+4 > maxlen)
        {
            std::cout << "FTDI_read_packet supplied buffer is not large enough for packet" << std::endl;
            FT_Purge(ftHandle, FT_PURGE_RX);
            return 0;
        }
        // and get payload
        if ( FTDI_read_bytes(ftHandle, &pkt[4], plen) )
        {
            return plen+4;
        }
    }
    return 0;
}

bool CUsb3xxxInterface::FTDI_read_bytes(FT_HANDLE ftHandle, char *buffer, int len)
{
    // this relies on FT_SetTimouts() mechanism
    int n;
    bool ok = false;
    
    ok = (FT_Read(ftHandle, (LPVOID)buffer, len, (LPDWORD)&n) == FT_OK) && (n == len);
    if ( !ok )
    {
        //FT_Purge(ftHandle, FT_PURGE_RX);
        std::cout << "FTDI_read_bytes(" << len << ") failed : " << n << std::endl;
    }
    
    return ok;
}

bool CUsb3xxxInterface::FTDI_write_packet(FT_HANDLE ft_handle, const char *pkt, int len)
{
    FT_STATUS ftStatus;
    bool  ok = true;
    int   nwritten;
    
    if ( len > 0 )
    {
        ftStatus = FT_Write(m_FtdiHandle, (LPVOID *)pkt, (DWORD)len, (LPDWORD)&nwritten);
        ok = (ftStatus == FT_OK) && (len == nwritten);
        if ( !ok )
        {
            FTDI_Error((char *)"FT_Write", ftStatus);
        }
    }
    return ok;
}

////////////////////////////////////////////////////////////////////////////////////////
// error reporting

void CUsb3xxxInterface::FTDI_Error(char *func_string, FT_STATUS ftStatus)
{
    std::cout << "FTDI function " << func_string << " error " << (int)ftStatus << std::endl;
}
