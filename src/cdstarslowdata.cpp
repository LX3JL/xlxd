//
//  cdstarslowdata.cpp
//  xlxd
//
//  D-Star slow-data generator for cross-mode → AMBE+ transcoded egress.
//  See cdstarslowdata.h for the wire-format reference.
//
//  Ported from MMDVMHost DStarSlowData.cpp (Jonathan Naylor G4KLX, GPL).
//  MMDVMHost permalink used as the canonical reference:
//    https://github.com/g4klx/MMDVMHost/blob/master/DStarSlowData.cpp
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
// ----------------------------------------------------------------------------

#include "main.h"
#include <string.h>
#include <stdio.h>
#include "cdstarslowdata.h"
#include "cdvheaderpacket.h"
#include "ccrc.h"
#include "creflector.h"

////////////////////////////////////////////////////////////////////////////////////////
// static constants

// D-Star slow-data scrambler. The same 3-byte XOR mask applies to every
// 3-byte slow-data frame independently — the scrambler is stateless. This
// matches DSTAR_SCRAMBLER_BYTES[0..2] in MMDVMHost's DStarDefines.h.
const uint8 CDStarSlowData::SCRAMBLER[3] = { 0x70, 0x4F, 0x93 };

// Pre-XOR filler byte. ASCII 'f' (0x66). After XOR with SCRAMBLER yields
// 0x16, 0x29, 0xF5 on the wire — the DSTAR_NULL_SLOW_DATA_BYTES pattern
// that real D-Star radios emit once their text message has been fully
// transmitted inside the current 21-frame cycle.
const uint8 CDStarSlowData::FILLER_PLAIN = 0x66;

// m_headerSyncPtr / m_textPtr are uint8. If either buffer grew past 255
// bytes, the pointer would overflow and the exhaustion guard in
// GetSlowData() would wrap instead of saturate — turning an exhaustion
// check into an infinite loop through a sliding window of the buffer,
// corrupting slow-data on the wire with no runtime crash. Both asserts
// reference the SAME named constants that size the arrays, so any
// future resizing flows through this check automatically rather than
// relying on a matching hand-edit in two places.
static_assert(DSTAR_HEADER_SYNC_BUFFER_LEN <= 255,
              "m_headerSyncPtr is uint8; widen if buffer grows past 255 bytes");
static_assert(DSTAR_TEXT_BUFFER_LEN <= 255,
              "m_textPtr is uint8; widen if text buffer grows past 255 bytes");

////////////////////////////////////////////////////////////////////////////////////////
// constructor

CDStarSlowData::CDStarSlowData()
{
    m_mode = MODE_TEXT;
    ::memset(m_text, FILLER_PLAIN, sizeof(m_text));
    m_textPtr = sizeof(m_text);                  // exhausted until SetText() populates
    ::memset(m_headerSync, 0x00, sizeof(m_headerSync));
    m_headerSyncPtr = sizeof(m_headerSync);      // exhausted until SetHeaderSync()
    m_bHasHeaderSync = false;
}

////////////////////////////////////////////////////////////////////////////////////////
// text buffer construction
//
// Layout after SetText("NXDN via XLX672 B   "):
//
//   m_text[ 0] = 0x40      m_text[ 6] = 0x41      m_text[12] = 0x42      m_text[18] = 0x43
//   m_text[ 1] = 'N'       m_text[ 7] = 'i'       m_text[13] = 'L'       m_text[19] = '2'
//   m_text[ 2] = 'X'       m_text[ 8] = 'a'       m_text[14] = 'X'       m_text[20] = ' '
//   m_text[ 3] = 'D'       m_text[ 9] = ' '       m_text[15] = '6'       m_text[21] = 'B'
//   m_text[ 4] = 'N'       m_text[10] = 'X'       m_text[16] = '7'       m_text[22] = ' '
//   m_text[ 5] = ' '       m_text[11] = 'L'       m_text[17] = '2'       m_text[23] = ' '
//
// GetSlowData() then emits 3 bytes at a time starting from m_text[0],
// producing segments:
//   [1]→[0x40 'N' 'X']  [2]→['D' 'N' ' ']  [3]→[0x41 ...]  ...  [8]→[... ' ' ' ']
// after which m_textPtr reaches 24 and subsequent calls emit filler.

void CDStarSlowData::SetText(const char *msg)
{
    // Space-pad the incoming message into exactly DSTAR_SLOW_DATA_TEXT_LEN
    // characters. Shorter input is right-padded with spaces; longer is
    // truncated. We never touch anything past msg's null terminator.
    uint8 padded[DSTAR_SLOW_DATA_TEXT_LEN];
    ::memset(padded, ' ', sizeof(padded));
    if ( msg != NULL )
    {
        size_t len = ::strlen(msg);
        if ( len > DSTAR_SLOW_DATA_TEXT_LEN ) len = DSTAR_SLOW_DATA_TEXT_LEN;
        ::memcpy(padded, msg, len);
    }

    // Interleave the 4 segment-header bytes (0x40..0x43) with 5-char
    // text chunks. Each segment is 6 bytes total and will be split
    // across two consecutive 3-byte slow-data frames on the wire.
    for ( int seg = 0; seg < 4; seg++ )
    {
        m_text[seg * 6] = (uint8)(0x40 + seg);
        ::memcpy(&m_text[seg * 6 + 1], &padded[seg * 5], 5);
    }

    m_textPtr = 0;
}

////////////////////////////////////////////////////////////////////////////////////////
// cycle rewind

void CDStarSlowData::Reset(void)
{
    // Rewind the active-mode pointer without changing which mode is live.
    // Kept for debug / manual-replay callers. Normal cycle-boundary usage
    // should call BeginTextCycle() / BeginHeaderCycle() explicitly so the
    // alternation policy is visible at the call site.
    if ( m_mode == MODE_HEADER )
    {
        m_headerSyncPtr = 0;
    }
    else
    {
        m_textPtr = 0;
    }
}

void CDStarSlowData::BeginTextCycle(void)
{
    m_mode = MODE_TEXT;
    m_textPtr = 0;
}

void CDStarSlowData::BeginHeaderCycle(void)
{
    m_mode = MODE_HEADER;
    m_headerSyncPtr = 0;
}

////////////////////////////////////////////////////////////////////////////////////////
// emit next 3 bytes

void CDStarSlowData::GetSlowData(uint8 *out3)
{
    // Pick the active mode's buffer. If the buffer is exhausted (or the
    // mode is header-sync but SetHeaderSync was never called), fall through
    // to the filler emitter below.
    //
    // NOTE: if a future contributor adds a new Mode enum value (e.g.
    // MODE_GPS for synthesized DPRS data), they MUST add a matching
    // dispatch branch here. Without one, the new mode falls through to
    // the filler path silently — no compile error, no runtime warning,
    // just filler bytes on the wire instead of the intended content.
    // Today there's no cross-mode use-case for GPS so we haven't added
    // a `default:` assertion, but future expansion should.
    const uint8 *src = NULL;
    uint8 *ptr = NULL;
    size_t len = 0;
    if ( m_mode == MODE_HEADER && m_bHasHeaderSync )
    {
        src = m_headerSync;
        ptr = &m_headerSyncPtr;
        len = sizeof(m_headerSync);
    }
    else if ( m_mode == MODE_TEXT )
    {
        src = m_text;
        ptr = &m_textPtr;
        len = sizeof(m_text);
    }

    if ( src != NULL && *ptr < len )
    {
        // Emit the next 3 bytes from the active buffer, XOR-scrambled.
        out3[0] = src[*ptr + 0] ^ SCRAMBLER[0];
        out3[1] = src[*ptr + 1] ^ SCRAMBLER[1];
        out3[2] = src[*ptr + 2] ^ SCRAMBLER[2];
        *ptr += 3;
        return;
    }

    // Buffer exhausted (or no header set in header mode) — emit filler.
    // 0x66 XOR {0x70,0x4F,0x93} = {0x16, 0x29, 0xF5} on the wire, the
    // DSTAR_NULL_SLOW_DATA_BYTES pattern real radios emit.
    out3[0] = FILLER_PLAIN ^ SCRAMBLER[0];
    out3[1] = FILLER_PLAIN ^ SCRAMBLER[1];
    out3[2] = FILLER_PLAIN ^ SCRAMBLER[2];
}

////////////////////////////////////////////////////////////////////////////////////////
// Header-sync arming
//
// Builds the header-sync buffer from the given CDvHeaderPacket.
// The 41-byte D-Star RF header is:
//   [Flag1][Flag2][Flag3] + [RPT2 8B][RPT1 8B][UR 8B][MY 8B][SUFFIX 4B] + [CRC 2B]
// We force Flag1 = DSTAR_SLOW_DATA_HEADER_FLAG1 (0x40, DSTAR_REPEATER_MASK)
// to match native-radio convention — see comment on the define in the header
// for why the slow-data copy diverges from the UDP gateway header's Flag1.
// CRC is recomputed over the modified 39-byte prefix.
//
// Fan-out: 8 full length-5 elements × 6 bytes = 48 bytes, covering header
// bytes 0..39 exactly, followed by one short length-1 3-byte tail element
// [0x51][header[40]=CRC_hi][FILLER_PLAIN]. Total 51 bytes = 17 × 3-byte
// slow-data slots (frames 22-38 of a header-sync cycle). After the
// 51-byte stream is drained by GetSlowData, the exhaustion path emits
// pure filler for the remaining frames 39-40.

void CDStarSlowData::SetHeaderSync(const CDvHeaderPacket &hdr)
{
    // Serialise to the on-wire 41-byte D-Star RF header layout.
    struct dstar_header raw;
    hdr.ConvertToDstarStruct(&raw);

    // Force Flag1 to radio-native repeater-mode value. The UDP header the
    // reflector sends keeps Flag1=0x00 (xlxd convention, unchanged), but
    // the slow-data copy must match what real radios put on the air so
    // strict decoders (Icom RP2C) see a well-formed RF header.
    raw.Flag1 = DSTAR_SLOW_DATA_HEADER_FLAG1;

    // Override RPT2 with a clean D-Star gateway-style callsign built from
    // g_Reflector.GetCallsign() + stream module. Why:
    //
    // Several cross-mode ingress protocols (NXDN, P25, YSF, DCS-in) patch
    // their OWN m_ReflectorCallsign in Init() with a protocol-specific
    // prefix for peer-network loop prevention. NXDN uses a 4-char patch
    // ("NXDN" over "XLX672" leaves "NXDN72"), P25 uses 3 chars ("P25672"),
    // YSF ("YSF672"), etc. That patched callsign ends up as RPT2 in the
    // header object that CPacketStream::m_DvHeader caches at Open().
    //
    // UDP header packets are fine: CReflector::RouterThread rewrites RPT2
    // per egress protocol on the cloned packet (creflector.cpp:788), so
    // each peer sees its protocol's own prefix ("XRF672 E" to DExtra,
    // "DCS672 E" to DCS, "REF672 E" to DPlus). But slow-data is generated
    // ONCE per stream, reading m_DvHeader directly — and can't match any
    // specific egress protocol's rewrite anyway because the stream may
    // fan out to all of them.
    //
    // Fix: synthesise a protocol-neutral RPT2 = base reflector callsign
    // ("XLX672", unpatched) + stream module (byte preserved correctly by
    // all ingress paths via SetRpt2Module). Result: "XLX672 E" — well-
    // formed D-Star gateway-style RPT2 that no strict decoder will reject
    // as obviously malformed, and consistent regardless of ingress type.
    {
        char refCallsign[CALLSIGN_LEN + 1];
        g_Reflector.GetCallsign().GetCallsignString(refCallsign);
        uint8 streamModule = (uint8)hdr.GetRpt2Callsign().GetModule();

        // Space-fill first so any slot past the base callsign's length
        // (e.g. if the reflector callsign is shorter than 7 chars) ends
        // up as a D-Star-legal space rather than a stray byte.
        ::memset(raw.RPT2, ' ', sizeof(raw.RPT2));
        size_t refLen = ::strlen(refCallsign);
        if ( refLen > sizeof(raw.RPT2) - 1U )
        {
            refLen = sizeof(raw.RPT2) - 1U;
        }
        ::memcpy(raw.RPT2, refCallsign, refLen);
        raw.RPT2[sizeof(raw.RPT2) - 1U] = streamModule;
    }

    // Copy the first 39 bytes (Flags + RPT2 + RPT1 + UR + MY + SUFFIX) into
    // a local buffer, leave bytes 39-40 uninitialised — CCRC::addCCITT161
    // writes the CRC there below. dstar_header is __packed__ so layout
    // matches the wire order.
    uint8 raw41[41];
    ::memcpy(raw41, &raw, 39);

    // Recompute the CCITT-16 CRC over bytes [0..38] and write it to
    // bytes [39..40] in little-endian order — same algorithm as
    // CDvHeaderPacket::ComputeCrc(), so the slow-data CRC is consistent
    // with how the rest of xlxd handles D-Star header CRCs.
    CCRC::addCCITT161(raw41, sizeof(raw41));

    // Fan out into 8 full length-5 elements covering header bytes 0..39,
    // then a single short length-1 element in the trailing 3-byte slot
    // carrying header byte 40 (the final CRC byte). Total buffer:
    // 8*6 + 3 = 51 bytes, matching what native D-Star radios emit on the
    // wire. After this 51-byte stream is drained by GetSlowData (frames
    // 22-38 of a cycle), the exhaustion path emits pure filler for the
    // remaining frames 39-40.
    for ( int e = 0; e < DSTAR_HEADER_SYNC_FULL_ELEMENTS; e++ )
    {
        m_headerSync[e * 6 + 0] = DSTAR_SLOW_DATA_HEADER_TYPE_BYTE;
        for ( int k = 0; k < 5; k++ )
        {
            // 8 elements × 5 content bytes = 40 slots, covering header
            // bytes 0..39 exactly; no bounds check needed.
            m_headerSync[e * 6 + 1 + k] = raw41[e * 5 + k];
        }
    }
    // Short length-1 tail: [type=0x51][header[40]=CRC_hi][FILLER_PLAIN].
    // The trailing filler byte populates the last slot of frame 38's
    // slow-data (can't emit a partial 3-byte frame), and seamlessly
    // bridges into frames 39+ where GetSlowData returns pure filler.
    m_headerSync[DSTAR_HEADER_SYNC_FULL_ELEMENTS * 6 + 0] = DSTAR_SLOW_DATA_HEADER_TYPE_BYTE_SHORT;
    m_headerSync[DSTAR_HEADER_SYNC_FULL_ELEMENTS * 6 + 1] = raw41[40];
    m_headerSync[DSTAR_HEADER_SYNC_FULL_ELEMENTS * 6 + 2] = FILLER_PLAIN;
    m_bHasHeaderSync = true;
    // Do NOT switch mode here — the caller (CCodecStream::Task) controls
    // when to start emitting from the header buffer via BeginHeaderCycle().
}

////////////////////////////////////////////////////////////////////////////////////////
// cross-mode text composition
//
// Single source of truth for the 20-char string that goes into both:
//   - the scrambler's text buffer (CCodecStream::InitSlowData)
//   - the DCS voice frame's text field at offset 64-83
//     (CDcsProtocol::EncodeDvPacket)
// Keeping this in one place guarantees slow-data and text-field carry
// identical bytes on the wire — if the two ever diverged, strict DCS
// receivers (Icom g2_link → RP2C) could fail to bind the audio.
//
// Output buffer contract: exactly DSTAR_SLOW_DATA_TEXT_LEN (20) bytes
// written, space-padded, NO NUL terminator. Callers that need a NUL
// (e.g. to hand to SetText which uses strlen) must provide a 21-byte
// buffer and write the terminator themselves.
//
// The `destModule` caveat: most ingress paths (NXDN, YSF, DCS, DMR+,
// DMR MMDVM, M17, G3, DExtra-peer, DExtra-rev2) rewrite RPT2 to
// <reflector-call> <linked-module> before OpenStream is called, so
// RPT2's module byte reflects the egress module. Two paths differ:
//   - DExtra non-peer, non-rev2 direct stations (cdextraprotocol.cpp):
//     SetRpt2Module is conditional, so RPT2 may still carry the
//     remote repeater's module letter.
//   - IMRS (cimrsprotocol.cpp): SetRpt2Module is commented out; RPT2's
//     module comes from the DG-ID mapping.
// On those paths the module shown in the composed text may be cosmetic-
// ally "wrong", but 21-frame cycle timing is unaffected — the RP2C's
// audio lock survives.

// static
void CDStarSlowData::ComposeText(const CDvHeaderPacket &hdr, char out[DSTAR_SLOW_DATA_TEXT_LEN])
{
    // Space-fill up front so any unused tail is spaces — not NUL, not
    // garbage — even if snprintf truncates or GetSuffix returns nothing
    // printable.
    ::memset(out, ' ', DSTAR_SLOW_DATA_TEXT_LEN);

    // Source mode — 4-byte SUFFIX stamped by the ingress protocol.
    // Printable ASCII kept; anything else replaced with space so the
    // assembled text is always well-formed.
    char sourceMode[CALLSUFFIX_LEN + 1];
    uint8 suffixBytes[CALLSUFFIX_LEN];
    hdr.GetMyCallsign().GetSuffix(suffixBytes);
    for ( int i = 0; i < CALLSUFFIX_LEN; i++ )
    {
        uint8 b = suffixBytes[i];
        sourceMode[i] = (b >= 0x20 && b < 0x7F) ? (char)b : ' ';
    }
    sourceMode[CALLSUFFIX_LEN] = '\0';

    // Destination module — RPT2's module byte. See block comment above
    // for the two paths where this is cosmetic-only.
    char destModule = hdr.GetRpt2Callsign().GetModule();

    // Reflector callsign (e.g. "XLX672"). GetCallsignString null-terminates.
    char refCall[CALLSIGN_LEN + 1];
    g_Reflector.GetCallsign().GetCallsignString(refCall);

    // Compose. Worst-case length is 4 (suffix) + 5 (" via ") + 8
    // (CALLSIGN_LEN) + 1 (space) + 1 (module) = 19 chars plus NUL, so
    // composed[64] is generous. The subsequent memcpy caps at 20 bytes
    // anyway, so any overflow past 20 chars is silently dropped.
    char composed[64];
    ::snprintf(composed, sizeof(composed), "%s via %s %c",
               sourceMode, refCall, destModule);

    size_t len = ::strlen(composed);
    if ( len > DSTAR_SLOW_DATA_TEXT_LEN ) len = DSTAR_SLOW_DATA_TEXT_LEN;
    ::memcpy(out, composed, len);
    // out[len..DSTAR_SLOW_DATA_TEXT_LEN-1] stay as the memset spaces.
}
