//
//  cstream.h
//  ambed
//
//  Created by Jean-Luc Deltombe (LX3JL) on 15/04/2017.
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

#ifndef cstream_h
#define cstream_h

#include <queue>
#include <vector>
#include "cudpsocket.h"
#include "ccallsign.h"
#include "cambepacket.h"
#include "cvocodecchannel.h"
#include "csignalprocessor.h"
#include "codec2/codec2.h"
#include "imbe/imbe_vocoder.h"

////////////////////////////////////////////////////////////////////////////////////////
// class

class CStream
{
public:
    // constructors
    CStream();
    CStream(uint16, const CCallsign &, const CIp &, uint8, uint8, uint8, uint8);

    // destructor
    virtual ~CStream();

    // initialization
    bool Init(uint16);
    void Close(void);

    // get
    uint16  GetId(void) const           { return m_uiId; }
    uint16  GetPort(void) const         { return m_uiPort; }
    uint8   GetCodecIn(void) const      { return m_uiCodecIn; }
    uint8   GetCodecOut(void) const     { return m_uiCodecOut1; }
    uint8   GetCodecOut1(void) const    { return m_uiCodecOut1; }
    uint8   GetCodecOut2(void) const    { return m_uiCodecOut2; }
    uint8   GetCodecOut3(void) const    { return m_uiCodecOut3; }

    // activity timer
    bool    IsActive(void) const        { return m_LastActivity.DurationSinceNow() <= STREAM_ACTIVITY_TIMEOUT; }

    // task
    static void Thread(CStream *);
    void Task(void);

protected:
    // packet decoding helpers
    bool IsValidDvFramePacket(const CBuffer &, uint8 *, uint8 *);
    bool IsValidCodec2FramePacket(const CBuffer &, uint8 *, uint8 *);
    bool IsValidImbeFramePacket(const CBuffer &, uint8 *, uint8 *);

    // packet encoding helpers
    void EncodeDvFramePacket(CBuffer *, uint8, uint8 *, uint8 *, uint8 *);

    // Codec2 input handling
    void TaskCodec2Input(void);

    // IMBE input handling
    void TaskImbeInput(void);

protected:
    // data
    uint16          m_uiId;
    CUdpSocket      m_Socket;
    uint16          m_uiPort;
    uint8           m_uiCodecIn;
    uint8           m_uiCodecOut1;
    uint8           m_uiCodecOut2;
    uint8           m_uiCodecOut3;
    CVocodecChannel *m_VocodecChannel;

    // Codec2 input mode: two encoder channels + decoder + signal processors + IMBE encoder
    CVocodecChannel *m_EncoderChannel1;  // AMBE+ encoder
    CVocodecChannel *m_EncoderChannel2;  // AMBE2+ encoder
    CCodec2         *m_pCodec2Decoder;   // Software Codec2 decoder
    CSignalProcessor *m_pSignalProcessor1;  // Gain for AMBE+ output
    CSignalProcessor *m_pSignalProcessor2;  // Gain for AMBE2+ output
    imbe_vocoder    *m_pImbeEncoder;     // Software IMBE encoder for Codec2 input
    std::queue<std::vector<uint8>> m_ImbeQueue;  // Queue for IMBE data in Codec2 input mode

    // IMBE input mode: IMBE decoder + Codec2 encoder
    imbe_vocoder    *m_pImbeDecoder;     // Software IMBE decoder
    CCodec2         *m_pCodec2Encoder;   // Software Codec2 encoder for IMBE input
    std::queue<std::vector<uint8>> m_Codec2Queue;  // Queue for Codec2 data in IMBE input mode

    // Precomputed gain multipliers (avoid pow() per packet)
    float           m_fCodec2DecodeGain;    // Codec2 decode output gain (Codec2 input mode)
    float           m_fImbeGain;            // IMBE encode/decode gain
    float           m_fCodec2Gain;          // Codec2 encode gain (IMBE input mode)

    // Atomic counters for software queue sizes (for thread-safe drain checks in Close())
    std::atomic<int> m_nImbeQueueSize;
    std::atomic<int> m_nCodec2QueueSize;

    // client details
    CCallsign       m_Callsign;
    CIp             m_Ip;

    // counters
    std::atomic<int> m_iTotalPackets;
    std::atomic<int> m_iLostPackets;

    // staged packet waiting for software codecs to catch up
    CAmbePacket     *m_pStagedPacket;
    CTimePoint      m_StagedPacketTime;

    // activity timer
    CTimePoint      m_LastActivity;

    // thread
    std::atomic<bool> m_bStopThread;
    std::thread     *m_pThread;

};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cstream_h */
