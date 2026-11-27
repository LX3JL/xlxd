//
//  cconfig.h
//  xlxd
//
//  Created by Andy Taylor (MW0MWZ) on 12/11/2026.
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
#ifndef cconfig_h
#define cconfig_h

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <ctime>
#include "main.h"

////////////////////////////////////////////////////////////////////////////////////////
// class
//
// Settings from xlxd.ini, with the main.h values as defaults.
//
// Start-up settings (callsign, listen IP, ports, paths) are read once by
// Load(), before any thread starts, and never change afterwards, so their
// getters need no lock. Live settings are re-read by Reload() and held in
// atomics, so any thread can read them at any time. The transcoder host is
// live text, so its getter returns a copy under the lock.

class CConfig
{
public:
    // constructor
    CConfig();

    // load, false if the file can't be read or a required setting is missing
    bool Load(const char *);

    // reload, does nothing until the file changes
    void Reload(void);

    // start-up settings
    const char *GetCallsign(void) const         { return m_Callsign.c_str(); }
    const char *GetListenIp(void) const         { return m_ListenIp.c_str(); }
    uint16 GetPort(const char *) const;
    const char *GetPath(const char *) const;

    // live settings
    int GetVoiceTos(void) const                 { return m_VoiceTos; }
    bool IsYsfAutolinkEnabled(void) const       { return m_bYsfAutolink; }
    char GetYsfAutolinkModule(void) const       { return m_cYsfAutolinkModule; }
    char GetImrsAutolinkModule(void) const      { return m_cImrsAutolinkModule; }
    uint32 GetYsfNodeTxFreq(void) const         { return m_uiYsfNodeTxFreq; }
    uint32 GetYsfNodeRxFreq(void) const         { return m_uiYsfNodeRxFreq; }
    // gains as set in xlxd.ini, in dB
    float GetYsfToDmrGain(void) const           { return m_fYsfToDmrGain; }
    float GetDmrToYsfGain(void) const           { return m_fDmrToYsfGain; }
    float GetNxdnToDmrGain(void) const          { return m_fNxdnToDmrGain; }
    float GetDmrToNxdnGain(void) const          { return m_fDmrToNxdnGain; }
    // AMBEd's IP or hostname, as set in xlxd.ini
    std::string GetTranscoderHost(void) const;

protected:
    // parsing
    bool ReadFile(void);
    void Apply(void);
    void Report(void) const;
    void ReportRestartNeeded(void) const;

protected:
    // values as read, keyed SECTION.NAME
    mutable std::mutex                  m_Mutex;
    std::map<std::string, std::string>  m_Values;
    std::string                         m_Filename;
    time_t                              m_LastModified;
    bool                                m_bFileMissing;

    // start-up settings, fixed after Load()
    std::map<std::string, std::string>  m_StartValues;
    std::string                         m_Callsign;
    std::string                         m_ListenIp;

    // live settings
    std::atomic<int>                    m_VoiceTos;     // TOS byte for voice packets, -1 for no marking
    std::atomic<bool>                   m_bYsfAutolink;
    std::atomic<char>                   m_cYsfAutolinkModule;
    std::atomic<char>                   m_cImrsAutolinkModule;
    std::atomic<uint32>                 m_uiYsfNodeTxFreq;
    std::atomic<uint32>                 m_uiYsfNodeRxFreq;
    std::atomic<float>                  m_fYsfToDmrGain;
    std::atomic<float>                  m_fDmrToYsfGain;
    std::atomic<float>                  m_fNxdnToDmrGain;
    std::atomic<float>                  m_fDmrToNxdnGain;
    std::string                         m_TranscoderHost; // guarded by m_Mutex
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cconfig_h */
