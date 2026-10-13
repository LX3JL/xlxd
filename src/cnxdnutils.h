//
//  cnxdnutils.h
//  xlxd
//
//  Created for NXDN voice extraction support
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

#ifndef cnxdnutils_h
#define cnxdnutils_h

#include "main.h"

////////////////////////////////////////////////////////////////////////////////////////
// NXDN voice utilities
//
// NXDN voice payload structure (33 bytes):
//   Byte 0:     LICH (Link Information Channel)
//   Bytes 1-4:  SACCH (Slow Associated Control Channel)
//   Bytes 5-18: Voice block 1 (14 bytes, contains 2 AMBE frames)
//   Bytes 19-32: Voice block 2 (14 bytes, contains 2 AMBE frames)
//
// Each NXDN packet contains 4 AMBE+2 voice frames at 49 bits each.

class CNxdnUtils
{
public:
    // Extract 4 AMBE frames from NXDN voice payload
    // Input: 33-byte NXDN voice payload (from NXDND packet bytes 10-42)
    // Output: 4 x 9-byte AMBE frames via the ambe array pointers
    static void DecodeVoice(const uint8 *payload, uint8 **ambe);

    // Extract single AMBE frame from NXDN voice block
    // Input: voice block data, bit offset (0 or 49)
    // Output: 9-byte AMBE frame
    static void DecodeAmbe(const uint8 *in, uint8 *out, unsigned int offset);

    // Encode AMBE frame into NXDN voice block
    // Input: 9-byte AMBE frame, bit offset (0 or 49)
    // Output: NXDN voice block data
    static void EncodeAmbe(const uint8 *in, uint8 *out, unsigned int offset);

    // Encode 4 AMBE frames into NXDN voice payload
    // Input: 4 x 9-byte AMBE frames via the ambe array pointers
    //        srcId: source radio ID
    //        dstId: destination group/radio ID
    //        packetNum: packet counter for SACCH cycling (0-3 for structure)
    // Output: 33-byte NXDN voice payload with proper SACCH
    static void EncodeVoice(uint8 **ambe, uint8 *payload, uint16_t srcId, uint16_t dstId, uint8_t packetNum);

    // Encode VCALL (Voice Call Header) FACCH1 payload
    // Constructs a valid 33-byte NXDN start-of-transmission payload
    // with proper LICH, SACCH, and FACCH1-encoded VCALL Layer3 message.
    // Input: srcId: source radio ID, dstId: destination group/radio ID
    // Output: 33-byte NXDN payload suitable for header packet
    static void EncodeVcallHeader(uint8 *payload, uint16_t srcId, uint16_t dstId);

    // Encode TX_REL (Transmission Release) FACCH1 payload
    // Constructs a valid 33-byte NXDN end-of-transmission payload
    // with proper LICH, SACCH, and FACCH1-encoded TX_REL Layer3 message.
    // Input: srcId: source radio ID, dstId: destination group/radio ID
    // Output: 33-byte NXDN payload suitable for trailer packet
    static void EncodeTxRel(uint8 *payload, uint16_t srcId, uint16_t dstId);

    // AMBE silence frame
    static const uint8 AMBE_SILENCE[9];

private:
    // Encode a FACCH1 non-superblock payload with the given Layer3 message type
    static void EncodeFacch1Payload(uint8 *payload, uint8_t messageType, uint16_t srcId, uint16_t dstId);

    // Compute NXDN CRC-12 over bit array
    // Input: data buffer, length in bits
    // Returns 12-bit CRC value
    static uint16_t ComputeCRC12(const uint8 *in, unsigned int lengthBits);

    // Encode CRC-12 into bit array starting at the given bit offset
    static void EncodeCRC12(uint8 *in, unsigned int lengthBits);

    // Encode SACCH CRC-6 over 26 bits into the SACCH raw bytes
    // Input: 4-byte SACCH raw data (payload bytes 1-4)
    // Computes CRC-6 over first 26 bits, writes into bits 26-31
    static void EncodeSacchCRC6(uint8 *sacch);
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cnxdnutils_h */
