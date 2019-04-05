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

////////////////////////////////////////////////////////////////////////////////////////
// defines

// version -----------------------------------------------------

#define VERSION_MAJOR                   3
#define VERSION_MINOR                   0
#define VERSION_REVISION                0

// global ------------------------------------------------------

#define RUN_AS_DAEMON
#define JSON_MONITOR
//#define NO_ERROR_ON_XML_OPEN_FAIL
//#define DEBUG_DUMPFILE

// reflector ---------------------------------------------------

#define NB_OF_MODULES                 10
//#define NB_OF_MODULES                   NB_MODULES_MAX

// protocols ---------------------------------------------------

#define NB_OF_PROTOCOLS                 7

#define PROTOCOL_ANY                    -1
#define PROTOCOL_NONE                   0
#define PROTOCOL_DEXTRA                 1
#define PROTOCOL_DEXTRA_OPEN            2
#define PROTOCOL_DPLUS                  3
#define PROTOCOL_DCS                    4
#define PROTOCOL_XLX                    5
#define PROTOCOL_DMRPLUS                6
#define PROTOCOL_DMRMMDVM               7

// DExtra (AMBE output)
#define DEXTRA_PORT                     30001                               // UDP port
#define DEXTRA_KEEPALIVE_PERIOD         3                                   // in seconds
#define DEXTRA_KEEPALIVE_TIMEOUT        (DEXTRA_KEEPALIVE_PERIOD*10)        // in seconds

// DExtra (Codec 2 output)
#define DEXTRA_OPEN_PORT                30201                               // UDP port

// DPlus
#define DPLUS_PORT                      20001                               // UDP port
#define DPLUS_KEEPALIVE_PERIOD          1                                   // in seconds
#define DPLUS_KEEPALIVE_TIMEOUT         (DPLUS_KEEPALIVE_PERIOD*10)         // in seconds

// DCS
#define DCS_PORT                        30051                               // UDP port
#define DCS_KEEPALIVE_PERIOD            1                                   // in seconds
#define DCS_KEEPALIVE_TIMEOUT           (DCS_KEEPALIVE_PERIOD*30)           // in seconds

// XLX
#define XLX_PORT                        10002                               // UDP port
#define XLX_KEEPALIVE_PERIOD            1                                   // in seconds
#define XLX_KEEPALIVE_TIMEOUT           (XLX_KEEPALIVE_PERIOD*30)           // in seconds
#define XLX_RECONNECT_PERIOD            5                                   // in seconds

// DMRPlus (dongle)
#define DMRPLUS_PORT                    8880                                // UDP port
#define DMRPLUS_KEEPALIVE_PERIOD        1                                   // in seconds
#define DMRPLUS_KEEPALIVE_TIMEOUT       (DMRPLUS_KEEPALIVE_PERIOD*10)       // in seconds
#define DMRPLUS_REFLECTOR_SLOT          DMR_SLOT2
#define DMRPLUS_REFLECTOR_COLOUR        1

// DMRMmdvm
#define DMRMMDVM_PORT                   62030                               // UDP port
#define DMRMMDVM_KEEPALIVE_PERIOD       10                                  // in seconds
#define DMRMMDVM_KEEPALIVE_TIMEOUT      (DMRMMDVM_KEEPALIVE_PERIOD*10)      // in seconds
#define DMRMMDVM_REFLECTOR_SLOT         DMR_SLOT2
#define DMRMMDVM_REFLECTOR_COLOUR       1

// Transcoder server --------------------------------------------

#define TRANSCODER_PORT                 10100                               // UDP port
#define TRANSCODER_KEEPALIVE_PERIOD     5                                   // in seconds
#define TRANSCODER_KEEPALIVE_TIMEOUT    30                                  // in seconds
#define TRANSCODER_AMBEPACKET_TIMEOUT   400                                 // in ms

// codec --------------------------------------------------------

#define CODEC_NONE          0
#define CODEC_AMBEPLUS      1                                               // DStar
#define CODEC_AMBE2PLUS     2                                               // DMR
#define CODEC_CODEC2_3200   4                                               // Codec 2 mode 3200
#define CODEC_CODEC2_2400   8                                               // Codec 2 mode 2400


// DMRid database -----------------------------------------------

#define DMRIDDB_USE_RLX_SERVER          1                                   // 1 = use http, 0 = use local file
#define DMRIDDB_PATH                    "/xlxd/dmrid.dat"                   // local file path
#define DMRIDDB_REFRESH_RATE            180                                 // in minutes


// xml & json reporting -----------------------------------------

#define LASTHEARD_USERS_MAX_SIZE        100
#define XML_UPDATE_PERIOD               10                              // in seconds
#define JSON_UPDATE_PERIOD              10                              // in seconds
#define JSON_PORT                       10001

// system paths -------------------------------------------------

#define XML_PATH                        "/var/log/xlxd.xml"
#define WHITELIST_PATH                  "/xlxd/xlxd.whitelist"
#define BLACKLIST_PATH                  "/xlxd/xlxd.blacklist"
#define INTERLINKLIST_PATH              "/xlxd/xlxd.interlink"
#define DEBUGDUMP_PATH                  "/var/log/xlxd.debug"

// system constants ---------------------------------------------

#define NB_MODULES_MAX                  26

////////////////////////////////////////////////////////////////////////////////////////
// typedefs

typedef unsigned char           uint8;
typedef unsigned short          uint16;
typedef unsigned int            uint32;
typedef unsigned int            uint;


////////////////////////////////////////////////////////////////////////////////////////
// macros

#define MIN(a,b) 				((a) < (b))?(a):(b)
#define MAX(a,b) 				((a) > (b))?(a):(b)
#define MAKEWORD(low, high)		((uint16)(((uint8)(low)) | (((uint16)((uint8)(high))) << 8)))
#define MAKEDWORD(low, high)	((uint32)(((uint16)(low)) | (((uint32)((uint16)(high))) << 16)))
#define LOBYTE(w)				((uint8)(uint16)(w & 0x00FF))
#define HIBYTE(w)				((uint8)((((uint16)(w)) >> 8) & 0xFF))
#define LOWORD(dw)				((uint16)(uint32)(dw & 0x0000FFFF))
#define HIWORD(dw)				((uint16)((((uint32)(dw)) >> 16) & 0xFFFF))

////////////////////////////////////////////////////////////////////////////////////////
// global objects

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

class CTranscoder;
extern CTranscoder g_Transcoder;


////////////////////////////////////////////////////////////////////////////////////////
#endif /* main_h */
