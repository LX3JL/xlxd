//
//  cdvheaderpacket.cpp
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

#include "main.h"
#include <string.h>
#include <cstddef>
#include <cstdio>
#include "ccrc.h"
#include "cdmriddir.h"
#include "cdvheaderpacket.h"

// Compile-time guards for ComputeCrc(). The whole CRC-at-offset-39 design
// depends on the layout of struct dstar_header being exactly 41 bytes with
// Crc at the end. If the __attribute__((__packed__)) is ever removed or
// padding sneaks in (e.g. an alignment change on a new field), these
// static_asserts catch it at compile time instead of producing silently
// wrong CRCs on the wire that real Icom hardware would reject.
static_assert(sizeof(struct dstar_header) == 41,
              "dstar_header must be exactly 41 bytes packed; "
              "ComputeCrc relies on Crc landing at offset 39");
static_assert(offsetof(struct dstar_header, Crc) == 39,
              "dstar_header Crc must be at byte offset 39; "
              "addCCITT161 writes the 2 CRC bytes into the last 2 bytes "
              "of the buffer it's given");

////////////////////////////////////////////////////////////////////////////////////////
// constructor

CDvHeaderPacket::CDvHeaderPacket()
{
    m_uiFlag1 = 0;
    m_uiFlag2 = 0;
    m_uiFlag3 = 0;
    m_uiCrc = 0;
}

// dstar constructor

CDvHeaderPacket::CDvHeaderPacket(const struct dstar_header *buffer, uint16 sid, uint8 pid)
    : CPacket(sid, pid)
{
    m_uiFlag1 = buffer->Flag1;
    m_uiFlag2 = buffer->Flag2;
    m_uiFlag3 = buffer->Flag3;
    m_csUR.SetCallsign(buffer->UR, CALLSIGN_LEN);
    m_csRPT1.SetCallsign(buffer->RPT1, CALLSIGN_LEN);
    m_csRPT2.SetCallsign(buffer->RPT2, CALLSIGN_LEN);
    m_csMY.SetCallsign(buffer->MY, CALLSIGN_LEN);
    m_csMY.SetSuffix(buffer->SUFFIX, CALLSUFFIX_LEN);
    // Start with the source's CRC, then recompute to guarantee it matches
    // whatever shape we ended up with. Router may still mutate RPT2 later,
    // which will re-run ComputeCrc() via the setter, but emitting from
    // this constructor without Router involvement already produces a
    // valid CRC.
    m_uiCrc = buffer->Crc;
    ComputeCrc();
}

// dmr constructor

CDvHeaderPacket::CDvHeaderPacket(uint32 my, const CCallsign &ur, const CCallsign &rpt1, const CCallsign &rpt2, uint16 sid, uint8 pid, uint8 spid)
    : CPacket(sid, pid, spid)
{
    m_uiFlag1 = 0;
    m_uiFlag2 = 0;
    m_uiFlag3 = 0;
    m_uiCrc = 0;
    m_csUR = ur;
    m_csRPT1 = rpt1;
    m_csRPT2 = rpt2;
    m_csMY = CCallsign("", my);
    // Cross-mode headers would otherwise ship with CRC = 0, which strict
    // D-Star receivers (real Icom repeaters) reject. Compute over the
    // final field values.
    ComputeCrc();
}

// YSF + IMRS constructor

CDvHeaderPacket::CDvHeaderPacket(const CCallsign &my, const CCallsign &ur, const CCallsign &rpt1, const CCallsign &rpt2, uint16 sid, uint8 pid)
: CPacket(sid, pid, 0, (uint8)0)
{
    m_uiFlag1 = 0;
    m_uiFlag2 = 0;
    m_uiFlag3 = 0;
    m_uiCrc = 0;
    m_csUR = ur;
    m_csRPT1 = rpt1;
    m_csRPT2 = rpt2;
    m_csMY = my;
    // See comment in DMR constructor — cross-mode CRC must be computed.
    ComputeCrc();
}


// copy constructor

CDvHeaderPacket::CDvHeaderPacket(const CDvHeaderPacket &Header)
: CPacket(Header)
{
    m_uiFlag1 = Header.m_uiFlag1;
    m_uiFlag2 = Header.m_uiFlag2;
    m_uiFlag3 = Header.m_uiFlag3;
    m_csUR = Header.m_csUR;
    m_csRPT1 = Header.m_csRPT1;
    m_csRPT2 = Header.m_csRPT2;
    m_csMY = Header.m_csMY;
    m_uiCrc = Header.m_uiCrc;
}


////////////////////////////////////////////////////////////////////////////////////////
// virtual duplication

CPacket *CDvHeaderPacket::Duplicate(void) const
{
    return new CDvHeaderPacket(*this);
}

////////////////////////////////////////////////////////////////////////////////////////
// conversion

void CDvHeaderPacket::ConvertToDstarStruct(struct dstar_header *buffer) const
{
    ::memset(buffer, 0, sizeof(struct dstar_header));
    buffer->Flag1 = m_uiFlag1;
    buffer->Flag2 = m_uiFlag2;
    buffer->Flag3 = m_uiFlag3;
    m_csUR.GetCallsign(buffer->UR);
    m_csRPT1.GetCallsign(buffer->RPT1);
    m_csRPT2.GetCallsign(buffer->RPT2);
    m_csMY.GetCallsign(buffer->MY);
    m_csMY.GetSuffix(buffer->SUFFIX);
    buffer->Crc = m_uiCrc;
}

////////////////////////////////////////////////////////////////////////////////////////
// CRC

// Compute the D-Star header CRC (CCITT-16, init 0xFFFF) over the 39 bytes
// preceding the Crc field and store the result in m_uiCrc. Called from
// every non-default constructor and from the mutating setters, so the
// stored CRC is always consistent with the current field values.
//
// Implementation note: we serialise into a temporary `dstar_header`,
// let CCRC::addCCITT161 write the CRC bytes at positions [39,40] of that
// buffer (its documented behaviour — see ccrc.cpp:188-207), then assemble
// m_uiCrc from those two bytes by explicit index (low byte at [39], high
// byte at [40]) rather than reading the packed-struct `Crc` field.
//
// Why index-reads and not `tmp.Crc`:
//   - Endian independence. `addCCITT161` emits the low CRC byte at
//     [length-2] and the high byte at [length-1] regardless of host
//     endianness, matching the D-Star wire spec. Reading those two bytes
//     by index and shifting reconstructs the value identically on LE and
//     BE hosts. Reading back through the packed `Crc` uint16 would give
//     a byte-swapped value on a BE host. xlxd ships on x86_64 / ARM64 LE
//     today, but the two-byte index read costs nothing and removes the
//     latent BE-portability trap.
//   - Strict-aliasing / alignment. The `Crc` field sits at byte offset 39
//     of a `__attribute__((packed))` struct — a 1-byte-aligned uint16 in
//     a stack temporary whose alignment is at most 1 (due to packing).
//     On strict-alignment architectures (ARMv5 / older MIPS) a typed
//     uint16 read at an odd address is UB; byte reads are always safe.
//
// Cost: ~90 bytes of stack for `tmp`, one call to ConvertToDstarStruct
// (which does ~40 bytes of member copies), and 39 table lookups inside
// addCCITT161. Negligible at ≤3 calls per transmission.
void CDvHeaderPacket::ComputeCrc(void)
{
    struct dstar_header tmp;
    ConvertToDstarStruct(&tmp);
    uint8 *bytes = (uint8 *)&tmp;
    CCRC::addCCITT161(bytes, sizeof(struct dstar_header));
    // Reassemble low-byte-first (D-Star wire order). Indices are the last
    // two bytes of the struct, guaranteed by the offsetof static_assert
    // above to coincide with the Crc field.
    m_uiCrc = (uint16)bytes[sizeof(struct dstar_header) - 2]
            | ((uint16)bytes[sizeof(struct dstar_header) - 1] << 8);
}


////////////////////////////////////////////////////////////////////////////////////////
// get valid

bool CDvHeaderPacket::IsValid(void) const
{
    bool valid = CPacket::IsValid();
    
    valid &= m_csRPT1.IsValid();
    valid &= m_csRPT2.IsValid();
    valid &= m_csMY.IsValid();
    
    return valid;
}


////////////////////////////////////////////////////////////////////////////////////////
// operators

bool CDvHeaderPacket::operator ==(const CDvHeaderPacket &Header) const
{
    return ( (m_uiFlag1 == Header.m_uiFlag1) &&
             (m_uiFlag2 == Header.m_uiFlag2) &&
             (m_uiFlag3 == Header.m_uiFlag3) &&
             (m_csUR == Header.m_csUR) &&
             (m_csRPT1 == Header.m_csRPT1) &&
             (m_csRPT2 == Header.m_csRPT2) &&
             (m_csMY == Header.m_csMY) );
}

#ifdef IMPLEMENT_CDVHEADERPACKET_CONST_CHAR_OPERATOR
CDvHeaderPacket::operator const char *() const
{
	char *sz = (char *)(const char *)m_sz;

    std::sprintf(sz, "%02X %02X %02X\n%s\n%s\n%s\n%s",
        m_uiFlag1, m_uiFlag2, m_uiFlag3,
        (const char *)m_csUR,
        (const char *)m_csRPT1,
        (const char *)m_csRPT2,
        (const char *)m_csMY);
        
    return m_sz;
}
#endif
