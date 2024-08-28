//
//  cusb3xxxinterface.h
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

#ifndef cusb3xxxinterface_h
#define cusb3xxxinterface_h


#include "ftd2xx.h"
#include "cftdidevicedescr.h"
#include "cbuffer.h"
#include "cambepacket.h"
#include "cvoicepacket.h"
#include "cvocodecinterface.h"
#include "ctimepoint.h"

////////////////////////////////////////////////////////////////////////////////////////
// define

#define USB3XXX_MAXPACKETSIZE   1024        // must be multiple of 64

#define PKT_HEADER              0x61

#define PKT_CONTROL             0x00
#define PKT_CHANNEL             0x01
#define PKT_SPEECH              0x02

#define PKT_SPEECHD             0x00
#define PKT_CHAND               0x01
#define PKT_RATET               0x09
#define PKT_INIT                0x0b
#define PKT_PRODID              0x30
#define PKT_VERSTRING           0x31
#define PKT_PARITYBYTE          0x2F
#define PKT_RESET               0x33
#define PKT_READY               0x39
#define PKT_CHANNEL0            0x40
#define PKT_CHANNEL1            0x41
#define PKT_CHANNEL2            0x42
#define PKT_PARITYMODE          0x3F
#define PKT_ECMODE              0x05
#define PKT_DCMODE              0x06
#define PKT_COMPAND             0x32
#define PKT_RATEP               0x0A
#define PKT_CHANFMT             0x15
#define PKT_SPCHFMT             0x16
#define PKT_GAIN                0x4B


////////////////////////////////////////////////////////////////////////////////////////
// class

class CUsb3xxxInterface : public CVocodecInterface
{
public:
    // constructors
    CUsb3xxxInterface(uint32, uint32, const char *, const char *);
    
    // destructor
    virtual ~CUsb3xxxInterface();
    
    // initialization
    bool Init(void);

    // get
    const char *GetName(void) const     { return m_szDeviceName; }
    const char *GetSerial(void) const   { return m_szDeviceSerial; }

    // task
    void Task(void);

protected:
    // decoder helper
    virtual bool IsValidChannelPacket(const CBuffer &, int *, CAmbePacket *)    { return false; }
    virtual bool IsValidSpeechPacket(const CBuffer &, int *, CVoicePacket *)    { return false; }
    
    // encoder helpers
    virtual void EncodeChannelPacket(CBuffer *, int, CAmbePacket *) {}
    virtual void EncodeSpeechPacket(CBuffer *, int, CVoicePacket *) {}
    
    // low level
    virtual bool OpenDevice(void)                           { return false; }
    virtual bool ResetDevice(void)                          { return false; }
    bool ReadDeviceVersion(void);
    bool DisableParity(void);
    virtual bool ConfigureDevice(void)                      { return false; }
    bool ConfigureChannel(uint8, const uint8 *, int, int);
    bool CheckIfDeviceNeedsReOpen(void);
    virtual int GetDeviceFifoSize(void) const              { return 1; }
    
    // io level
    bool ReadBuffer(CBuffer *);
    bool WriteBuffer(const CBuffer &);
    int  FTDI_read_packet(FT_HANDLE, char *, int);
    bool FTDI_read_bytes(FT_HANDLE, char *, int);
    bool FTDI_write_packet(FT_HANDLE, const char *, int);
    
    // error reporting
    void FTDI_Error(char *, FT_STATUS);
    
protected:
    // data
    uint32                      m_uiVid;
    uint32                      m_uiPid;
    char                        m_szDeviceName[FTDI_MAX_STRINGLENGTH];
    char                        m_szDeviceSerial[FTDI_MAX_STRINGLENGTH];
    FT_HANDLE                   m_FtdiHandle;
    
    // queue
    std::vector<CPacketQueue*>  m_SpeechQueues;
    std::vector<CPacketQueue*>  m_ChannelQueues;
    CPacketQueue                m_DeviceQueue;
    int                         m_iSpeechFifolLevel;
    int                         m_iChannelFifolLevel;
    CTimePoint                  m_SpeechFifoLevelTimeout;
    CTimePoint                  m_ChannelFifoLevelTimeout;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cusb3xxxinterface_h */
