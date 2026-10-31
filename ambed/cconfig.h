//
//  cconfig.h
//  ambed
//
//  Created by Andy Taylor (MW0MWZ) on 31/10/2026.
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
#ifndef cconfig_h
#define cconfig_h

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <ctime>
#include "main.h"

////////////////////////////////////////////////////////////////////////////////////////
// tuning values, indexed by codec where they are per codec

struct CTuning
{
    float   fToPcm[NB_OF_CODECS];       // <MODE>_TO_PCM, in dB
    float   fFromPcm[NB_OF_CODECS];     // PCM_TO_<MODE>, in dB
    bool    bUseAgc;
    float   fAgcTarget;                 // in dBFS
    float   fAgcClamping;               // in dB
    float   fAgcAttack;                 // in ms
    float   fAgcHang;                   // in ms
    float   fAgcRecovery;               // in dB/s for each dB below the target
    float   fAgcRecoveryMin;            // in dB/s
    float   fAgcRecoveryMax;            // in dB/s
    float   fAgcGate;                   // in dBFS
    bool    bUseFilter;
    int     iStreamTimeout;             // in seconds
    bool    bTuningLog;
};

////////////////////////////////////////////////////////////////////////////////////////
// class

class CConfig
{
public:
    // constructor
    CConfig();

    // load, Reload does nothing until the file changes
    void Load(const char *);
    void Reload(void);

    // get, a copy so a stream keeps its values however long it runs
    CTuning GetTuning(void) const;
    std::string GetListenIp(void) const;
    uint16 GetListenPort(void) const            { return m_ListenPort; }
    int GetVoiceTos(void) const                 { return m_VoiceTos; }
    bool IsValidIp(const std::string &) const;
    static const char *GetModeName(uint8);

protected:
    // parsing
    void ReadFile(void);
    void Report(void) const;
    void Apply(void);

protected:
    // data
    mutable std::mutex              m_Mutex;
    std::map<std::string, float>    m_Values;
    std::string                     m_IpValue;      // as read, the socket keeps the one it started with
    std::string                     m_ListenIp;
    uint16                          m_ListenPort;
    std::atomic<int>                m_VoiceTos;     // TOS byte for voice packets, -1 for no marking
    std::string                     m_Filename;
    time_t                          m_LastModified;
    bool                            m_bFileMissing;
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cconfig_h */
