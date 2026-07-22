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

// timeout
#define DEVICE_TIMEOUT      600     // in ms

////////////////////////////////////////////////////////////////////////////////////////
// constructor

CUsb3xxxInterface::CUsb3xxxInterface(uint32 uiVid, uint32 uiPid, const char *szDeviceName, const char *szDeviceSerial)
{
    m_FtdiHandle = NULL;
    m_uiVid = uiVid;
    m_uiPid = uiPid;
    ::strncpy(m_szDeviceName, szDeviceName, sizeof(m_szDeviceName) - 1);
    m_szDeviceName[sizeof(m_szDeviceName) - 1] = '\0';
    ::strncpy(m_szDeviceSerial, szDeviceSerial, sizeof(m_szDeviceSerial) - 1);
    m_szDeviceSerial[sizeof(m_szDeviceSerial) - 1] = '\0';
    m_iSpeechFifolLevel = 0;
    m_iChannelFifolLevel = 0;
    m_nRxBufLen = 0;
}

////////////////////////////////////////////////////////////////////////////////////////
// destructor

CUsb3xxxInterface::~CUsb3xxxInterface()
{
    // stop thread first
    m_bStopThread = true;
    if ( m_pThread != NULL )
    {
        m_pThread->join();
        delete m_pThread;
        m_pThread = NULL;
    }
    
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
  
    // create queues and start thread only if device init succeeded
    if ( ok )
    {
        for ( int i = 0; i < GetNbChannels(); i++ )
        {
            m_SpeechQueues.push_back(new CPacketQueue);
            m_ChannelQueues.push_back(new CPacketQueue);
        }
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
    
    // Phase 1: Batched read from USB device
    // Only query hardware if we expect a response (FIFO has pending packets)
    // or have partial data in the carry-over buffer
    if ( m_iSpeechFifolLevel > 0 || m_iChannelFifolLevel > 0 || m_nRxBufLen > 0 )
    {
        DWORD nAvail = 0;
        if ( FT_GetQueueStatus(m_FtdiHandle, &nAvail) == FT_OK && nAvail > 0 )
        {
            int nToRead = (int)nAvail;
            if ( m_nRxBufLen + nToRead > (int)sizeof(m_rawRxBuf) )
                nToRead = (int)sizeof(m_rawRxBuf) - m_nRxBufLen;

            if ( nToRead > 0 )
            {
                DWORD nRead = 0;
                if ( FT_Read(m_FtdiHandle, (LPVOID)&m_rawRxBuf[m_nRxBufLen], nToRead, &nRead) == FT_OK )
                {
                    m_nRxBufLen += (int)nRead;
                }
            }
        }

        // Parse complete DVSI packets from carry-over buffer
        int pos = 0;
        while ( pos < m_nRxBufLen )
        {
            // need at least 4 bytes for header
            if ( m_nRxBufLen - pos < 4 )
                break;

            // validate start byte and type byte
            if ( (uint8)m_rawRxBuf[pos] != PKT_HEADER )
            {
                std::cout << "Batched read: invalid header byte, purging" << std::endl;
                FT_Purge(m_FtdiHandle, FT_PURGE_RX);
                m_nRxBufLen = 0;
                pos = 0;
                break;
            }
            uint8 pktType = (uint8)m_rawRxBuf[pos + 3];
            if ( pktType != PKT_CONTROL && pktType != PKT_CHANNEL && pktType != PKT_SPEECH )
            {
                std::cout << "Batched read: invalid packet type, purging" << std::endl;
                FT_Purge(m_FtdiHandle, FT_PURGE_RX);
                m_nRxBufLen = 0;
                pos = 0;
                break;
            }

            // get payload length and total packet size
            int plen = ((m_rawRxBuf[pos + 1] & 0xFF) << 8) | (m_rawRxBuf[pos + 2] & 0xFF);
            if ( plen > USB3XXX_MAXPACKETSIZE - 4 )
            {
                std::cout << "Batched read: implausible payload length " << plen << ", purging" << std::endl;
                FT_Purge(m_FtdiHandle, FT_PURGE_RX);
                m_nRxBufLen = 0;
                pos = 0;
                break;
            }
            int pktTotal = 4 + plen;

            // wait for complete packet
            if ( m_nRxBufLen - pos < pktTotal )
                break;

            // extract complete packet into Buffer and dispatch
            Buffer.clear();
            Buffer.resize(pktTotal);
            ::memcpy(Buffer.data(), &m_rawRxBuf[pos], pktTotal);
            pos += pktTotal;

            if ( IsValidSpeechPacket(Buffer, &iCh, &VoicePacket) )
            {
                m_iChannelFifolLevel = MAX(0, m_iChannelFifolLevel-1);

                Channel = GetChannelWithChannelIn(iCh);
                if ( Channel != NULL )
                {
                    CVoicePacket *clone = new CVoicePacket(VoicePacket);
                    Channel->ProcessSignal(*clone);
                    Queue = Channel->GetVoiceQueue();
                    Queue->push(clone);
                    Channel->ReleaseVoiceQueue();
                }
            }
            else if ( IsValidChannelPacket(Buffer, &iCh, &AmbePacket) )
            {
                m_iSpeechFifolLevel = MAX(0, m_iSpeechFifolLevel-1);

                Channel = GetChannelWithChannelOut(iCh);
                if ( Channel != NULL )
                {
                    // Restore the PID that was on the corresponding input
                    // packet — the hardware response carries channel and
                    // codec data but not the original pid, so we recover
                    // it from the per-channel FIFO populated when the
                    // input packet was consumed (see input-side push
                    // below in this same Task() loop). FIFO ordering
                    // matches because the DVStick processes packets
                    // sequentially per physical channel.
                    CAmbePacket *clone = new CAmbePacket(AmbePacket);
                    clone->SetPid(Channel->PopPid());
                    Queue = Channel->GetPacketQueueOut();
                    Queue->push(clone);
                    Channel->ReleasePacketQueueOut();
                }
            }
        }

        // move residual bytes to front of buffer
        if ( pos > 0 && pos < m_nRxBufLen )
        {
            ::memmove(m_rawRxBuf, &m_rawRxBuf[pos], m_nRxBufLen - pos);
            m_nRxBufLen -= pos;
        }
        else if ( pos >= m_nRxBufLen )
        {
            m_nRxBufLen = 0;
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
                    // this is second step of transcoding
                    // we just received from hardware a decoded speech packet
                    // post it to relevant channel encoder
                    int chIdx = Channel->GetChannelOut();
                    Packet->SetChannel(chIdx);
                    m_SpeechQueues[chIdx]->push(Packet);
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
                    // PID-preservation push: capture the pid before the
                    // packet heads into the USB write path (where it
                    // gets freed). The matching pop happens above when
                    // the encoder-side hardware response arrives, so
                    // the pid round-trips correctly via the per-channel
                    // FIFO instead of via packet bytes (which the DVStick
                    // protocol doesn't preserve).
                    Channel->PushPid(Packet->GetPid());
                    // this is first step of transcoding
                    // a fresh new packet to be transcoded is showing up
                    // post it to relevant channel decoder
                    int chIdx = Channel->GetChannelIn();
                    Packet->SetChannel(chIdx);
                    m_ChannelQueues[chIdx]->push(Packet);
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
    
    // Phase 4: Batched write to USB device
    // Run timeout checks once per iteration
    int fifoSize = GetDeviceFifoSize();
    if ( (m_iSpeechFifolLevel > 0) && (m_SpeechFifoLevelTimeout.DurationSinceNow() >= (DEVICE_TIMEOUT/1000.0f)) )
    {
        std::cout << "Reseting " << m_szDeviceName << ":" << m_szDeviceSerial << " device fifo level due to timeout" << std::endl;
        m_iSpeechFifolLevel = 0;
        // Purge any stale data from hardware and carry-over buffer
        FT_Purge(m_FtdiHandle, FT_PURGE_RX);
        m_nRxBufLen = 0;
        if ( CheckIfDeviceNeedsReOpen() )
        {
            m_iChannelFifolLevel = 0;
        }
    }
    if ( (m_iChannelFifolLevel > 0) && (m_ChannelFifoLevelTimeout.DurationSinceNow() >= (DEVICE_TIMEOUT/1000.0f)) )
    {
        std::cout << "Reseting " << m_szDeviceName << ":" << m_szDeviceSerial << " device fifo level due to timeout" << std::endl;
        m_iChannelFifolLevel = 0;
        FT_Purge(m_FtdiHandle, FT_PURGE_RX);
        m_nRxBufLen = 0;
        if ( CheckIfDeviceNeedsReOpen() )
        {
            m_iSpeechFifolLevel = 0;
        }
    }

    // Gather packets into a single TX batch
    CBuffer txBatch;
    int speechToSend = 0;
    int channelToSend = 0;
    bool gathering = true;
    while ( gathering && !m_DeviceQueue.empty() )
    {
        CPacket *Packet = m_DeviceQueue.front();

        // check if channel is still open
        Channel = NULL;
        if ( Packet->IsVoice() )
            Channel = GetChannelWithChannelOut(Packet->GetChannel());
        else if ( Packet->IsAmbe() )
            Channel = GetChannelWithChannelIn(Packet->GetChannel());
        if ( (Channel != NULL) && !Channel->IsOpen() )
        {
            m_DeviceQueue.pop();
            delete Packet;
            continue;
        }

        if ( Packet->IsVoice() )
        {
            if ( m_iSpeechFifolLevel + speechToSend < fifoSize )
            {
#ifdef DEBUG_DUMPFILE
                int dbgCh = Packet->GetChannel();
#endif
                EncodeSpeechPacket(&Buffer, Packet->GetChannel(), (CVoicePacket *)Packet);
                txBatch.Append(Buffer.data(), (int)Buffer.size());
                m_DeviceQueue.pop();
                delete Packet;
                speechToSend++;
#ifdef DEBUG_DUMPFILE
                g_AmbeServer.m_DebugFile << m_szDeviceName << "\t" << "Sp" << dbgCh << "->" << std::endl; std::cout.flush();
#endif
            }
            else
            {
                gathering = false;
            }
        }
        else if ( Packet->IsAmbe() )
        {
            if ( m_iChannelFifolLevel + channelToSend < fifoSize )
            {
#ifdef DEBUG_DUMPFILE
                int dbgCh = Packet->GetChannel();
#endif
                EncodeChannelPacket(&Buffer, Packet->GetChannel(), (CAmbePacket *)Packet);
                txBatch.Append(Buffer.data(), (int)Buffer.size());
                m_DeviceQueue.pop();
                delete Packet;
                channelToSend++;
#ifdef DEBUG_DUMPFILE
                g_AmbeServer.m_DebugFile << m_szDeviceName << "\t" << "Ch" << dbgCh << "->" << std::endl; std::cout.flush();
#endif
            }
            else
            {
                gathering = false;
            }
        }
        else
        {
            m_DeviceQueue.pop();
            delete Packet;
        }
    }

    // Single write for entire batch
    if ( txBatch.size() > 0 )
    {
        WriteBuffer(txBatch);
        m_iSpeechFifolLevel += speechToSend;
        m_iChannelFifolLevel += channelToSend;
        if ( speechToSend > 0 )
            m_SpeechFifoLevelTimeout.Now();
        if ( channelToSend > 0 )
            m_ChannelFifoLevelTimeout.Now();
    }
    
    // Adaptive sleep: 1ms when work is pending, 10ms when idle
    bool hasWork = (m_iSpeechFifolLevel > 0) || (m_iChannelFifolLevel > 0) ||
                   !m_DeviceQueue.empty() || (m_nRxBufLen > 0);
    if ( !hasWork )
    {
        for ( int i = 0; i < GetNbChannels() && !hasWork; i++ )
        {
            hasWork = !m_SpeechQueues[i]->empty() || !m_ChannelQueues[i]->empty();
        }
    }
    CTimePoint::TaskSleepFor(hasWork ? 1 : 10);
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
        ok = (len > 0);
        //we succeed in reading a packet, print it out
        std::cout << "ReadDeviceVersion : ";
        for ( i = 5; (i < len+4) && (rxpacket[i] != 0x00); i++ )
        {
            std::cout << (char)(rxpacket[i] & 0x00ff);
        }
        std::cout << " ";
        for ( i = i+2; (i < len+4) && (rxpacket[i] != 0x00); i++ )
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

bool CUsb3xxxInterface::CheckIfDeviceNeedsReOpen(void)
{
    bool ok = false;
    int len;
    char rxpacket[64];
    char txpacket[5] =
    {
        PKT_HEADER,
        0,
        1,
        PKT_CONTROL,
        PKT_PRODID
    };

    // write packet
    if ( FTDI_write_packet(m_FtdiHandle, txpacket, sizeof(txpacket)) )
    {
        // read reply
        len = FTDI_read_packet( m_FtdiHandle, rxpacket, sizeof(rxpacket) ) - 4;
        ok = ((len > 0) && (rxpacket[3] == PKT_CONTROL) && (rxpacket[4] == PKT_PRODID));
    }

    if ( !ok )
    {
        std::cout << "Device " << m_szDeviceName << ":" << m_szDeviceSerial << " is unresponsive, trying to re-open it..." << std::endl;
        FT_Close(m_FtdiHandle);
        CTimePoint::TaskSleepFor(100);
        if ( OpenDevice() )
        {
            if ( ResetDevice() )
            {
                DisableParity();
                ConfigureDevice();
            }
        }
    }

    return !ok;
}


////////////////////////////////////////////////////////////////////////////////////////
// io level

bool CUsb3xxxInterface::ReadBuffer(CBuffer *buffer)
{
    bool ok = false;
    DWORD n;

    // any byte in tx queue ?
    if  ( FT_GetQueueStatus(m_FtdiHandle, &n) == FT_OK )
    {
        if ( n != 0 )
        {
            buffer->clear();
            buffer->resize(USB3XXX_MAXPACKETSIZE);
            int len = FTDI_read_packet(m_FtdiHandle, (char *)buffer->data(), USB3XXX_MAXPACKETSIZE);
            buffer->resize(len);
            ok = (len != 0);
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
        // ensure we got a valid packet header
        if ( (pkt[0] != PKT_HEADER) || ((pkt[3] != PKT_CONTROL) && (pkt[3] != PKT_CHANNEL) && (pkt[3] != PKT_SPEECH)) )
        {
            std::cout << "FTDI_read_packet invalid packet header" << std::endl;
            FT_Purge(ftHandle, FT_PURGE_RX);
            return 0;
        }
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
    DWORD n;
    bool ok = false;

    ok = (FT_Read(ftHandle, (LPVOID)buffer, len, &n) == FT_OK) && ((int)n == len);
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
    DWORD nwritten;

    if ( len > 0 )
    {
        ftStatus = FT_Write(ft_handle, (LPVOID)pkt, (DWORD)len, &nwritten);
        ok = (ftStatus == FT_OK) && ((int)nwritten == len);
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
