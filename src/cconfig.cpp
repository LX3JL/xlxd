//
//  cconfig.cpp
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
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <sys/stat.h>
#include "cconfig.h"

////////////////////////////////////////////////////////////////////////////////////////
// global objects

CConfig g_Config;

////////////////////////////////////////////////////////////////////////////////////////
// keys, the defaults are the values in main.h

enum EConfigKind
{
    KIND_INT,           // whole number, fMin to fMax
    KIND_FLOAT,         // number, fMin to fMax
    KIND_MODULE,        // module letter
    KIND_IP,            // IPv4 address
    KIND_HOST,          // IPv4 address or hostname
    KIND_CALLSIGN,      // reflector callsign, XLX + 3 characters
    KIND_PATH           // absolute file path
};

struct SConfigKey
{
    const char  *section;
    const char  *name;
    EConfigKind kind;
    bool        bStartup;       // read once at start-up, a change needs a restart
    double      fDefault;       // default for numbers and modules
    const char  *sDefault;      // default for text, NULL when there is none
    float       fMin;
    float       fMax;
};

static const SConfigKey g_ConfigKeys[] =
{
    { "REFLECTOR", "CALLSIGN",             KIND_CALLSIGN, true,  0, NULL, 0, 0 },
    { "REFLECTOR", "LISTEN_IP",            KIND_IP,       true,  0, LISTEN_IP, 0, 0 },
    { "REFLECTOR", "TRANSCODER_HOST",      KIND_HOST,     false, 0, TRANSCODER_HOST, 0, 0 },

    { "NETWORK",   "DSCP_MARKING_ENABLE",  KIND_INT,      false, DSCP_MARKING_ENABLE, NULL, 0, 1 },
    { "NETWORK",   "DSCP_VALUE",           KIND_INT,      false, DSCP_VALUE, NULL, 0, 63 },

    { "PORTS",     "DEXTRA_PORT",          KIND_INT,      true,  DEXTRA_PORT, NULL, 1, 65535 },
    { "PORTS",     "DPLUS_PORT",           KIND_INT,      true,  DPLUS_PORT, NULL, 1, 65535 },
    { "PORTS",     "DCS_PORT",             KIND_INT,      true,  DCS_PORT, NULL, 1, 65535 },
    { "PORTS",     "XLX_PORT",             KIND_INT,      true,  XLX_PORT, NULL, 1, 65535 },
    { "PORTS",     "DMRPLUS_PORT",         KIND_INT,      true,  DMRPLUS_PORT, NULL, 1, 65535 },
    { "PORTS",     "DMRMMDVM_PORT",        KIND_INT,      true,  DMRMMDVM_PORT, NULL, 1, 65535 },
    { "PORTS",     "YSF_PORT",             KIND_INT,      true,  YSF_PORT, NULL, 1, 65535 },
    { "PORTS",     "IMRS_PORT",            KIND_INT,      true,  IMRS_PORT, NULL, 1, 65535 },
    { "PORTS",     "M17_PORT",             KIND_INT,      true,  M17_PORT, NULL, 1, 65535 },
    { "PORTS",     "G3_PRESENCE_PORT",     KIND_INT,      true,  G3_PRESENCE_PORT, NULL, 1, 65535 },
    { "PORTS",     "G3_CONFIG_PORT",       KIND_INT,      true,  G3_CONFIG_PORT, NULL, 1, 65535 },
    { "PORTS",     "G3_DV_PORT",           KIND_INT,      true,  G3_DV_PORT, NULL, 1, 65535 },
    { "PORTS",     "JSON_PORT",            KIND_INT,      true,  JSON_PORT, NULL, 1, 65535 },
    { "PORTS",     "TRANSCODER_PORT",      KIND_INT,      true,  TRANSCODER_PORT, NULL, 1, 65535 },

    { "YSF",       "AUTOLINK_ENABLE",      KIND_INT,      false, YSF_AUTOLINK_ENABLE, NULL, 0, 1 },
    { "YSF",       "AUTOLINK_MODULE",      KIND_MODULE,   false, YSF_AUTOLINK_MODULE, NULL, 0, 0 },
    { "YSF",       "DEFAULT_NODE_TX_FREQ", KIND_INT,      false, YSF_DEFAULT_NODE_TX_FREQ, NULL, 0, 4.0e9f },
    { "YSF",       "DEFAULT_NODE_RX_FREQ", KIND_INT,      false, YSF_DEFAULT_NODE_RX_FREQ, NULL, 0, 4.0e9f },

    { "IMRS",      "AUTOLINK_MODULE",      KIND_MODULE,   false, IMRS_AUTOLINK_MODULE, NULL, 0, 0 },

    { "GAIN",      "YSF_TO_DMR",           KIND_FLOAT,    false, GAIN_YSF_TO_DMR, NULL, -90, 90 },
    { "GAIN",      "DMR_TO_YSF",           KIND_FLOAT,    false, GAIN_DMR_TO_YSF, NULL, -90, 90 },
    { "GAIN",      "NXDN_TO_DMR",          KIND_FLOAT,    false, GAIN_NXDN_TO_DMR, NULL, -90, 90 },
    { "GAIN",      "DMR_TO_NXDN",          KIND_FLOAT,    false, GAIN_DMR_TO_NXDN, NULL, -90, 90 },

    { "FILES",     "PID_PATH",             KIND_PATH,     true,  0, PID_PATH, 0, 0 },
    { "FILES",     "XML_PATH",             KIND_PATH,     true,  0, XML_PATH, 0, 0 },
    { "FILES",     "WHITELIST_PATH",       KIND_PATH,     true,  0, WHITELIST_PATH, 0, 0 },
    { "FILES",     "BLACKLIST_PATH",       KIND_PATH,     true,  0, BLACKLIST_PATH, 0, 0 },
    { "FILES",     "INTERLINK_PATH",       KIND_PATH,     true,  0, INTERLINK_PATH, 0, 0 },
    { "FILES",     "TERMINAL_PATH",        KIND_PATH,     true,  0, TERMINAL_PATH, 0, 0 },
    { "FILES",     "DMRIDDB_PATH",         KIND_PATH,     true,  0, DMRIDDB_PATH, 0, 0 },
    { "FILES",     "YSFNODEDB_PATH",       KIND_PATH,     true,  0, YSFNODEDB_PATH, 0, 0 },
    { "FILES",     "NXDNIDDB_PATH",        KIND_PATH,     true,  0, NXDNIDDB_PATH, 0, 0 },
};

static std::string KeyId(const SConfigKey &key)
{
    return std::string(key.section) + "." + key.name;
}

static const SConfigKey *FindKey(const std::string &section, const std::string &name)
{
    for ( const SConfigKey &key : g_ConfigKeys )
    {
        if ( section == key.section && name == key.name )
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

static std::string Upper(std::string str)
{
    for ( char &c : str )
    {
        c = (char)::toupper((unsigned char)c);
    }
    return str;
}

// a hostname: labels of letters, digits and hyphens, separated by dots,
// each 1 to 63 characters and not starting or ending with a hyphen
static bool IsHostname(const std::string &text)
{
    if ( text.empty() || text.size() > 253 )
    {
        return false;
    }
    // only digits and dots is a mistyped IP, not a hostname
    if ( text.find_first_not_of("0123456789.") == std::string::npos )
    {
        return false;
    }
    size_t start = 0;
    while ( start <= text.size() )
    {
        size_t end = text.find('.', start);
        if ( end == std::string::npos )
        {
            end = text.size();
        }
        size_t len = end - start;
        if ( len == 0 || len > 63 || text[start] == '-' || text[end - 1] == '-' )
        {
            return false;
        }
        for ( size_t i = start; i < end; i++ )
        {
            if ( !::isalnum((unsigned char)text[i]) && text[i] != '-' )
            {
                return false;
            }
        }
        start = end + 1;
    }
    return true;
}

static std::string FormatNumber(const SConfigKey &key, double value)
{
    std::ostringstream text;
    if ( key.kind == KIND_INT )
    {
        text << (long long)value;
    }
    else
    {
        text << value;
    }
    return text.str();
}

static std::string DefaultValue(const SConfigKey &key)
{
    switch ( key.kind )
    {
        case KIND_INT:
        case KIND_FLOAT:
            return FormatNumber(key, key.fDefault);
        case KIND_MODULE:
            return std::string(1, (char)key.fDefault);
        default:
            return (key.sDefault != NULL) ? key.sDefault : "";
    }
}

// checks a value from the file, and puts it in the form the getters expect
static bool Validate(const SConfigKey &key, const std::string &text, std::string *value)
{
    switch ( key.kind )
    {
        case KIND_INT:
        case KIND_FLOAT:
        {
            char *end = NULL;
            double number = ::strtod(text.c_str(), &end);
            if ( text.empty() || *end != '\0' || !std::isfinite(number) ||
                 number < key.fMin || number > key.fMax ||
                 (key.kind == KIND_INT && number != std::floor(number)) )
            {
                return false;
            }
            *value = (key.kind == KIND_INT) ? FormatNumber(key, number) : text;
            return true;
        }
        case KIND_MODULE:
        {
            if ( text.size() != 1 )
            {
                return false;
            }
            char module = (char)::toupper((unsigned char)text[0]);
            if ( module < 'A' || module >= ('A' + NB_OF_MODULES) )
            {
                return false;
            }
            *value = std::string(1, module);
            return true;
        }
        case KIND_IP:
        {
            struct in_addr addr;
            if ( ::inet_pton(AF_INET, text.c_str(), &addr) != 1 )
            {
                return false;
            }
            *value = text;
            return true;
        }
        case KIND_HOST:
        {
            struct in_addr addr;
            if ( (::inet_pton(AF_INET, text.c_str(), &addr) != 1) && !IsHostname(text) )
            {
                return false;
            }
            *value = text;
            return true;
        }
        case KIND_CALLSIGN:
        {
            std::string callsign = Upper(text);
            if ( callsign.size() != 6 || callsign.compare(0, 3, "XLX") != 0 )
            {
                return false;
            }
            for ( size_t i = 3; i < callsign.size(); i++ )
            {
                if ( !::isalnum((unsigned char)callsign[i]) )
                {
                    return false;
                }
            }
            *value = callsign;
            return true;
        }
        case KIND_PATH:
        {
            if ( text.empty() || text[0] != '/' )
            {
                return false;
            }
            *value = text;
            return true;
        }
    }
    return false;
}

static const char *Range(const SConfigKey &key)
{
    switch ( key.kind )
    {
        case KIND_MODULE:   return "a module letter";
        case KIND_IP:       return "an IPv4 address";
        case KIND_HOST:     return "an IPv4 address or hostname";
        case KIND_CALLSIGN: return "XLX and 3 letters or digits";
        case KIND_PATH:     return "a full path, starting with /";
        default:            return NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// constructor

CConfig::CConfig()
{
    for ( const SConfigKey &key : g_ConfigKeys )
    {
        m_Values[KeyId(key)] = DefaultValue(key);
    }
    m_StartValues = m_Values;
    m_LastModified = 0;
    m_bFileMissing = false;
    Apply();
}

////////////////////////////////////////////////////////////////////////////////////////
// load

bool CConfig::Load(const char *filename)
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Filename = filename;

    struct stat st;
    if ( ::stat(m_Filename.c_str(), &st) != 0 )
    {
        std::cout << "Config: cannot read " << m_Filename << std::endl;
        return false;
    }
    m_LastModified = st.st_mtime;
    if ( !ReadFile() )
    {
        return false;
    }

    if ( m_Values.at("REFLECTOR.CALLSIGN").empty() )
    {
        std::cout << "Config: CALLSIGN is missing from [Reflector] in " << m_Filename << std::endl;
        return false;
    }

    // start-up settings are fixed from here on
    m_StartValues = m_Values;
    m_Callsign = m_Values.at("REFLECTOR.CALLSIGN");
    m_ListenIp = m_Values.at("REFLECTOR.LISTEN_IP");

    Apply();
    Report();
    return true;
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
        std::cout << "Config: reloading " << m_Filename << std::endl;
        if ( ReadFile() )
        {
            Apply();
            Report();
            ReportRestartNeeded();
        }
    }
}

void CConfig::Apply(void)
{
    bool dscp = (std::stoi(m_Values.at("NETWORK.DSCP_MARKING_ENABLE")) != 0);
    m_VoiceTos = dscp ? (std::stoi(m_Values.at("NETWORK.DSCP_VALUE")) << 2) : -1;

    m_bYsfAutolink = (std::stoi(m_Values.at("YSF.AUTOLINK_ENABLE")) != 0);
    m_cYsfAutolinkModule = m_Values.at("YSF.AUTOLINK_MODULE")[0];
    m_uiYsfNodeTxFreq = (uint32)std::stoul(m_Values.at("YSF.DEFAULT_NODE_TX_FREQ"));
    m_uiYsfNodeRxFreq = (uint32)std::stoul(m_Values.at("YSF.DEFAULT_NODE_RX_FREQ"));

    m_cImrsAutolinkModule = m_Values.at("IMRS.AUTOLINK_MODULE")[0];

    m_fYsfToDmrGain = std::stof(m_Values.at("GAIN.YSF_TO_DMR"));
    m_fDmrToYsfGain = std::stof(m_Values.at("GAIN.DMR_TO_YSF"));
    m_fNxdnToDmrGain = std::stof(m_Values.at("GAIN.NXDN_TO_DMR"));
    m_fDmrToNxdnGain = std::stof(m_Values.at("GAIN.DMR_TO_NXDN"));

    m_TranscoderHost = m_Values.at("REFLECTOR.TRANSCODER_HOST");
}

////////////////////////////////////////////////////////////////////////////////////////
// parsing

bool CConfig::ReadFile(void)
{
    std::ifstream file(m_Filename);
    if ( !file.is_open() )
    {
        std::cout << "Config: cannot open " << m_Filename << ", keeping current values" << std::endl;
        return false;
    }

    // a key left out of the file goes back to its default
    std::map<std::string, std::string> values;
    for ( const SConfigKey &key : g_ConfigKeys )
    {
        values[KeyId(key)] = DefaultValue(key);
    }

    std::string section;
    std::string line;
    int lineNumber = 0;
    while ( std::getline(file, line) )
    {
        lineNumber++;
        line = Trim(line.substr(0, line.find_first_of("#;")));
        if ( line.empty() )
        {
            continue;
        }

        if ( line[0] == '[' )
        {
            size_t close = line.find(']');
            section = Upper(Trim(line.substr(1, (close == std::string::npos) ? std::string::npos : close - 1)));
            continue;
        }

        size_t equals = line.find('=');
        std::string name = (equals == std::string::npos) ? "" : Trim(line.substr(0, equals));
        // TRANSCODER_IP was the name in the first 2.7.0 builds
        if ( section == "REFLECTOR" && name == "TRANSCODER_IP" )
        {
            std::cout << "Config: line " << lineNumber << ", TRANSCODER_IP is now called TRANSCODER_HOST, please rename it" << std::endl;
            name = "TRANSCODER_HOST";
        }
        const SConfigKey *key = (equals == std::string::npos) ? NULL : FindKey(section, name);
        if ( key == NULL )
        {
            std::cout << "Config: line " << lineNumber << " ignored, unknown setting in [" << section << "]: " << line << std::endl;
            continue;
        }

        // a bad value keeps the value already in use, not the default
        std::string id = KeyId(*key);
        values[id] = m_Values[id];

        std::string text = Trim(line.substr(equals + 1));
        std::string value;
        if ( !Validate(*key, text, &value) )
        {
            std::cout << "Config: line " << lineNumber << " ignored, bad value for " << key->name << " (";
            if ( Range(*key) != NULL )
            {
                std::cout << Range(*key);
            }
            else
            {
                std::cout << key->fMin << " to " << key->fMax;
            }
            std::cout << "): " << text << std::endl;
            continue;
        }
        values[id] = value;
    }

    m_Values = values;
    return true;
}

void CConfig::Report(void) const
{
    const char *section = "";
    for ( const SConfigKey &key : g_ConfigKeys )
    {
        if ( ::strcmp(section, key.section) != 0 )
        {
            if ( *section != '\0' )
            {
                std::cout << std::endl;
            }
            section = key.section;
            std::cout << "Config: [" << section << "]";
        }
        std::cout << " " << key.name << "=" << m_Values.at(KeyId(key));
    }
    std::cout << std::endl;
}

void CConfig::ReportRestartNeeded(void) const
{
    for ( const SConfigKey &key : g_ConfigKeys )
    {
        std::string id = KeyId(key);
        if ( key.bStartup && m_Values.at(id) != m_StartValues.at(id) )
        {
            std::cout << "Config: " << key.name << " changed to " << m_Values.at(id)
                      << ", still using " << m_StartValues.at(id) << " until a restart" << std::endl;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// get

std::string CConfig::GetTranscoderHost(void) const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_TranscoderHost;
}

uint16 CConfig::GetPort(const char *name) const
{
    return (uint16)std::stoi(m_StartValues.at(std::string("PORTS.") + name));
}

const char *CConfig::GetPath(const char *name) const
{
    return m_StartValues.at(std::string("FILES.") + name).c_str();
}
