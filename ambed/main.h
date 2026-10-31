//
//  main.h
//  ambed
//
//  Created by Jean-Luc Deltombe (LX3JL) on 13/04/2017.
//  Copyright © 2015 Jean-Luc Deltombe (LX3JL). All rights reserved.
//
// ----------------------------------------------------------------------------
//    This file is part of ambed.
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

#define VERSION_MAJOR                   1
#define VERSION_MINOR                   5
#define VERSION_REVISION                0

// global ------------------------------------------------------
//#define RUN_AS_DAEMON
//#define DEBUG_DUMPFILE

// Network, also overridden by ambed.ini
#define LISTEN_IP                       "127.0.0.1"                         // IP to bind to
#define LISTEN_PORT                     10100                               // UDP port, xlxd's TRANSCODER_PORT must match

// QoS - set DSCP/TOS on outgoing packets for network prioritization, also overridden by ambed.ini
// DSCP values: 0-63 (6 bits). Common choices for voice:
//   46 (EF)  - Expedited Forwarding, highest priority voice
//   34 (AF41)- Assured Forwarding class 4, high priority
//   26 (AF31)- Assured Forwarding class 3, medium-high priority
//   0        - Best effort (default, no priority)
#define DSCP_MARKING_ENABLE             1                                   // 1 = enable, 0 = disable
#define DSCP_VALUE                      46                                  // DSCP value (0-63)

// Codecs -------------------------------------------------------
#define CODEC_NONE                      0
#define CODEC_AMBEPLUS                  1
#define CODEC_AMBE2PLUS                 2
#define CODEC_CODEC2                    3
#define CODEC_IMBE                      4
#define NB_OF_CODECS                    5                                   // CODEC_NONE to CODEC_IMBE

// Frame sizes --------------------------------------------------
#define AMBE_FRAME_SIZE                 9
#define CODEC2_FRAME_SIZE               8
#define IMBE_FRAME_SIZE                 11

// Transcoding gains, in dB. These are the defaults, ambed.ini overrides them at runtime.
// Every path is: decode -> <MODE>_TO_PCM -> bandpass filter -> AGC -> PCM_TO_<MODE> -> encode
#define DSTAR_TO_PCM                    -10.0f                              // D-Star (AMBE+) decode
#define DMR_TO_PCM                      -1.5f                               // DMR, YSF and NXDN (AMBE2+) decode
#define M17_TO_PCM                      -10.75f                             // M17 (Codec2) decode
#define P25_TO_PCM                      -3.75f                              // P25 (IMBE) decode
#define PCM_TO_DSTAR                    +11.5f                              // D-Star (AMBE+) encode
#define PCM_TO_DMR                      0.0f                                // DMR, YSF and NXDN (AMBE2+) encode
#define PCM_TO_M17                      +5.25f                              // M17 (Codec2) encode
#define PCM_TO_P25                      +0.75f                              // P25 (IMBE) encode

// AGC, also overridden by ambed.ini. It levels each speaker: fast down, hold
// through gaps, then up, faster the further a station is below the target
#define USE_AGC                         1                                   // 1=AGC, 0=no AGC
#define AGC_TARGET                      -41.0f                              // speech peak level the AGC aims for, in dBFS
#define AGC_CLAMPING                    9.0f                                // most the AGC can move the gain either way in dB, 0=off
#define AGC_ATTACK                      150.0f                              // in ms, how fast the gain comes down
#define AGC_HANG                        1000.0f                             // in ms, how long the gain holds before it can rise
#define AGC_RECOVERY                    2.0f                                // in dB/s for each dB below the target, how fast the gain rises
#define AGC_RECOVERY_MIN                3.0f                                // in dB/s, slowest rise
#define AGC_RECOVERY_MAX                20.0f                               // in dB/s, fastest rise
#define AGC_GATE                        -60.0f                              // in dBFS, quieter frames don't move the gain
#define AGC_CEILING                     -1.0f                               // in dBFS, the limiter keeps the loudest encoder input below this, not in ambed.ini

// Bandpass filter on PCM, also overridden by ambed.ini
#define USE_BANDPASSFILTER              1                                   // 1=on, 0=off

// Tuning -------------------------------------------------------
#define TUNING_LOG                      1                                   // 1 = log levels when a stream closes, 0 = off, also overridden by ambed.ini
#define CONFIG_RELOAD_PERIOD            180                                 // in seconds, how often ambed.ini is checked
#define TUNING_SILENCE_LEVEL            -55.0f                              // in dBFS, decoded frames below this aren't voice

// Software queue depth limit - prevents unbounded growth if hardware stalls
// At 50 frames/sec, 50 = 1 second of buffering
#define SOFTWARE_QUEUE_MAX_DEPTH        50

// Timeouts, also overridden by ambed.ini ------------------------
#define STREAM_ACTIVITY_TIMEOUT         3                                   // in seconds


////////////////////////////////////////////////////////////////////////////////////////
// typedefs

typedef unsigned char           uint8;
typedef unsigned short          uint16;
typedef unsigned int            uint32;
typedef unsigned int            uint;


////////////////////////////////////////////////////////////////////////////////////////
// macros

#define MIN(a,b) 				(((a) < (b)) ? (a) : (b))
#define MAX(a,b) 				(((a) > (b)) ? (a) : (b))
#define MAKEWORD(low, high)		((uint16)(((uint8)(low)) | (((uint16)((uint8)(high))) << 8)))
#define MAKEDWORD(low, high)	((uint32)(((uint16)(low)) | (((uint32)((uint16)(high))) << 16)))
#define LOBYTE(w)				((uint8)(uint16)(w & 0x00FF))
#define HIBYTE(w)				((uint8)((((uint16)(w)) >> 8) & 0xFF))
#define LOWORD(dw)				((uint16)(uint32)(dw & 0x0000FFFF))
#define HIWORD(dw)				((uint16)((((uint32)(dw)) >> 16) & 0xFFFF))

////////////////////////////////////////////////////////////////////////////////////////
// FIR Filter coefficients computed to be the closest to the recommended filter in 
// Documentation
//
// Following GNU Octave script was used
/*
pkg load signal;
fsamp = 8000;
fcuts = [300 400 3000 3400];
mags = [0 1 0];
devs = [0.2 1 0.2];

[n,Wn,beta,ftype] = kaiserord(fcuts,mags,devs,fsamp);
n = n + rem(n,2);
hh = fir1(n,Wn,ftype,kaiser(n+1,beta),'noscale');

freqz(hh);
[H,f] = freqz(hh,1,1024,fsamp);
plot(f,abs(H))
disp(hh);
grid
*/

const float FILTER_TAPS[] {
-0.05063341f, -0.00060337f, -0.08892498f, -0.02026701f, -0.05940750f, -0.10977641f,  0.03244024f, -0.22304499f,
0.11452865f,  0.72500000f,  0.11452865f, -0.22304499f,  0.03244024f, -0.10977641f, -0.05940750f, -0.02026701f,
-0.08892498f, -0.00060337f, -0.05063341f };
#define FILTER_TAPS_LENGTH          19

////////////////////////////////////////////////////////////////////////////////////////
// global objects

class CAmbeServer;
extern CAmbeServer  g_AmbeServer;

class CVocodecs;
extern CVocodecs    g_Vocodecs;

class CConfig;
extern CConfig      g_Config;

 
////////////////////////////////////////////////////////////////////////////////////////
#endif /* main_h */
