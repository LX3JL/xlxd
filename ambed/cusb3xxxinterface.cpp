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
    m_uiVid = uiVid;
    m_uiPid = uiPid;
    ::strcpy(m_szDeviceName, szDeviceName);
    ::strcpy(m_szDeviceSerial, szDeviceSerial);
    m_iSpeechFifolLevel = 0;
    m_iChannelFifolLevel = 0;
}

////////////////////////////////////////////////////////////////////////////////////////
// destructor

CUsb3xxxInterface::~CUsb3xxxInterface()
{
    // delete m_SpeechQueues
    for ( int i = 0; i < m_SpeechQueues.size(); i++ )
    {
        delete m_SpeechQueues[i];
    }
    m_SpeechQueues.clear();
    
    // delete m_ChannelQueues
    for ( int i = 0; i < m_ChannelQueues.size(); i++ )
    {
        delete m_ChannelQueues[i];
    }
    m_ChannelQueues.clear();
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
  
    // create our queues
    for ( int i = 0; i < GetNbChannels(); i++ )
    {
        m_SpeechQueues.push_back(new CPacketQueue);
        m_ChannelQueues.push_back(new CPacketQueue);
    }
    
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
            // update fifo level
            // as we get a speech packet, it means that the device
            // channel fifo input decreased by 1
            m_iChannelFifolLevel = MAX(0, m_iChannelFifolLevel-1);
            
            // push back to relevant channel voice queue
            // our incoming channel packet has now been through the decoder
            // find the coupled channel encoder and push to it's queue
            // this is were the DVSI enc-dec channel crossover take place
            Channel = GetChannelWithChannelIn(iCh);
            if ( Channel != NULL )
            {
                CVoicePacket *clone1 = new CVoicePacket(VoicePacket);
                clone1->ApplyGain(Channel->GetSpeechGain());
                CVoicePacket *clone2 = new CVoicePacket(*clone1);

                Queue = Channel->GetVoiceQueue1();
                Queue->push(clone1);
                Channel->ReleaseVoiceQueue1();

                Queue = Channel->GetVoiceQueue2();
                Queue->push(clone2);
                Channel->ReleaseVoiceQueue2();
            }
        }
        else if ( IsValidChannelPacket(Buffer, &iCh, &AmbePacket) )
        {
            // update fifo level
            // as we get a channel packet, it means that the device
            // speech fifo input decreased by 1
            m_iSpeechFifolLevel = MAX(0, m_iSpeechFifolLevel-1);

            // push back to relevant channel outcoming queue
            // we are done with this packet transcoding
            // it's final step
            Channel = GetChannelWithChannelOut(iCh);
            if ( Channel != NULL )
            {
                CAmbePacket *clone = new CAmbePacket(AmbePacket);

                if ( Channel->IsInterfaceOut1(this) )
                {
                    Queue = Channel->GetPacketQueueOut1();
                    Queue->push(clone);
                    Channel->ReleasePacketQueueOut1();
                }
                else if ( Channel->IsInterfaceOut2(this) )
                {
                    Queue = Channel->GetPacketQueueOut2();
                    Queue->push(clone);
                    Channel->ReleasePacketQueueOut2();
                }
            }
        }
    }
    
    // process the streams (channels) incoming queue
    // make sure that packets from different channels
    // are interlaced so to keep the device fifo busy
    do
    {
        done = true;
        for ( int i = 0; i < GetNbChannels(); i++ )
        {
            // get active outgoing channel for interface channel
            Channel = GetChannelWithChannelOut(i);
            
            // any packet in voice queue 1 ?
            if ( Channel != NULL && Channel->IsInterfaceOut1(this) )
            {
                Queue = Channel->GetVoiceQueue1();
                if ( !Queue->empty() )
                {
                    // get packet
                    CVoicePacket *Packet = (CVoicePacket *)Queue->front();
                    Queue->pop();
                    // this is second step of transcoding
                    // we just received from hardware a decoded speech packet
                    // post it to relevant channel encoder
                    int i = Channel->GetChannelOut1();
                    Packet->SetChannel(i);
                    m_SpeechQueues[i]->push(Packet);
                    // done
                    done = false;
                }
                Channel->ReleaseVoiceQueue1();
            }

            // any packet in voice queue 2 ?
            if ( Channel != NULL && Channel->IsInterfaceOut2(this) )
            {
                Queue = Channel->GetVoiceQueue2();
                if ( !Queue->empty() )
                {
                    // get packet
                    CVoicePacket *Packet = (CVoicePacket *)Queue->front();
                    Queue->pop();
                    // this is second step of transcoding
                    // we just received from hardware a decoded speech packet
                    // post it to relevant channel encoder
                    int i = Channel->GetChannelOut2();
                    Packet->SetChannel(i);
                    m_SpeechQueues[i]->push(Packet);
                    // done
                    done = false;
                }
                Channel->ReleaseVoiceQueue2();
            }
            
            // get active incoming channel for interface channel
            Channel = GetChannelWithChannelIn(i);

            // any packet in ambe queue for us ?
            if ( Channel != NULL && Channel->IsInterfaceIn(this) )
            {
                Queue = Channel->GetPacketQueueIn();
                if ( !Queue->empty() )
                {
                    // get packet
                    CAmbePacket *Packet = (CAmbePacket *)Queue->front();
                    Queue->pop();
                    // this is first step of transcoding
                    // a fresh new packet to be transcoded is showing up
                    // post it to relevant channel decoder
                    int i = Channel->GetChannelIn();
                    Packet->SetChannel(i);
                    m_ChannelQueues[i]->push(Packet);
                    // done
                    done = false;
                }
                Channel->ReleasePacketQueueIn();
            }
        }
    } while (!done);
    
    // process device incoming queues (aka to device)
    // interlace speech and channels packets
    // and post to final device queue
    do
    {
        done = true;
        // loop on all channels
        for ( int i = 0; i < GetNbChannels(); i++ )
        {
            // speech
            if ( !m_SpeechQueues[i]->empty() )
            {
                // get packet
                CPacket *Packet = m_SpeechQueues[i]->front();
                m_SpeechQueues[i]->pop();
                // and push to device queue
                m_DeviceQueue.push(Packet);
                // next
                done = false;
            }
            // ambe
            if ( !m_ChannelQueues[i]->empty() )
            {
                // get packet
                CPacket *Packet = m_ChannelQueues[i]->front();
                m_ChannelQueues[i]->pop();
                // and push to device queue
                m_DeviceQueue.push(Packet);
                // done = false;
            }
        }
        
    } while (!done);
    
    // process device queue to feed hardware
    // make sure that device fifo is fed all the time
    int fifoSize = GetDeviceFifoSize();
    do
    {
        done = true;
        // if device fifo level is zero (device idle)
        // wait that at least 3 packets are in incoming
        // queue before restarting
        if ( ((m_iSpeechFifolLevel+m_iChannelFifolLevel) > 0) || (m_DeviceQueue.size() >= (fifoSize+1)) )
        {
            // any packet to send ?
            if ( m_DeviceQueue.size() > 0 )
            {
                // yes, get it
                CPacket *Packet = m_DeviceQueue.front();
                if ( Packet->IsVoice() && (m_iSpeechFifolLevel < fifoSize) )
                {
                    // encode & post
                    EncodeSpeechPacket(&Buffer, Packet->GetChannel(), (CVoicePacket *)Packet);
                    WriteBuffer(Buffer);
                    // remove from queue
                    m_DeviceQueue.pop();
                    // and delete it
                    delete Packet;
                    // update fifo level
                    m_iSpeechFifolLevel++;
                    // next
                    done = false;
#ifdef DEBUG_DUMPFILE
                    g_AmbeServer.m_DebugFile << m_szDeviceName << "\t" << "Sp" << Packet->GetChannel() << "->" << std::endl; std::cout.flush();
#endif
                }
                else if ( Packet->IsAmbe() && (m_iChannelFifolLevel < fifoSize) )
                {
                    // encode & post
                    EncodeChannelPacket(&Buffer, Packet->GetChannel(), (CAmbePacket *)Packet);
                    WriteBuffer(Buffer);
                    // remove from queue
                    m_DeviceQueue.pop();
                    // and delete it
                    delete Packet;
                    // update fifo level
                    m_iChannelFifolLevel++;
                    // next
                    done = false;
#ifdef DEBUG_DUMPFILE
                    g_AmbeServer.m_DebugFile << m_szDeviceName << "\t" << "Ch" << Packet->GetChannel() << "->" << std::endl; std::cout.flush();
#endif
                }

            }
        }
    } while (!done);
    
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
