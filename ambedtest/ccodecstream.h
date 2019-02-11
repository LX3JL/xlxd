//
//  ccodecstream.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 13/04/2017.
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

#ifndef ccodecstream_h
#define ccodecstream_h

#include "csemaphore.h"
#include "cudpsocket.h"
#include "ctimepoint.h"
#include "cambe.h"


////////////////////////////////////////////////////////////////////////////////////////
// define

// frame sizes
#define AMBE_SIZE           9
#define AMBEPLUS_SIZE       9


////////////////////////////////////////////////////////////////////////////////////////
// class

class CCodecStream
{
public:
    // constructor
    CCodecStream(uint16, uint8, uint8);
    
    // destructor
    virtual ~CCodecStream();
    
    // initialization
    bool Init(uint16);
    void Close(void);
    
    // get
    bool   IsConnected(void) const          { return m_bConnected; }
    uint16 GetStreamId(void) const          { return m_uiStreamId; }

    // task
    static void Thread(CCodecStream *);
    void Task(void);
    

protected:
    // packet decoding helpers
    bool IsValidAmbePacket(const CBuffer &, uint8 *, uint8 *, uint8 *, uint8 *);
    
    // packet encoding helpers
    void EncodeAmbePacket(CBuffer *, const uint8 *);

    // stats helpers
    void ResetStats(void);
    void DisplayStats(void);
    
protected:
    // test data
    std::vector<CAmbe *>    m_AmbeSrc;
    int                     m_iAmbeSrcPtr;
    std::vector<CAmbe *>    m_AmbeDest;
    int                     m_iAmbeDestPtr;
    
    // data
    uint16          m_uiStreamId;
    uint16          m_uiPort;
    uint8           m_uiPid;
    uint8           m_uiCodecIn;
    uint8           m_uiCodecsOut;

    // socket
    CIp             m_Ip;
    CUdpSocket      m_Socket;
    bool            m_bConnected;
    
    // thread
    bool            m_bStopThread;
    std::thread     *m_pThread;
    CTimePoint      m_TimeoutTimer;
    CTimePoint      m_FrameTimer;
    uint32          m_uiNbTotalPacketSent;
    
    // stats
    CTimePoint      m_StatsTimer;
    CTimePoint      m_DisplayStatsTimer;
    uint32          m_uiNbPacketSent;
    uint32          m_uiNbPacketReceived;
    uint32          m_uiNbPacketBad;
    uint32          m_uiNbPacketTimeout;
};


////////////////////////////////////////////////////////////////////////////////////////
#endif /* ccodecstream_h */
