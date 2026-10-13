//
//  nxdndefines.h
//  xlxd
//
//  Created for NXDN Reflector peering support
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

#ifndef nxdndefines_h
#define nxdndefines_h

// NXDN Protocol packet sizes
#define NXDN_POLL_PACKET_SIZE       17      // NXDNP poll/connect packet
#define NXDN_UNLINK_PACKET_SIZE     17      // NXDNU unlink packet
#define NXDN_DATA_PACKET_SIZE       43      // NXDND voice/data packet

// NXDN Protocol packet prefixes
#define NXDN_POLL_PREFIX            "NXDNP"
#define NXDN_UNLINK_PREFIX          "NXDNU"
#define NXDN_DATA_PREFIX            "NXDND"

// NXDN callsign length (in protocol packets)
#define NXDN_CALLSIGN_LENGTH        10

// NXDN voice payload size (within NXDND packet)
#define NXDN_VOICE_PAYLOAD_SIZE     33

// NXDN ID limits
#define NXDN_ID_MAX                 65535   // 16-bit maximum
#define NXDN_ID_MIN                 1

// NXDN default IDs
#define NXDN_ID_NXDN2DMR            9999    // Used for unknown/gateway
#define NXDN_ID_DISCONNECT          9999    // Disconnect/voice announcements

// NXDN data packet flags (byte 9)
#define NXDN_FLAG_GROUP             0x01    // Group call (vs private)
#define NXDN_FLAG_DATA              0x02    // Data (vs voice)
#define NXDN_FLAG_HEADER            0x04    // Header frame
#define NXDN_FLAG_TRAILER           0x08    // End of transmission

// NXDN LICH (Link Information CHannel) field values
#define NXDN_LICH_RFCT_RDCH         2U      // Radio Data Channel
#define NXDN_LICH_FCT_SACCH_SS      2U      // SACCH with Slow Signaling
#define NXDN_LICH_OPTION_STEAL_NONE 3U      // No frame stealing (full voice)
#define NXDN_LICH_DIRECTION_INBOUND 0U      // Inbound direction

// Pre-computed LICH byte for voice frames
// Format: RFCT(2) FCT(2) Option(2) Direction(1) Parity(1)
// RDCH(2)=10, SACCH_SS(2)=10, STEAL_NONE(3)=11, INBOUND(0)=0, Parity=0
// Binary: 10 10 11 00 = 0xAC
#define NXDN_LICH_VOICE             0xAC

#endif /* nxdndefines_h */
