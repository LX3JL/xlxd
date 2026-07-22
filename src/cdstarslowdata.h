//
//  cdstarslowdata.h
//  xlxd
//
//  D-Star slow-data generator for cross-mode → AMBE+ transcoded egress.
//  Ported from MMDVMHost DStarSlowData.cpp (Jonathan Naylor G4KLX, GPL).
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

#ifndef cdstarslowdata_h
#define cdstarslowdata_h

#include "main.h"

////////////////////////////////////////////////////////////////////////////////////////
// D-Star slow-data wire format reference — summary of the bits this class produces:
//
//   Each AMBE+ voice frame carries 3 bytes of "slow data" interleaved with
//   the 72-bit vocoder payload, giving a 150 bps sub-channel. Frames repeat
//   in a 21-frame cycle (voice frame position == PacketId % 21):
//
//     Frame 0         : sync word 0x55 0x2D 0x16 (emitted unscrambled by
//                       CCodecStream directly — NOT by this class)
//     Frames 1..20    : 3 XOR-scrambled bytes from this class's GetSlowData().
//
//   The 20-character text message is framed into four 6-byte logical
//   segments, each spanning two consecutive slow-data frames:
//
//     Segment 0 : header byte 0x40  + text[ 0.. 4]   (frames 1..2)
//     Segment 1 : header byte 0x41  + text[ 5.. 9]   (frames 3..4)
//     Segment 2 : header byte 0x42  + text[10..14]   (frames 5..6)
//     Segment 3 : header byte 0x43  + text[15..19]   (frames 7..8)
//     Filler    : 0x66 0x66 0x66                     (frames 9..20)
//
//   Every 3-byte frame is XOR-scrambled against {0x70, 0x4F, 0x93}. The
//   scrambler is stateless — the same 3-byte mask applies to every frame
//   independently.
//
//   Filler on the wire (after XOR) is 0x16 0x29 0xF5 — this is the
//   DSTAR_NULL_SLOW_DATA_BYTES pattern emitted by real radios when no
//   text is active.
//
//   CCodecStream calls Reset() at the start of each 21-frame cycle (on
//   the sync frame) so the text message replays once per cycle. At ~420ms
//   per cycle this gives a strict decoder (Icom RP2C) ~2.4 complete copies
//   of the text per second — well above the redundancy threshold.

////////////////////////////////////////////////////////////////////////////////////////
// D-Star slow-data wire constants (exposed so CCodecStream can emit the
// frame-0 sync marker directly without instantiating this class).

#define DSTAR_SLOW_DATA_SYNC_B0     0x55
#define DSTAR_SLOW_DATA_SYNC_B1     0x2D
#define DSTAR_SLOW_DATA_SYNC_B2     0x16

#define DSTAR_SLOW_DATA_TEXT_LEN    20      // characters of text per cycle

// Text-buffer size = 4 segments × 6 bytes per segment (1 type byte +
// 5 text chars). Keep as a named constant so the array declaration
// on m_text[] and the static_assert on m_textPtr's width stay
// coupled — if a future change resizes the text buffer, both sites
// follow automatically via this single definition.
#define DSTAR_TEXT_BUFFER_LEN       24

// Header-sync "late-entry header" slow-data. Native D-Star radios embed
// a scrambled copy of their 41-byte RF header into the slow-data channel
// on alternating cycles. This allows receivers that joined mid-stream (or
// strict hardware decoders that validate the stream) to reconstruct the
// header without the initial UDP header packet. See
// "D-Star slow-data wire format reference" below for the element layout.
//
// Emission schedule — matches native D-Star radio behavior confirmed
// across multi-cycle pcaps (VA3UV via VE3RSD hotspot into XLX405):
//   Frames 22-37: 8 full-length elements, each [0x55][5 content bytes],
//                 covering header bytes 0..39 (Flag1..SUFFIX..CRC[0]).
//   Frame 38:     1 short-length element [0x51][CRC[1]][FILLER_PLAIN],
//                 covering header byte 40 in a single 3-byte slow-data
//                 slot, then padding the remaining slot with filler so
//                 the transition to pure-filler frames 39+ is seamless.
//   Frames 39-40: pure filler (emitted by GetSlowData's exhaustion path).
//
// Type-byte encoding is [type_nibble : length_nibble]:
//   0x55 = HEADER type (0x5), length 5 bytes
//   0x51 = HEADER type (0x5), length 1 byte
// Matches the same nibble scheme used by the text-segment headers
// (0x40/0x41/0x42/0x43 in SetText).
//
// MMDVMHost's original formulation emits 9 full length-5 elements and
// pads the trailing 4 content slots with 0x00, which on the wire produces
// the literal scrambler bytes (70 4F 93) at frame 39 — a pattern that
// never legitimately appears. Strict Icom decoders (RP2C via g2_link)
// appear to distinguish this from real filler (0x66 plain -> 16 29 F5
// wire) when tracking the header-sync -> filler transition.
#define DSTAR_SLOW_DATA_HEADER_TYPE_BYTE         0x55    // full length-5 element
#define DSTAR_SLOW_DATA_HEADER_TYPE_BYTE_SHORT   0x51    // short length-1 element for final CRC byte
#define DSTAR_HEADER_SYNC_FULL_ELEMENTS          8
#define DSTAR_HEADER_SYNC_BUFFER_LEN             (DSTAR_HEADER_SYNC_FULL_ELEMENTS * 6 + 3)  // 8*6 + 3 = 51

// Flag1 value written into the slow-data header-sync copy. Matches what
// real D-Star radios put on air (DSTAR_REPEATER_MASK = 0x40 — "this
// transmission is relayed via a repeater"). Distinct from the value in
// the UDP gateway-protocol header packet (xlxd sets that to 0x00), so
// the slow-data copy presents a valid-looking RF header to strict
// downstream decoders (Icom g2_link / RP2C) that expect radio-format
// Flag1 semantics.
#define DSTAR_SLOW_DATA_HEADER_FLAG1         0x40

////////////////////////////////////////////////////////////////////////////////////////
// forward declarations

class CDvHeaderPacket;  // only referenced by-reference from ComposeText()

////////////////////////////////////////////////////////////////////////////////////////
// class

class CDStarSlowData
{
public:
    CDStarSlowData();

    // Set the 20-character text message. Up to DSTAR_SLOW_DATA_TEXT_LEN
    // chars copied; shorter strings are space-padded; longer are truncated.
    // Also resets the segment pointer, so the next GetSlowData() call
    // starts from segment 0 of the new message.
    void SetText(const char *msg);

    // Arm the header-sync buffer from the given CDvHeaderPacket.
    // Internally converts the packet to dstar_header layout, forces Flag1
    // to DSTAR_SLOW_DATA_HEADER_FLAG1 (0x40) to match native-radio
    // convention, recomputes the CCITT-16 CRC, then fans the 41 bytes out
    // into 8 full length-5 elements + 1 short length-1 tail element
    // (see the emission-schedule comment above DSTAR_HEADER_SYNC_*).
    //
    // Call once per stream alongside SetText(). After both are armed, the
    // caller selects which one GetSlowData() drains via BeginTextCycle()
    // or BeginHeaderCycle() at each 21-frame sync-boundary.
    void SetHeaderSync(const CDvHeaderPacket &hdr);

    // Cycle-boundary API. Call one of these at the start of each 21-frame
    // cycle (triggered by the sync frame, packet_id % 21 == 0). The choice
    // determines what GetSlowData() will emit across the 20 non-sync frames:
    //
    //   BeginTextCycle()   : emit 4 text segments (frames 1-8) then filler
    //                        (frames 9-20). 20 non-sync frames consumed.
    //
    //   BeginHeaderCycle() : emit 8 full + 1 short header-sync elements
    //                        (frames 1-17 in cycle-local numbering) then
    //                        filler (frames 18-20). Matches what native
    //                        D-Star radios emit on "header-sync cycles"
    //                        per multi-cycle pcap analysis.
    //
    // Native-radio policy confirmed by multi-cycle pcap: cycle 0 = text
    // (emitted once, per-transmission), cycles 1+ = header-sync streamed
    // continuously. CCodecStream::Task drives that schedule.
    void BeginTextCycle(void);
    void BeginHeaderCycle(void);

    // Rewind the current mode's pointer without changing which mode is
    // active. Retained for callers that only need to replay the same
    // cycle content (e.g. retransmission / debug). Normal cycle-boundary
    // usage should call BeginTextCycle() / BeginHeaderCycle() explicitly.
    void Reset(void);

    // Emit the next 3-byte slow-data frame. Bytes are XOR-scrambled
    // against {0x70, 0x4F, 0x93}. After the active mode's buffer is
    // exhausted, emits the null-filler pattern 0x16 0x29 0xF5 on every
    // call until the next cycle-boundary call.
    void GetSlowData(uint8 *out3);

    // Compose the 20-char cross-mode slow-data text from header fields.
    // Output is exactly DSTAR_SLOW_DATA_TEXT_LEN (20) bytes, space-padded,
    // no NUL terminator written. Format: "<SUFFIX> via <REFCALL> <MOD>".
    //
    // Used both to seed this class's text buffer (CCodecStream::InitSlowData)
    // and to populate the 20-byte DCS text field at frame offset 64-83
    // (CDcsProtocol::EncodeDvPacket), so slow-data and the wire text field
    // always carry identical content.
    //
    // Source mode = SUFFIX bytes verbatim (printable ASCII kept; anything
    // else falls back to space). Destination module = RPT2's module byte.
    // Reflector callsign = g_Reflector.GetCallsign(). Reads no shared
    // mutable state — the reflector callsign is set at startup and
    // immutable thereafter.
    static void ComposeText(const CDvHeaderPacket &hdr, char out[DSTAR_SLOW_DATA_TEXT_LEN]);

private:
    // Which mode GetSlowData() is currently draining from. Switched at
    // cycle boundaries by BeginTextCycle() / BeginHeaderCycle().
    enum Mode { MODE_TEXT, MODE_HEADER };
    Mode    m_mode;

    // Text mode: 4 segments × 6 bytes = 24. Each segment has a 1-byte type
    // header (0x40..0x43) followed by 5 text characters.
    uint8   m_text[DSTAR_TEXT_BUFFER_LEN];
    uint8   m_textPtr;

    // Header-sync mode: 8 full length-5 elements (48 bytes) + 1 short
    // length-1 tail element (3 bytes) = 51 bytes. See the emission-
    // schedule comment above DSTAR_HEADER_SYNC_BUFFER_LEN for layout.
    // Populated by SetHeaderSync(); drained by GetSlowData() when mode is
    // MODE_HEADER.
    uint8   m_headerSync[DSTAR_HEADER_SYNC_BUFFER_LEN];
    uint8   m_headerSyncPtr;
    bool    m_bHasHeaderSync;

    static const uint8 SCRAMBLER[3];    // {0x70, 0x4F, 0x93}
    static const uint8 FILLER_PLAIN;    // 0x66 (ASCII 'f') — pre-XOR filler byte
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cdstarslowdata_h */
