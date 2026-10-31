//
//  cconfig.cpp
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
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sys/stat.h>
#include <arpa/inet.h>
#include "cconfig.h"

////////////////////////////////////////////////////////////////////////////////////////
// global objects

CConfig g_Config;

////////////////////////////////////////////////////////////////////////////////////////
// keys, the defaults are the values in main.h

struct SConfigKey
{
    const char  *name;
    float       fDefault;
    float       fMin;
    float       fMax;
    bool        bInteger;
};

static const SConfigKey g_ConfigKeys[] =
{
    { "LISTEN_PORT",            LISTEN_PORT,          1.0f, 65000.0f, true },
    { "DSCP_MARKING_ENABLE",    DSCP_MARKING_ENABLE,  0.0f, 1.0f, true },
    { "DSCP_VALUE",             DSCP_VALUE,           0.0f, 63.0f, true },
    { "DSTAR_TO_PCM",           DSTAR_TO_PCM,       -40.0f, 40.0f, false },
    { "DMR_TO_PCM",             DMR_TO_PCM,         -40.0f, 40.0f, false },
    { "M17_TO_PCM",             M17_TO_PCM,         -40.0f, 40.0f, false },
    { "P25_TO_PCM",             P25_TO_PCM,         -40.0f, 40.0f, false },
    { "PCM_TO_DSTAR",           PCM_TO_DSTAR,       -40.0f, 40.0f, false },
    { "PCM_TO_DMR",             PCM_TO_DMR,         -40.0f, 40.0f, false },
    { "PCM_TO_M17",             PCM_TO_M17,         -40.0f, 40.0f, false },
    { "PCM_TO_P25",             PCM_TO_P25,         -40.0f, 40.0f, false },
    { "USE_AGC",                USE_AGC,              0.0f,  1.0f, true },
    { "AGC_TARGET",             AGC_TARGET,         -60.0f, -3.0f, false },
    { "AGC_CLAMPING",           AGC_CLAMPING,         0.0f, 20.0f, false },
    { "AGC_ATTACK",             AGC_ATTACK,          10.0f, 2000.0f, false },
    { "AGC_HANG",               AGC_HANG,             0.0f, 10000.0f, false },
    { "AGC_RECOVERY",           AGC_RECOVERY,         0.1f, 10.0f, false },
    { "AGC_RECOVERY_MIN",       AGC_RECOVERY_MIN,     0.5f, 50.0f, false },
    { "AGC_RECOVERY_MAX",       AGC_RECOVERY_MAX,     0.5f, 100.0f, false },
    { "AGC_GATE",               AGC_GATE,           -80.0f, -20.0f, false },
    { "USE_BANDPASSFILTER",     USE_BANDPASSFILTER,   0.0f,  1.0f, true },
    { "STREAM_ACTIVITY_TIMEOUT", STREAM_ACTIVITY_TIMEOUT, 1.0f, 60.0f, true },
    { "TUNING_LOG",             TUNING_LOG,           0.0f,  1.0f, true },
};

static const SConfigKey *FindKey(const std::string &name)
{
    for ( const SConfigKey &key : g_ConfigKeys )
    {
        if ( name == key.name )
        {
            return &key;
        }
    }
    return NULL;
}

static std::string Trim(const std::string &str)
{
    size_t first = str.find_first_not_of(" \t\r");
    if ( first == std::string::npos )
    {
        return "";
    }
    return str.substr(first, str.find_last_not_of(" \t\r") - first + 1);
}

////////////////////////////////////////////////////////////////////////////////////////
// constructor

CConfig::CConfig()
{
    for ( const SConfigKey &key : g_ConfigKeys )
    {
        m_Values[key.name] = key.fDefault;
    }
    m_IpValue = LISTEN_IP;
    m_ListenIp = LISTEN_IP;
    m_ListenPort = LISTEN_PORT;
    m_VoiceTos = -1;
    m_LastModified = 0;
    m_bFileMissing = false;
    Apply();
}

////////////////////////////////////////////////////////////////////////////////////////
// load

void CConfig::Load(const char *filename)
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Filename = filename;

    struct stat st;
    if ( ::stat(m_Filename.c_str(), &st) == 0 )
    {
        m_LastModified = st.st_mtime;
        ReadFile();
    }
    else
    {
        std::cout << "Config: cannot read " << m_Filename << ", using main.h defaults" << std::endl;
        m_bFileMissing = true;
    }
    m_ListenIp = m_IpValue;
    m_ListenPort = (uint16)m_Values.at("LISTEN_PORT");
    Apply();
    Report();
}

void CConfig::Reload(void)
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    if ( m_Filename.empty() )
    {
        return;
    }

    struct stat st;
    if ( ::stat(m_Filename.c_str(), &st) != 0 )
    {
        if ( !m_bFileMissing )
        {
            std::cout << "Config: cannot read " << m_Filename << ", keeping current values" << std::endl;
            m_bFileMissing = true;
        }
        return;
    }

    if ( m_bFileMissing || st.st_mtime != m_LastModified )
    {
        m_bFileMissing = false;
        m_LastModified = st.st_mtime;
        std::cout << "Config: reloading " << m_Filename << ", changes apply from the next stream" << std::endl;
        ReadFile();
        Apply();
        Report();
        if ( m_IpValue != m_ListenIp || (uint16)m_Values.at("LISTEN_PORT") != m_ListenPort )
        {
            std::cout << "Config: LISTEN_IP and LISTEN_PORT changes need a restart" << std::endl;
        }
    }
}

// DSCP can follow the file, it is read for every voice packet
void CConfig::Apply(void)
{
    bool enabled = (m_Values.at("DSCP_MARKING_ENABLE") != 0.0f);
    m_VoiceTos = enabled ? ((int)m_Values.at("DSCP_VALUE") << 2) : -1;
}

////////////////////////////////////////////////////////////////////////////////////////
// parsing

void CConfig::ReadFile(void)
{
    std::ifstream file(m_Filename);
    if ( !file.is_open() )
    {
        std::cout << "Config: cannot open " << m_Filename << ", keeping current values" << std::endl;
        return;
    }

    // a key left out of the file goes back to its default
    std::map<std::string, float> values;
    for ( const SConfigKey &key : g_ConfigKeys )
    {
        values[key.name] = key.fDefault;
    }

    std::string ip = LISTEN_IP;

    std::string line;
    int lineNumber = 0;
    while ( std::getline(file, line) )
    {
        lineNumber++;
        line = Trim(line.substr(0, line.find_first_of("#;")));
        if ( line.empty() || line[0] == '[' )
        {
            continue;
        }

        size_t equals = line.find('=');

        // the one setting that isn't a number
        if ( equals != std::string::npos && Trim(line.substr(0, equals)) == "LISTEN_IP" )
        {
            std::string text = Trim(line.substr(equals + 1));
            if ( IsValidIp(text) )
            {
                ip = text;
            }
            else
            {
                std::cout << "Config: line " << lineNumber << " ignored, bad value for LISTEN_IP: " << text << std::endl;
                ip = m_IpValue;
            }
            continue;
        }

        const SConfigKey *key = (equals == std::string::npos) ? NULL : FindKey(Trim(line.substr(0, equals)));
        if ( key == NULL )
        {
            std::cout << "Config: line " << lineNumber << " ignored, unknown setting: " << line << std::endl;
            continue;
        }

        // a bad value keeps the value already in use, not the default
        values[key->name] = m_Values[key->name];

        std::string text = Trim(line.substr(equals + 1));
        char *end = NULL;
        float value = ::strtof(text.c_str(), &end);
        if ( text.empty() || *end != '\0' || !std::isfinite(value) ||
             value < key->fMin || value > key->fMax || (key->bInteger && value != floorf(value)) )
        {
            std::cout << "Config: line " << lineNumber << " ignored, bad value for " << key->name
                      << " (" << key->fMin << " to " << key->fMax << "): " << text << std::endl;
            continue;
        }
        values[key->name] = value;
    }

    m_Values = values;
    m_IpValue = ip;
}

bool CConfig::IsValidIp(const std::string &ip) const
{
    struct in_addr addr;
    return ::inet_pton(AF_INET, ip.c_str(), &addr) == 1;
}

void CConfig::Report(void) const
{
    std::cout << "Config: LISTEN_IP=" << m_IpValue;
    for ( const SConfigKey &key : g_ConfigKeys )
    {
        std::cout << " " << key.name << "=" << m_Values.at(key.name);
    }
    std::cout << std::endl;
}

////////////////////////////////////////////////////////////////////////////////////////
// get

CTuning CConfig::GetTuning(void) const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    CTuning tuning;

    tuning.fToPcm[CODEC_NONE] = 0.0f;
    tuning.fToPcm[CODEC_AMBEPLUS] = m_Values.at("DSTAR_TO_PCM");
    tuning.fToPcm[CODEC_AMBE2PLUS] = m_Values.at("DMR_TO_PCM");
    tuning.fToPcm[CODEC_CODEC2] = m_Values.at("M17_TO_PCM");
    tuning.fToPcm[CODEC_IMBE] = m_Values.at("P25_TO_PCM");

    tuning.fFromPcm[CODEC_NONE] = 0.0f;
    tuning.fFromPcm[CODEC_AMBEPLUS] = m_Values.at("PCM_TO_DSTAR");
    tuning.fFromPcm[CODEC_AMBE2PLUS] = m_Values.at("PCM_TO_DMR");
    tuning.fFromPcm[CODEC_CODEC2] = m_Values.at("PCM_TO_M17");
    tuning.fFromPcm[CODEC_IMBE] = m_Values.at("PCM_TO_P25");

    tuning.bUseAgc = (m_Values.at("USE_AGC") != 0.0f);
    tuning.fAgcTarget = m_Values.at("AGC_TARGET");
    tuning.fAgcClamping = m_Values.at("AGC_CLAMPING");
    tuning.fAgcAttack = m_Values.at("AGC_ATTACK");
    tuning.fAgcHang = m_Values.at("AGC_HANG");
    tuning.fAgcRecovery = m_Values.at("AGC_RECOVERY");
    tuning.fAgcRecoveryMin = m_Values.at("AGC_RECOVERY_MIN");
    tuning.fAgcRecoveryMax = m_Values.at("AGC_RECOVERY_MAX");
    tuning.fAgcGate = m_Values.at("AGC_GATE");
    tuning.bUseFilter = (m_Values.at("USE_BANDPASSFILTER") != 0.0f);
    tuning.iStreamTimeout = (int)m_Values.at("STREAM_ACTIVITY_TIMEOUT");
    tuning.bTuningLog = (m_Values.at("TUNING_LOG") != 0.0f);

    return tuning;
}

std::string CConfig::GetListenIp(void) const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_ListenIp;
}

const char *CConfig::GetModeName(uint8 uiCodec)
{
    switch ( uiCodec )
    {
        case CODEC_AMBEPLUS:    return "DSTAR";
        case CODEC_AMBE2PLUS:   return "DMR";
        case CODEC_CODEC2:      return "M17";
        case CODEC_IMBE:        return "P25";
        default:                return "NONE";
    }
}
