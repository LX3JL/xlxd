//
//  cdvframepacket.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 01/11/2015.
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

#ifndef cdvframepacket_h
#define cdvframepacket_h

#include "cpacket.h"

////////////////////////////////////////////////////////////////////////////////////////
// defines

#define AMBE_SIZE       9
#define DVDATA_SIZE     3

#define AMBEPLUS_SIZE   9
#define DVSYNC_SIZE     7

#define CODEC2_SIZE     8   // Codec2 3200bps frame = 8 bytes per 20ms

#define IMBE_SIZE       11  // IMBE 88-bit frame = 11 bytes

// typedef & structures

struct __attribute__ ((__packed__))dstar_dvframe
{
    uint8	AMBE[AMBE_SIZE];
    uint8	DVDATA[DVDATA_SIZE];
};

////////////////////////////////////////////////////////////////////////////////////////
// class

class CDvFramePacket : public CPacket
{
friend class CCodecStream;
public:
    // constructor
    CDvFramePacket();
    CDvFramePacket(const struct dstar_dvframe *, uint16, uint8);
    CDvFramePacket(const uint8 *, const uint8 *, uint16, uint8, uint8);
    CDvFramePacket(const uint8 *, uint16, uint8, uint8, uint8);
    CDvFramePacket(const uint8 *, uint16, uint8, uint8, uint16);
    CDvFramePacket(uint16, uint8, const uint8 *, const uint8 *, uint8, uint8, const uint8 *, const uint8 *);
    // M17 constructor
    CDvFramePacket(const uint8 *codec2, uint16 sid, uint8 pid);
    // P25 constructor
    CDvFramePacket(uint16 sid, const uint8 *imbe);
    CDvFramePacket(const CDvFramePacket &);
    
    // destructor
    virtual ~CDvFramePacket() {};
    
    // virtual duplication
    CPacket *Duplicate(void) const;
    
    // identity
    bool IsDvFrame(void) const              { return true; }
    bool HaveTranscodableAmbe(void) const   { return true; }
    
    // get
    const uint8 *GetAmbe(uint8) const;
    const uint8 *GetAmbe(void) const        { return m_uiAmbe; }
    const uint8 *GetAmbePlus(void) const    { return m_uiAmbePlus; }
    const uint8 *GetCodec2(void) const      { return m_uiCodec2; }
    const uint8 *GetImbe(void) const        { return m_uiImbe; }
    const uint8 *GetDvData(void) const      { return m_uiDvData; }
    const uint8 *GetDvSync(void) const      { return m_uiDvSync; }
    // Per-codec presence flags. Each Has* returns true iff the
    // corresponding byte slot has been populated with real codec
    // data (via SetAmbe or a real-data constructor). The default
    // state for every flag is false — set explicitly when a writer
    // populates the slot.
    //
    // Why all four flags matter (the asymmetric design pre-2026
    // had only Codec2/IMBE flags): the receiving-side codec-stream
    // Phase 1 handler uses these flags to decide whether to
    // overwrite a slot from an ambed response. Slots that arrived
    // pre-populated from a multi-codec wire frame (XLX interlink)
    // must NOT be overwritten — that would replace original
    // wire-quality audio with re-transcoded audio carrying an
    // extra vocoder hop. See ccodecstream.cpp Phase 1 for the
    // guard logic.
    bool HasAmbeData(void) const            { return m_bHasAmbe; }
    bool HasAmbePlusData(void) const        { return m_bHasAmbePlus; }
    bool HasCodec2Data(void) const          { return m_bHasCodec2; }
    bool HasImbeData(void) const            { return m_bHasImbe; }

    // set
    void SetDvData(uint8 *);
    void SetAmbe(uint8, uint8 *);
    void SetCodec2(const uint8 *);

    // Reconcile the four Has-flags to reflect what the source actually
    // provides. mask is a bitmask of (1U << CODEC_*) values from the
    // source client's GetAvailableCodecs(). For each codec, the flag
    // is set true iff the bit is set in mask, false otherwise. Used
    // by CXlxProtocol's packet-in handlers to override the wire-format
    // parser's default assumptions for multi-slot XLX frames — the
    // XLX wire-frame constructor cannot know whether reserved slots
    // are populated with real codec data (native XLX peer) or empty
    // bytes (BM-style CXlxDmrPeer where the dstar slot is reserved
    // but unused). Source-aware code calls this immediately after
    // parsing to align the packet's flags with what the source
    // genuinely provides.
    void ConformAvailableCodecs(uint32 mask);

    // operators
    bool operator ==(const CDvFramePacket &) const;

protected:
    // get
    uint8 *GetAmbeData(void)                { return m_uiAmbe; }
    uint8 *GetAmbePlusData(void)            { return m_uiAmbePlus; }
    uint8 *GetCodec2Data(void)              { return m_uiCodec2; }
    uint8 *GetImbeData(void)                { return m_uiImbe; }

protected:
    // data (dstar)
    uint8       m_uiAmbe[AMBE_SIZE];
    uint8       m_uiDvData[DVDATA_SIZE];
    bool        m_bHasAmbe;
    // data (dmr)
    uint8       m_uiAmbePlus[AMBEPLUS_SIZE];
    uint8       m_uiDvSync[DVSYNC_SIZE];
    bool        m_bHasAmbePlus;
    // data (m17)
    uint8       m_uiCodec2[CODEC2_SIZE];
    bool        m_bHasCodec2;
    // data (p25)
    uint8       m_uiImbe[IMBE_SIZE];
    bool        m_bHasImbe;
};


////////////////////////////////////////////////////////////////////////////////////////
#endif /* cdvframepacket_h */
