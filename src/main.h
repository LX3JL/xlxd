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

#define VERSION_MAJOR               1
#define VERSION_MINOR               4
#define VERSION_REVISION            1

// global ------------------------------------------------------

#define RUN_AS_DAEMON
#define JSON_MONITOR
//#define NO_ERROR_ON_XML_OPEN_FAIL

// reflector ---------------------------------------------------

#define NB_OF_MODULES               8

// protocols ---------------------------------------------------

#define NB_OF_PROTOCOLS             4

#define PROTOCOL_ANY                -1
#define PROTOCOL_NONE               0
#define PROTOCOL_DEXTRA             1
#define PROTOCOL_DPLUS              2
#define PROTOCOL_DCS                3
#define PROTOCOL_XLX                4

// DExtra
#define DEXTRA_PORT                 30001                           // UDP port
#define DEXTRA_KEEPALIVE_PERIOD     3                               // in seconds
#define DEXTRA_KEEPALIVE_TIMEOUT    (DEXTRA_KEEPALIVE_PERIOD*10)    // in seconds

// DPlus
#define DPLUS_PORT                  20001                           // UDP port
#define DPLUS_KEEPALIVE_PERIOD      1                               // in seconds
#define DPLUS_KEEPALIVE_TIMEOUT     (DPLUS_KEEPALIVE_PERIOD*10)     // in seconds

// DCS
#define DCS_PORT                    30051                           // UDP port
#define DCS_KEEPALIVE_PERIOD        1                               // in seconds
#define DCS_KEEPALIVE_TIMEOUT       (DCS_KEEPALIVE_PERIOD*30)       // in seconds

// XLX
#define XLX_PORT                    10002                           // UDP port
#define XLX_KEEPALIVE_PERIOD        1                               // in seconds
#define XLX_KEEPALIVE_TIMEOUT       (XLX_KEEPALIVE_PERIOD*30)       // in seconds
#define XLX_RECONNECT_PERIOD        5                               // in seconds

// xml & json reporting -----------------------------------------

#define LASTHEARD_USERS_MAX_SIZE    100
#define XML_UPDATE_PERIOD           10                              // in seconds
#define JSON_UPDATE_PERIOD          10                              // in seconds
#define JSON_PORT                   10001

// system paths -------------------------------------------------

#define XML_PATH                    "/var/log/xlxd.xml"
#define WHITELIST_PATH              "/xlxd/xlxd.whitelist"
#define BLACKLIST_PATH              "/xlxd/xlxd.blacklist"
#define INTERLINKLIST_PATH          "/xlxd/xlxd.interlink"

// system constants ---------------------------------------------

#define NB_MODULES_MAX              26

////////////////////////////////////////////////////////////////////////////////////////
// typedefs

typedef unsigned char           uint8;
typedef unsigned short          uint16;
typedef unsigned long           uint32;
typedef unsigned int            uint;


////////////////////////////////////////////////////////////////////////////////////////
// macros

#define MIN(a,b) 				((a) < (b))?(a):(b)
#define MAX(a,b) 				((a) > (b))?(a):(b)
#define MAKEWORD(low, high)		((uint16)(((uint8)(low)) | (((uint16)((uint8)(high))) << 8)))
#define LOBYTE(w)				((uint8)(uint16)(w & 0x00FF))
#define HIBYTE(w)				((uint8)((((uint16)(w)) >> 8) & 0xFF))

////////////////////////////////////////////////////////////////////////////////////////
// global objects

class CReflector;
extern CReflector  g_Reflector;

class CGateKeeper;
extern CGateKeeper g_GateKeeper;

////////////////////////////////////////////////////////////////////////////////////////
#endif /* main_h */
