//
//  cxlxcallsignlist.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 31/01/2016.
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

#include <string.h>
#include "main.h"
#include "cpeercallsignlist.h"


////////////////////////////////////////////////////////////////////////////////////////
// file io

bool CPeerCallsignList::LoadFromFile(const char *filename)
{
    bool ok = false;
    char sz[256];

    // and load
    std::ifstream file (filename);
    if ( file.is_open() )
    {
        Lock();

        // empty list
        clear();
        // fill with file content
        while ( file.getline(sz, sizeof(sz)).good()  )
        {
            // remove leading & trailing spaces
            char *szt = TrimWhiteSpaces(sz);

            // crack it
            if ( (::strlen(szt) > 0) && (szt[0] != '#') )
            {
                // 1st token is callsign
                if ( (szt = ::strtok(szt, " ,\t")) != NULL )
                {
                    // Check if this is a YSF, NXDN, P25, REF, XRF, or DCS reflector peer (before creating CCallsign)
                    // YSF peers start with "YSF", NXDN peers start with "NX" followed by a digit
                    // P25 peers start with "P25" followed by a digit
                    // REF peers start with "REF" followed by a digit
                    // XRF peers start with "XRF" followed by a digit
                    // DCS peers start with "DCS" followed by a digit
                    bool isYsfPeer = (::strncmp(szt, "YSF", 3) == 0);
                    bool isNxdnPeer = (::strncmp(szt, "NX", 2) == 0) && (szt[2] >= '0') && (szt[2] <= '9');
                    bool isP25Peer = (::strncmp(szt, "P25", 3) == 0) && (szt[3] >= '0') && (szt[3] <= '9');
                    bool isRefPeer = (::strncmp(szt, "REF", 3) == 0) && (szt[3] >= '0') && (szt[3] <= '9');
                    bool isXrfPeer = (::strncmp(szt, "XRF", 3) == 0) && (szt[3] >= '0') && (szt[3] <= '9');
                    bool isDcsPeer = (::strncmp(szt, "DCS", 3) == 0) && (szt[3] >= '0') && (szt[3] <= '9');

                    // Create callsign - use appropriate method to avoid module parsing issues
                    CCallsign callsign;
                    if ( isYsfPeer )
                    {
                        callsign.SetYsfCallsign(szt);
                    }
                    else if ( isNxdnPeer )
                    {
                        callsign.SetNxdnCallsign(szt);
                    }
                    else if ( isP25Peer )
                    {
                        callsign.SetP25Callsign(szt);
                    }
                    else if ( isRefPeer )
                    {
                        // REF callsigns are standard D-Star callsigns (REFnnn)
                        callsign.SetCallsign(szt);
                    }
                    else if ( isXrfPeer )
                    {
                        // XRF callsigns are standard D-Star callsigns (XRFnnn)
                        callsign.SetCallsign(szt);
                    }
                    else if ( isDcsPeer )
                    {
                        // DCS callsigns are standard D-Star callsigns (DCSnnn)
                        callsign.SetCallsign(szt);
                    }
                    else
                    {
                        callsign.SetCallsign(szt);
                    }

                    // 2nd token is ip (or ip:port for YSF/NXDN peers)
                    char *szip;
                    if ( (szip = ::strtok(NULL, " ,\t")) != NULL )
                    {
                        // 3rd token is modules list
                        if ( (szt = ::strtok(NULL, " ,\t")) != NULL )
                        {
                            uint16 port = 0;
                            char hostOnly[256];
                            ::strncpy(hostOnly, szip, sizeof(hostOnly)-1);
                            hostOnly[sizeof(hostOnly)-1] = '\0';

                            if ( isYsfPeer )
                            {
                                // YSF peers require exactly one module
                                if ( ::strlen(szt) != 1 )
                                {
                                    std::cout << "Gatekeeper: YSF peer " << callsign
                                              << " must have exactly one module, got '" << szt
                                              << "' - skipping" << std::endl;
                                    continue;
                                }

                                // Parse host:port format
                                char *colon = ::strchr(hostOnly, ':');
                                if ( colon != NULL )
                                {
                                    *colon = '\0';
                                    port = (uint16)::atoi(colon + 1);
                                    if ( port == 0 )
                                    {
                                        std::cout << "Gatekeeper: YSF peer " << callsign
                                                  << " has invalid port - skipping" << std::endl;
                                        continue;
                                    }
                                }
                                else
                                {
                                    // Default YSF port
                                    port = YSF_PORT;
                                }
                            }
                            else if ( isNxdnPeer )
                            {
                                // NXDN peers require exactly one module
                                if ( ::strlen(szt) != 1 )
                                {
                                    std::cout << "Gatekeeper: NXDN peer " << callsign
                                              << " must have exactly one module, got '" << szt
                                              << "' - skipping" << std::endl;
                                    continue;
                                }

                                // Parse host:port format - port is required for NXDN
                                char *colon = ::strchr(hostOnly, ':');
                                if ( colon != NULL )
                                {
                                    *colon = '\0';
                                    port = (uint16)::atoi(colon + 1);
                                    if ( port == 0 )
                                    {
                                        std::cout << "Gatekeeper: NXDN peer " << callsign
                                                  << " has invalid port - skipping" << std::endl;
                                        continue;
                                    }
                                }
                                else
                                {
                                    std::cout << "Gatekeeper: NXDN peer " << callsign
                                              << " requires port (host:port format) - skipping" << std::endl;
                                    continue;
                                }
                            }
                            else if ( isP25Peer )
                            {
                                // P25 peers require exactly one module
                                if ( ::strlen(szt) != 1 )
                                {
                                    std::cout << "Gatekeeper: P25 peer " << callsign
                                              << " must have exactly one module, got '" << szt
                                              << "' - skipping" << std::endl;
                                    continue;
                                }

                                // Parse host:port format - port is required for P25
                                char *colon = ::strchr(hostOnly, ':');
                                if ( colon != NULL )
                                {
                                    *colon = '\0';
                                    port = (uint16)::atoi(colon + 1);
                                    if ( port == 0 )
                                    {
                                        std::cout << "Gatekeeper: P25 peer " << callsign
                                                  << " has invalid port - skipping" << std::endl;
                                        continue;
                                    }
                                }
                                else
                                {
                                    std::cout << "Gatekeeper: P25 peer " << callsign
                                              << " requires port (host:port format) - skipping" << std::endl;
                                    continue;
                                }
                            }
                            else if ( isRefPeer )
                            {
                                // REF peers require exactly two module characters (local + remote)
                                if ( ::strlen(szt) != 2 )
                                {
                                    std::cout << "Gatekeeper: REF peer " << callsign
                                              << " must have exactly two module characters (local+remote), got '" << szt
                                              << "' - skipping" << std::endl;
                                    continue;
                                }

                                // Validate module characters
                                if ( !isupper(szt[0]) || !isupper(szt[1]) )
                                {
                                    std::cout << "Gatekeeper: REF peer " << callsign
                                              << " module characters must be A-Z, got '" << szt
                                              << "' - skipping" << std::endl;
                                    continue;
                                }

                                // Parse host:port format - port defaults to DPLUS_PORT if not specified
                                char *colon = ::strchr(hostOnly, ':');
                                if ( colon != NULL )
                                {
                                    *colon = '\0';
                                    port = (uint16)::atoi(colon + 1);
                                    if ( port == 0 )
                                    {
                                        std::cout << "Gatekeeper: REF peer " << callsign
                                                  << " has invalid port - skipping" << std::endl;
                                        continue;
                                    }
                                }
                                else
                                {
                                    // Default DPlus port
                                    port = DPLUS_PORT;
                                }
                            }
                            else if ( isXrfPeer )
                            {
                                // XRF peers require exactly two module characters (local + remote)
                                if ( ::strlen(szt) != 2 )
                                {
                                    std::cout << "Gatekeeper: XRF peer " << callsign
                                              << " must have exactly two module characters (local+remote), got '" << szt
                                              << "' - skipping" << std::endl;
                                    continue;
                                }

                                // Validate module characters
                                if ( !isupper(szt[0]) || !isupper(szt[1]) )
                                {
                                    std::cout << "Gatekeeper: XRF peer " << callsign
                                              << " module characters must be A-Z, got '" << szt
                                              << "' - skipping" << std::endl;
                                    continue;
                                }

                                // Parse host:port format - port defaults to DEXTRA_PORT if not specified
                                char *colon = ::strchr(hostOnly, ':');
                                if ( colon != NULL )
                                {
                                    *colon = '\0';
                                    port = (uint16)::atoi(colon + 1);
                                    if ( port == 0 )
                                    {
                                        std::cout << "Gatekeeper: XRF peer " << callsign
                                                  << " has invalid port - skipping" << std::endl;
                                        continue;
                                    }
                                }
                                else
                                {
                                    // Default DExtra port
                                    port = DEXTRA_PORT;
                                }
                            }
                            else if ( isDcsPeer )
                            {
                                // DCS peers require exactly two module characters (local + remote)
                                if ( ::strlen(szt) != 2 )
                                {
                                    std::cout << "Gatekeeper: DCS peer " << callsign
                                              << " must have exactly two module characters (local+remote), got '" << szt
                                              << "' - skipping" << std::endl;
                                    continue;
                                }

                                // Validate module characters
                                if ( !isupper(szt[0]) || !isupper(szt[1]) )
                                {
                                    std::cout << "Gatekeeper: DCS peer " << callsign
                                              << " module characters must be A-Z, got '" << szt
                                              << "' - skipping" << std::endl;
                                    continue;
                                }

                                // Parse host:port format - port defaults to DCS_PORT if not specified
                                char *colon = ::strchr(hostOnly, ':');
                                if ( colon != NULL )
                                {
                                    *colon = '\0';
                                    port = (uint16)::atoi(colon + 1);
                                    if ( port == 0 )
                                    {
                                        std::cout << "Gatekeeper: DCS peer " << callsign
                                                  << " has invalid port - skipping" << std::endl;
                                        continue;
                                    }
                                }
                                else
                                {
                                    // Default DCS port
                                    port = DCS_PORT;
                                }
                            }

                            // Create the list item
                            CCallsignListItem item(callsign, hostOnly, szt);
                            if ( port != 0 )
                            {
                                item.SetPort(port);
                            }
                            push_back(item);
                        }
                    }
                }
            }
        }
        // close file
        file.close();

        // keep file path
        m_Filename = filename;

        // update time
        GetLastModTime(&m_LastModTime);

        // and done
        Unlock();
        ok = true;
        std::cout << "Gatekeeper loaded " << size() << " lines from " << filename <<  std::endl;
    }
    else
    {
        std::cout << "Gatekeeper cannot find " << filename <<  std::endl;
    }

    return ok;
}

////////////////////////////////////////////////////////////////////////////////////////
// find by IP

CCallsignListItem *CPeerCallsignList::FindListItemByIp(const CIp &ip)
{
    CCallsignListItem *item = NULL;

    for ( int i = 0; i < size(); i++ )
    {
        if ( data()[i].GetIp().GetAddr() == ip.GetAddr() )
        {
            item = &(data()[i]);
            break;
        }
    }
    return item;
}

