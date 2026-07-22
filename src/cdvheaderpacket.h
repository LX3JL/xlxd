//
//  cdvheaderpacket.h
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

#ifndef cdvheaderpacket_h
#define cdvheaderpacket_h

#include "ccallsign.h"
#include "cpacket.h"

////////////////////////////////////////////////////////////////////////////////////////
// implementation details

//#define IMPLEMENT_CDVHEADERPACKET_CONST_CHAR_OPERATOR

////////////////////////////////////////////////////////////////////////////////////////
// typedef & structures

struct __attribute__ ((__packed__))dstar_header
{
    // flags
    uint8	Flag1;
    uint8	Flag2;
    uint8	Flag3;
    // callsigns
    uint8	RPT2[CALLSIGN_LEN];
    uint8	RPT1[CALLSIGN_LEN];
    uint8	UR[CALLSIGN_LEN];
    uint8	MY[CALLSIGN_LEN];
    uint8	SUFFIX[CALLSUFFIX_LEN];
    // crc
    uint16  Crc;
};


////////////////////////////////////////////////////////////////////////////////////////
// class

class CDvHeaderPacket : public CPacket
{
public:
    // constructor
    CDvHeaderPacket();
    CDvHeaderPacket(const struct dstar_header *, uint16, uint8);
    CDvHeaderPacket(uint32, const CCallsign &, const CCallsign &, const CCallsign &, uint16, uint8, uint8);
    CDvHeaderPacket(const CCallsign &, const CCallsign &, const CCallsign &, const CCallsign &, uint16, uint8);
    CDvHeaderPacket(const CDvHeaderPacket &);
    
    // destructor
    virtual ~CDvHeaderPacket(){};

    // virtual duplication
    CPacket *Duplicate(void) const;

    // identity
    bool IsDvHeader(void) const                     { return true; }

    // conversion
    void ConvertToDstarStruct(struct dstar_header *) const;

    // CRC — recomputed over the 39 bytes before Crc (CCITT-16, init 0xFFFF)
    // as specified by the D-Star protocol. Called automatically by the
    // non-default constructors and by mutating setters below, so m_uiCrc is
    // consistent with the current field values for any fully-constructed
    // packet (default constructor leaves fields and CRC at zero — setting
    // RPT2/suffix before other fields would compute a CRC over partial
    // state, but no code path does that today). Protocols that emit the
    // full 41-byte dstar_header (DExtra, DPlus, G3 terminal) therefore
    // write a valid CRC on the wire, which strict-CRC receivers (real Icom
    // hardware) require. DCS's encoder deliberately drops the last 2 bytes
    // and replaces them with StreamId, so m_uiCrc goes unused there — no
    // change on the DCS wire. XLX interlink doesn't emit a D-Star header.
    void ComputeCrc(void);

    // get valid
    bool IsValid(void) const;

    // get callsigns
    const CCallsign &GetUrCallsign(void) const      { return m_csUR; }
    const CCallsign &GetRpt1Callsign(void) const    { return m_csRPT1; }
    const CCallsign &GetRpt2Callsign(void) const    { return m_csRPT2; }
    const CCallsign &GetMyCallsign(void) const      { return m_csMY; }

    // get modules
    char GetUrModule(void) const                    { return m_csUR.GetModule(); }
    char GetRpt1Module(void) const                  { return m_csRPT1.GetModule(); }
    char GetRpt2Module(void) const                  { return m_csRPT2.GetModule(); }
    char GetMyModule(void) const                    { return m_csMY.GetModule(); }

    // set callsigns — any mutation invalidates the CRC, so each setter
    // re-runs ComputeCrc() to keep m_uiCrc fresh. Cost is a 39-byte CCITT-16
    // (microseconds); called at most ~3 times per transmission.
    void SetRpt2Callsign(const CCallsign &cs)       { m_csRPT2 = cs; ComputeCrc(); }
    void SetRpt2Module(char c)                      { m_csRPT2.SetModule(c); ComputeCrc(); }
    void SetMySuffix(const char *sz)                { m_csMY.SetSuffix(sz); ComputeCrc(); }
    bool HasMySuffix(void) const                    { return m_csMY.HasSuffix(); }
    
    // operators
    bool operator ==(const CDvHeaderPacket &) const;
#ifdef IMPLEMENT_CDVHEADERPACKET_CONST_CHAR_OPERATOR
    operator const char *() const;
#endif

protected:
    // data
    uint8       m_uiFlag1;
    uint8       m_uiFlag2;
    uint8       m_uiFlag3;
    CCallsign   m_csUR;
    CCallsign   m_csRPT1;
    CCallsign   m_csRPT2;
    CCallsign   m_csMY;
    uint16      m_uiCrc;
#ifdef IMPLEMENT_CDVHEADERPACKET_CONST_CHAR_OPERATOR
    // buffer
    char		m_sz[256];
#endif
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cdvheaderpacket_h */
