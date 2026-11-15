//
//  main.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 31/10/2015.
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

#ifndef main_h
#define main_h
#include <vector>
#include <array>
#include <map>
#include <queue>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <ctime>
#include <cctype>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <arpa/inet.h>
#include <ifaddrs.h>

// version -----------------------------------------------------
#define VERSION_MAJOR                   2
#define VERSION_MINOR                   7
#define VERSION_REVISION                0

// global ------------------------------------------------------
#define RUN_AS_DAEMON
#define JSON_MONITOR

// Settings marked [ini] can be overridden in xlxd.ini, the values here
// are the defaults used when the file leaves them out.
#define CONFIG_RELOAD_PERIOD            180                                 // in seconds, how often xlxd.ini is checked
#define LISTEN_IP                       "0.0.0.0"                           // [ini] IP to listen on
#define TRANSCODER_HOST                 "127.0.0.1"                         // [ini] IP or hostname of AMBEd

// QoS - set DSCP/TOS on outgoing packets for network prioritization.
// Voice-flow packets (voice frames, headers, EoT) are marked DSCP_VALUE.
// Signalling packets (keepalives, logins, acks, disconnects) are sent
// unmarked (DSCP 0, best-effort) so networks treat them as background
// control traffic and prioritise the voice class over them.
// DSCP values: 0-63 (6 bits). Common choices for voice:
//   46 (EF)  - Expedited Forwarding, highest priority voice
//   34 (AF41)- Assured Forwarding class 4, high priority
//   26 (AF31)- Assured Forwarding class 3, medium-high priority
//   0        - Best effort (default, no priority)
#define DSCP_MARKING_ENABLE             1                                   // [ini] 1 = enable, 0 = disable
#define DSCP_VALUE                      46                                  // [ini] DSCP value (0-63)

// debug -------------------------------------------------------
//#define DEBUG_NO_ERROR_ON_XML_OPEN_FAIL
//#define DEBUG_DUMPFILE
//#define DEBUG_NO_G3_ICMP_SOCKET

// reflector ---------------------------------------------------
#define NB_OF_MODULES                   26
//#define NB_OF_MODULES                   NB_MODULES_MAX

// security ----------------------------------------------------
#define MAX_REJECT_TRACKERS             4096                                // max tracked IPs for progressive ignore

// protocols ---------------------------------------------------
#define NB_OF_PROTOCOLS                 12
#define PROTOCOL_ANY                    -1
#define PROTOCOL_NONE                   0
#define PROTOCOL_DEXTRA                 1
#define PROTOCOL_DPLUS                  2
#define PROTOCOL_DCS                    3
#define PROTOCOL_XLX                    4
#define PROTOCOL_DMRPLUS                5
#define PROTOCOL_DMRMMDVM               6
#define PROTOCOL_YSF                    7
#define PROTOCOL_G3                     8
#define PROTOCOL_IMRS                   9
#define PROTOCOL_M17                    10
#define PROTOCOL_NXDN                   11
#define PROTOCOL_P25                    12

// DExtra - timing matched to ircDDBGateway (10s poll, 60s timeout)
#define DEXTRA_PORT                     30001                               // [ini] UDP port to listen on
#define DEXTRA_KEEPALIVE_PERIOD         10                                  // in seconds
#define DEXTRA_KEEPALIVE_TIMEOUT        (DEXTRA_KEEPALIVE_PERIOD*6)         // in seconds
#define DEXTRA_PEER_KEEPALIVE_PERIOD    10                                  // in seconds
#define DEXTRA_PEER_KEEPALIVE_TIMEOUT   (DEXTRA_PEER_KEEPALIVE_PERIOD*6)    // in seconds
#define DEXTRA_PEER_RECONNECT_PERIOD    5                                   // in seconds
#define DEXTRA_PEER_CONNECT_TIMEOUT     10                                  // in seconds (for handshake)

// DPlus - timing matched to ircDDBGateway (1s poll, 30s timeout)
#define DPLUS_PORT                      20001                               // [ini] UDP port to listen on
#define DPLUS_KEEPALIVE_PERIOD          1                                   // in seconds
#define DPLUS_KEEPALIVE_TIMEOUT         (DPLUS_KEEPALIVE_PERIOD*30)         // in seconds
#define DPLUS_PEER_KEEPALIVE_PERIOD     5                                   // in seconds
#define DPLUS_PEER_KEEPALIVE_TIMEOUT    (DPLUS_PEER_KEEPALIVE_PERIOD*6)     // in seconds
#define DPLUS_PEER_RECONNECT_PERIOD     5                                   // in seconds
#define DPLUS_PEER_CONNECT_TIMEOUT      10                                  // in seconds (for handshake)

// DCS - timing matched to ircDDBGateway (5s poll, 60s timeout)
#define DCS_PORT                        30051                               // [ini] UDP port to listen on
#define DCS_KEEPALIVE_PERIOD            5                                   // in seconds
#define DCS_KEEPALIVE_TIMEOUT           (DCS_KEEPALIVE_PERIOD*12)           // in seconds
#define DCS_PEER_KEEPALIVE_PERIOD       5                                   // in seconds
#define DCS_PEER_KEEPALIVE_TIMEOUT      (DCS_PEER_KEEPALIVE_PERIOD*6)       // in seconds
#define DCS_PEER_RECONNECT_PERIOD       5                                   // in seconds
#define DCS_PEER_CONNECT_TIMEOUT        10                                  // in seconds (for handshake)

// XLX
#define XLX_PORT                        10002                               // [ini] UDP port to listen on
#define XLX_KEEPALIVE_PERIOD            1                                   // in seconds
#define XLX_KEEPALIVE_TIMEOUT           (XLX_KEEPALIVE_PERIOD*30)           // in seconds
#define XLX_RECONNECT_PERIOD            5                                   // in seconds

// DMRPlus (dongle)
#define DMRPLUS_PORT                    8880                                // [ini] UDP port to listen on
#define DMRPLUS_KEEPALIVE_PERIOD        1                                   // in seconds
#define DMRPLUS_KEEPALIVE_TIMEOUT       (DMRPLUS_KEEPALIVE_PERIOD*10)       // in seconds
#define DMRPLUS_REFLECTOR_SLOT          DMR_SLOT2
#define DMRPLUS_REFLECTOR_COLOUR        1

// DMRMmdvm
#define DMRMMDVM_PORT                   62030                               // [ini] UDP port to listen on
#define DMRMMDVM_KEEPALIVE_PERIOD       10                                  // in seconds
#define DMRMMDVM_KEEPALIVE_TIMEOUT      (DMRMMDVM_KEEPALIVE_PERIOD*10)      // in seconds
#define DMRMMDVM_REFLECTOR_SLOT         DMR_SLOT2
#define DMRMMDVM_REFLECTOR_COLOUR       1

// YSF
#define YSF_PORT                        42000                               // [ini] UDP port to listen on
#define YSF_KEEPALIVE_PERIOD            3                                   // in seconds
#define YSF_KEEPALIVE_TIMEOUT           (YSF_KEEPALIVE_PERIOD*10)           // in seconds
#define YSF_DEFAULT_NODE_TX_FREQ        437000000                           // [ini] in Hz
#define YSF_DEFAULT_NODE_RX_FREQ        437000000                           // [ini] in Hz
#define YSF_AUTOLINK_ENABLE             1                                   // [ini] 1 = enable, 0 = disable auto-link
#define YSF_AUTOLINK_MODULE             'B'                                 // [ini] module for client to auto-link to

// YSF Reflector Peering
#define YSF_PEER_KEEPALIVE_PERIOD       5                                   // in seconds (poll interval)
#define YSF_PEER_KEEPALIVE_TIMEOUT      (YSF_PEER_KEEPALIVE_PERIOD*6)       // in seconds
#define YSF_PEER_RECONNECT_PERIOD       5                                   // in seconds
#define YSF_PEER_MAX_ID                 99999                               // max 5-digit YSF reflector ID

// G3 Terminal
#define G3_PRESENCE_PORT                12346                               // [ini] UDP port
#define G3_CONFIG_PORT                  12345                               // [ini] UDP port
#define G3_DV_PORT                      40000                               // [ini] UDP port
#define G3_KEEPALIVE_PERIOD             10                                  // in seconds
#define G3_KEEPALIVE_TIMEOUT            (G3_KEEPALIVE_PERIOD*360)           // in seconds (1 hour)

// IMRS
#define IMRS_PORT                       21110                               // [ini] UDP port to listen on
#define IMRS_KEEPALIVE_PERIOD           30                                  // in seconds
#define IMRS_KEEPALIVE_TIMEOUT          (IMRS_KEEPALIVE_PERIOD*5)           // in seconds
#define IMRS_AUTOLINK_MODULE            'B'                                 // [ini] module for repeaters to link to

// M17
#define M17_PORT                        17000                               // [ini] UDP port to listen on
#define M17_KEEPALIVE_PERIOD            5                                   // in seconds
#define M17_KEEPALIVE_TIMEOUT           (M17_KEEPALIVE_PERIOD*6)            // in seconds

// NXDN Reflector Peering (peer-only, no direct NXDN client support)
#define NXDN_PEER_KEEPALIVE_PERIOD      5                                   // in seconds (poll interval)
#define NXDN_PEER_KEEPALIVE_TIMEOUT     (NXDN_PEER_KEEPALIVE_PERIOD*6)      // in seconds
#define NXDN_PEER_RECONNECT_PERIOD      5                                   // in seconds

// P25 Reflector Peering (peer-only, no direct P25 client support)
#define P25_PEER_KEEPALIVE_PERIOD       5                                   // in seconds (poll interval)
#define P25_PEER_KEEPALIVE_TIMEOUT      (P25_PEER_KEEPALIVE_PERIOD*6)       // in seconds
#define P25_PEER_RECONNECT_PERIOD       5                                   // in seconds

// AMBE gain adjustments to balance YSF and NXDN audio levels with DMR, in dB.
// DMR AMBE is the reflector's internal format: <MODE>_TO_DMR applies to all
// audio arriving over that mode, DMR_TO_<MODE> to all audio sent out over it.
#define GAIN_YSF_TO_DMR                 7                                   // [ini] in dB
#define GAIN_DMR_TO_YSF                 -4                                  // [ini] in dB
#define GAIN_NXDN_TO_DMR                -6                                  // [ini] in dB
#define GAIN_DMR_TO_NXDN                -3                                  // [ini] in dB

// Transcoder server --------------------------------------------
#define TRANSCODER_PORT                 10100                               // [ini] UDP port
#define TRANSCODER_KEEPALIVE_PERIOD     5                                   // in seconds
#define TRANSCODER_KEEPALIVE_TIMEOUT    (TRANSCODER_KEEPALIVE_PERIOD*6)     // in seconds
#define TRANSCODER_AMBEPACKET_TIMEOUT   400                                 // in ms

// codec --------------------------------------------------------
#define CODEC_NONE                      0
#define CODEC_AMBEPLUS                  1                                   // DStar
#define CODEC_AMBE2PLUS                 2                                   // DMR
#define CODEC_CODEC2                    3                                   // M17
#define CODEC_IMBE                      4                                   // P25

// transcoder packet sizes --------------------------------------
#define AMBE_FRAME_SIZE                 9                                   // AMBE+/AMBE2+ frame
#define CODEC2_FRAME_SIZE               8                                   // Codec2 3200bps frame
#define IMBE_FRAME_SIZE                 11                                  // IMBE 88-bit frame
#define TRANSCODER_PACKET_SIZE_LEGACY   11                                  // codec(1)+pid(1)+ambe(9)
#define TRANSCODER_PACKET_SIZE_MULTI    20                                  // codec1(1)+pid(1)+ambe(9)+codec2(1)+codec2(8)
#define TRANSCODER_PACKET_SIZE_C2IN     21                                  // codec1(1)+pid(1)+ambe(9)+codec2(1)+ambe(9) - Codec2 input

// DMRid database -----------------------------------------------
#define DMRIDDB_USE_RLX_SERVER          1                                   // 1 = use http, 0 = use local file
#define DMRIDDB_PATH                    "/xlxd/dmrid.dat"                   // [ini] local file path
#define DMRIDDB_REFRESH_RATE            180                                 // in minutes

// Wires-X node database ----------------------------------------
#define YSFNODEDB_USE_RLX_SERVER        1                                   // 1 = use http, 0 = use local file
#define YSFNODEDB_PATH                  "/xlxd/ysfnode.dat"                 // [ini] local file path
#define YSFNODEDB_REFRESH_RATE          180                                 // in minutes

// NXDN ID database ---------------------------------------------
#define NXDNIDDB_USE_RLX_SERVER         1                                   // 1 = use http, 0 = use local file
#define NXDNIDDB_PATH                   "/xlxd/nxdn.csv"                    // [ini] local file path
#define NXDNIDDB_REFRESH_RATE           720                                 // in minutes (12 hours)

// xml & json reporting -----------------------------------------
#define LASTHEARD_USERS_MAX_SIZE        100
#define XML_UPDATE_PERIOD               10                                  // in seconds
#define JSON_UPDATE_PERIOD              10                                  // in seconds
#define JSON_PORT                       10001                               // [ini] UDP port

// system paths -------------------------------------------------
#define XML_PATH                        "/var/log/xlxd.xml"                 // [ini]
#define PID_PATH                        "/var/log/xlxd.pid"                 // [ini]
#define WHITELIST_PATH                  "/xlxd/xlxd.whitelist"              // [ini]
#define BLACKLIST_PATH                  "/xlxd/xlxd.blacklist"              // [ini]
#define INTERLINK_PATH                  "/xlxd/xlxd.interlink"              // [ini]
#define TERMINAL_PATH                   "/xlxd/xlxd.terminal"               // [ini]
#define DEBUGDUMP_PATH                  "/var/log/xlxd.debug"

// system constants ---------------------------------------------
#define NB_MODULES_MAX                  26

// typedefs -----------------------------------------------------
typedef unsigned char           uint8;
typedef unsigned short          uint16;
typedef unsigned int            uint32;
typedef unsigned int            uint;

// macros -------------------------------------------------------
#define MIN(a,b) 				((a) < (b))?(a):(b)
#define MAX(a,b) 				((a) > (b))?(a):(b)
#define MAKEWORD(low, high)		((uint16)(((uint8)(low)) | (((uint16)((uint8)(high))) << 8)))
#define MAKEDWORD(low, high)	((uint32)(((uint16)(low)) | (((uint32)((uint16)(high))) << 16)))
#define LOBYTE(w)				((uint8)(uint16)(w & 0x00FF))
#define HIBYTE(w)				((uint8)((((uint16)(w)) >> 8) & 0xFF))
#define LOWORD(dw)				((uint16)(uint32)(dw & 0x0000FFFF))
#define HIWORD(dw)				((uint16)((((uint32)(dw)) >> 16) & 0xFFFF))

// global objects -----------------------------------------------
class CReflector;
extern CReflector  g_Reflector;

class CGateKeeper;
extern CGateKeeper g_GateKeeper;

#if (DMRIDDB_USE_RLX_SERVER == 1)
    class CDmridDirHttp;
    extern CDmridDirHttp   g_DmridDir;
#else
    class CDmridDirFile;
    extern CDmridDirFile   g_DmridDir;
#endif

#if (YSFNODEDB_USE_RLX_SERVER == 1)
    class CYsfNodeDirHttp;
    extern CYsfNodeDirHttp   g_YsfNodeDir;
#else
    class CYsfNodeDirFile;
    extern CYsfNodeDirFile   g_YsfNodeDir;
#endif

#if (NXDNIDDB_USE_RLX_SERVER == 1)
    class CNxdnIdDirHttp;
    extern CNxdnIdDirHttp   g_NxdnIdDir;
#else
    class CNxdnIdDirFile;
    extern CNxdnIdDirFile   g_NxdnIdDir;
#endif

class CTranscoder;
extern CTranscoder g_Transcoder;

class CConfig;
extern CConfig g_Config;

#endif /* main_h */
