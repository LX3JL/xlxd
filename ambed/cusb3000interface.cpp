//
//  cusb3000interface.cpp
//  ambed
//
//  Created by Jean-Luc Deltombe (LX3JL) on 21/08/2017.
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
#include "ctimepoint.h"
#include "cambepacket.h"
#include "cusb3000interface.h"
#include "cvocodecs.h"

////////////////////////////////////////////////////////////////////////////////////////
// configuration:
//
//      PKT_CHANNEL0 = HYBRID


////////////////////////////////////////////////////////////////////////////////////////
// constructor

CUsb3000Interface::CUsb3000Interface(uint32 uiVid, uint32 uiPid, const char *szDeviceName, const char *szDeviceSerial)
: CUsb3xxxInterface(uiVid, uiPid, szDeviceName, szDeviceSerial)
{
    m_uiChCodec = CODEC_NONE;
}

////////////////////////////////////////////////////////////////////////////////////////
// initialization

bool CUsb3000Interface::Init(uint8 uiOddCodec)
{
    bool ok = true;
    
    // init the odd channel
    m_uiChCodec  = uiOddCodec;
    
    // base class
    ok &= CUsb3xxxInterface::Init();
    
    // do not create our channels now
    // this is delegated to caller (CVocodecs) as our channel
    // may be hybrids between 2 interfaces in case of odd n' of channel device)
    
    // done
    return ok;
}

////////////////////////////////////////////////////////////////////////////////////////
// manage Channels

uint8 CUsb3000Interface::GetChannelCodec(int iCh) const
{
    return (iCh == 0) ? m_uiChCodec : CODEC_NONE;
}

////////////////////////////////////////////////////////////////////////////////////////
// manage vocodec channels

CVocodecChannel *CUsb3000Interface::GetChannelWithChannelIn(int iCh)
{
    CVocodecChannel *Channel = NULL;
    bool done = false;
    for ( int i = 0; (i < m_Channels.size()) && !done; i++ )
    {
        if ( iCh == 0 )
        {
            if ( (m_Channels[i]->GetChannelIn() == iCh) && !(m_Channels[i]->IsInterfaceOut(this)) )
            {
                Channel = m_Channels[i];
                done = true;
            }
        }
    }
    return Channel;
}

CVocodecChannel *CUsb3000Interface::GetChannelWithChannelOut(int iCh)
{
    CVocodecChannel *Channel = NULL;
    bool done = false;
    for ( int i = 0; (i < m_Channels.size()) && !done; i++ )
    {
        if ( (m_Channels[i]->GetChannelOut() == iCh) && (m_Channels[i]->IsInterfaceOut(this)) )
        {
            Channel = m_Channels[i];
            done = true;
        }
    }
    return Channel;
}


////////////////////////////////////////////////////////////////////////////////////////
// decoder helper

bool CUsb3000Interface::IsValidChannelPacket(const CBuffer &buffer, int *ch, CAmbePacket *packet)
{
    bool valid = false;
    uint8 tag[] = { PKT_HEADER,0x00,0x0B,PKT_CHANNEL };
    
    if ( (buffer.size() == 15) && (buffer.Compare(tag, sizeof(tag)) == 0))
    {
        *ch = 0;
        packet->SetCodec(m_uiChCodec);
        packet->SetAmbe(&(buffer.data()[6]));
        valid = (*ch < GetNbChannels());
        //std::cout << "CHAN " << *ch << " " << buffer.size() << " " << (int)buffer[6] << std::endl;
    }
    return valid;
}

bool CUsb3000Interface::IsValidSpeechPacket(const CBuffer &buffer, int *ch, CVoicePacket *packet)
{
    bool valid = false;
    
    if ( (buffer.size() > 6) &&
        (buffer.data()[0] == PKT_HEADER) && (buffer.data()[3] == PKT_SPEECH) &&
        (buffer.data()[4] == PKT_SPEECHD) )
    {
        *ch = 0;
        packet->SetVoice(&(buffer.data()[6]), buffer.data()[5] * 2);
        valid = (*ch < GetNbChannels());
        //std::cout << "SPCH " << *ch << " " << buffer.size() << std::endl;
    }
    return valid;
}

////////////////////////////////////////////////////////////////////////////////////////
// encoder helpers

void CUsb3000Interface::EncodeChannelPacket(CBuffer *buffer, int ch, CAmbePacket *packet)
{
    uint size = (uint16)packet->GetAmbeSize() + 2;
    buffer->clear();
    buffer->Append((uint8)PKT_HEADER);
    buffer->Append((uint8)HIBYTE(size));
    buffer->Append((uint8)LOBYTE(size));
    buffer->Append((uint8)PKT_CHANNEL);
    buffer->Append((uint8)(PKT_CHAND));
    buffer->Append((uint8)(packet->GetAmbeSize()*8));
    buffer->Append(packet->GetAmbe(), packet->GetAmbeSize());
}

void CUsb3000Interface::EncodeSpeechPacket(CBuffer *buffer, int ch, CVoicePacket *packet)
{
    uint16 size = (uint16)packet->GetVoiceSize() + 2;
    buffer->clear();
    buffer->Append((uint8)PKT_HEADER);
    buffer->Append((uint8)HIBYTE(size));
    buffer->Append((uint8)LOBYTE(size));
    buffer->Append((uint8)PKT_SPEECH);
    buffer->Append((uint8)PKT_SPEECHD);
    buffer->Append((uint8)(packet->GetVoiceSize()/2));
    buffer->Append(packet->GetVoice(), packet->GetVoiceSize());
}


////////////////////////////////////////////////////////////////////////////////////////
// low level

bool CUsb3000Interface::OpenDevice(void)
{
    FT_STATUS ftStatus;
    int baudrate = 460800;
    
    //sets serial VID/PID for a Standard Device NOTE:  This is for legacy purposes only.  This can be ommitted.
    ftStatus = FT_SetVIDPID(m_uiVid, m_uiPid);
    if (ftStatus != FT_OK) {FTDI_Error((char *)"FT_SetVIDPID", ftStatus ); return false; }
    
    ftStatus = FT_OpenEx((PVOID)m_szDeviceSerial, FT_OPEN_BY_SERIAL_NUMBER, &m_FtdiHandle);
    if (ftStatus != FT_OK) { FTDI_Error((char *)"FT_OpenEx", ftStatus ); return false; }
    
    CTimePoint::TaskSleepFor(50);
    FT_Purge(m_FtdiHandle, FT_PURGE_RX | FT_PURGE_TX );
    CTimePoint::TaskSleepFor(50);
    
    ftStatus = FT_SetDataCharacteristics(m_FtdiHandle, FT_BITS_8, FT_STOP_BITS_1, FT_PARITY_NONE);
    if ( ftStatus != FT_OK ) { FTDI_Error((char *)"FT_SetDataCharacteristics", ftStatus ); return false; }
    
    ftStatus = FT_SetFlowControl(m_FtdiHandle, FT_FLOW_RTS_CTS, 0x11, 0x13);
    if (ftStatus != FT_OK) { FTDI_Error((char *)"FT_SetFlowControl", ftStatus ); return false; }
    
    ftStatus = FT_SetRts (m_FtdiHandle);
    if (ftStatus != FT_OK) { FTDI_Error((char *)"FT_SetRts", ftStatus ); return false; }
    
    //for usb-3012 pull DTR high to take AMBE3003 out of reset.
    //for other devices noting is connected to DTR so it is a dont care
    ftStatus = FT_ClrDtr(m_FtdiHandle);
    if (ftStatus != FT_OK) { FTDI_Error((char *)"FT_ClrDtr", ftStatus); return false; }
    
    ftStatus = FT_SetBaudRate(m_FtdiHandle, baudrate );
    if (ftStatus != FT_OK) { FTDI_Error((char *)"FT_SetBaudRate", ftStatus ); return false; }
    
    ftStatus = FT_SetLatencyTimer(m_FtdiHandle, 4);
    if (ftStatus != FT_OK) { FTDI_Error((char *)"FT_SetLatencyTimer", ftStatus ); return false; }
    
    ftStatus = FT_SetUSBParameters(m_FtdiHandle, USB3XXX_MAXPACKETSIZE, 0);
    if (ftStatus != FT_OK) { FTDI_Error((char *)"FT_SetUSBParameters", ftStatus ); return false; }
    
    ftStatus = FT_SetTimeouts(m_FtdiHandle, 200, 200 );
    if (ftStatus != FT_OK) { FTDI_Error((char *)"FT_SetTimeouts", ftStatus ); return false; }
    
    // done
    return true;
}

bool CUsb3000Interface::ResetDevice(void)
{
    bool ok = false;
    int len;
    char rxpacket[100];
    char zeropacket[10] =
    {
        0,0,0,0,0,0,0,0,0,0
    };
    char txpacket[7] =
    {
        PKT_HEADER,
        0,
        3,
        0,
        PKT_RESET,
        PKT_PARITYBYTE,
        3 ^ PKT_RESET ^ PKT_PARITYBYTE
    };
    
    
    //the chip might be in a state where it is waiting to receive bytes from a prior incomplete packet.
    //first send 350 zeros in case, the chip's receive state is still waiting for characters
    //if we send more than needed, the exta characters will just get discarded since they do not match the header byte
    //after that we send PKT_RESET to reset the device
    //As long as the AMBE3000 is able to receive via uart, this method will succeed in resetting it.
    
    for ( int i = 0; i < 35 ; i++ )
    {
        FTDI_write_packet(m_FtdiHandle, zeropacket, sizeof(zeropacket));
    }
    
    
    // write soft-reset packet
    if ( FTDI_write_packet(m_FtdiHandle, txpacket, sizeof(txpacket)) )
    {
        // read reply
        len = FTDI_read_packet( m_FtdiHandle, rxpacket, sizeof(rxpacket) );
        ok = ((len == 5) && (rxpacket[4] == PKT_READY));
        if ( !ok )
        {
            std::cout << "USB-3000 soft reset failed" << std::endl;
        }
    }
    
    // done
    return ok;
}

bool CUsb3000Interface::ConfigureDevice(void)
{
    bool ok = true;
    uint8 pkt_ratep_ambeplus[]  = { 0x01,0x30,0x07,0x63,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x48 };
    uint8 pkt_ratep_ambe2plus[] = { 0x04,0x31,0x07,0x54,0x24,0x00,0x00,0x00,0x00,0x00,0x6F,0x48 };
    
    // configure the channel for desired codec
    switch ( m_uiChCodec )
    {
        case CODEC_AMBEPLUS:
            ok &= ConfigureChannel(PKT_CHANNEL0, pkt_ratep_ambeplus, 0, 0);
            break;
        case CODEC_AMBE2PLUS:
            ok &= ConfigureChannel(PKT_CHANNEL0, pkt_ratep_ambe2plus, 0, 0);
            break;
        case CODEC_NONE:
        default:
            break;
    }
    
    // done
    return ok;
}
