//
//  ccallsign.cpp
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

#include "main.h"
#include <string.h>
#include <cctype>
#include "cdmriddirfile.h"
#include "cdmriddirhttp.h"
#include "ccallsign.h"

////////////////////////////////////////////////////////////////////////////////////////
// constructors

CCallsign::CCallsign()
{
    // blank all
    ::memset(m_Callsign, ' ', sizeof(m_Callsign));
    ::memset(m_Suffix, ' ', sizeof(m_Suffix));
    m_Module = ' ';
    m_uiDmrid = 0;
}

CCallsign::CCallsign(const char *sz, uint32 dmrid)
{
    // blank all
    ::memset(m_Callsign, ' ', sizeof(m_Callsign));
    ::memset(m_Suffix, ' ', sizeof(m_Suffix));
    m_Module = ' ';
    m_uiDmrid = dmrid;
    
    // and populate
    if ( ::strlen(sz) > 0 )
    {
        // callsign valid
        ::memcpy(m_Callsign, sz, MIN(strlen(sz), sizeof(m_Callsign)-1));
        if ( strlen(sz) >= sizeof(m_Callsign) )
        {
            m_Module = sz[sizeof(m_Callsign)-1];
        }
        // dmrid ok ?
        if ( m_uiDmrid == 0 )
        {
			g_DmridDir.Lock();
			{
            	m_uiDmrid = g_DmridDir.FindDmrid(*this);
            }
            g_DmridDir.Unlock();
        }
    }
    else if ( m_uiDmrid != 0 )
    {
    	g_DmridDir.Lock();
    	{
			const CCallsign *callsign = g_DmridDir.FindCallsign(m_uiDmrid);
			if ( callsign != NULL )
			{
				::memcpy(m_Callsign, callsign->m_Callsign, sizeof(m_Callsign));
			}
		}
		g_DmridDir.Unlock();
    }
}

CCallsign::CCallsign(const CCallsign &callsign)
{
    ::memcpy(m_Callsign, callsign.m_Callsign, sizeof(m_Callsign));
    ::memcpy(m_Suffix, callsign.m_Suffix, sizeof(m_Suffix));
    m_Module = callsign.m_Module;
    m_uiDmrid = callsign.m_uiDmrid;
}

////////////////////////////////////////////////////////////////////////////////////////
// status

bool CCallsign::IsValid(void) const
{
    bool valid = true;
    int i;
    
    // callsign
    // first 3 chars are letter or number but cannot be all number
    int iNum = 0;
    for ( i = 0; i < 3; i++ )
    {
        valid &= IsLetter(m_Callsign[i]) || IsNumber(m_Callsign[i]);
        if ( IsNumber(m_Callsign[i]) )
        {
            iNum++;
        }
    }
    valid &= (iNum < 3);
    // all remaining char are letter, number or space
    for ( ; i < CALLSIGN_LEN; i++)
    {
        valid &= IsLetter(m_Callsign[i]) || IsNumber(m_Callsign[i]) || IsSpace(m_Callsign[i]);
    }
    
    // prefix
    // all chars are number, uppercase or space
    for ( i = 0; i < CALLSUFFIX_LEN; i++ )
    {
        valid &= IsLetter(m_Suffix[i]) || IsNumber(m_Suffix[i]) || IsSpace(m_Suffix[i]);
    }
    
    // module
    // is an letter or space
    valid &= IsLetter(m_Module) || IsSpace(m_Module);
    
    // dmrid is not tested, as it can be NULL
    // if station does is not dmr registered
    
    // done
    return valid;
}

bool CCallsign::HasSuffix(void) const
{
    bool has = false;
    for ( int i = 0; i < CALLSUFFIX_LEN; i++ )
    {
        has |= (m_Suffix[i] != ' ');
    }
    return has;
}

////////////////////////////////////////////////////////////////////////////////////////
// set

void CCallsign::SetCallsign(const char *sz, bool UpdateDmrid)
{
    // set callsign
    ::memset(m_Callsign, ' ', sizeof(m_Callsign));
    m_Module = ' ';
    ::memcpy(m_Callsign, sz, MIN(strlen(sz), sizeof(m_Callsign)-1));
    if ( strlen(sz) >= sizeof(m_Callsign) )
    {
        m_Module = sz[sizeof(m_Callsign)-1];
    }
    // and update dmrid
    if ( UpdateDmrid )
    {
    	g_DmridDir.Lock();
    	{
        	m_uiDmrid = g_DmridDir.FindDmrid(*this);
        }
        g_DmridDir.Unlock();
    }
}

void CCallsign::SetCallsign(const uint8 *buffer, int len, bool UpdateDmrid)
{
    // set callsign
    ::memset(m_Callsign, ' ', sizeof(m_Callsign));
    m_Module = ' ';
    ::memcpy(m_Callsign, buffer, MIN(len, sizeof(m_Callsign)-1));
    for ( int i = 0; i < sizeof(m_Callsign); i++ )
    {
        if ( m_Callsign[i] == 0 )
        {
            m_Callsign[i] = ' ';
        }
    }
    if ( (len >= sizeof(m_Callsign)) && ((char)buffer[sizeof(m_Callsign)-1] != 0) )
    {
        m_Module = (char)buffer[sizeof(m_Callsign)-1];
    }
    if ( UpdateDmrid )
    {
    	g_DmridDir.Lock();
    	{
        	m_uiDmrid = g_DmridDir.FindDmrid(*this);
        }
        g_DmridDir.Unlock();
    }
}

void CCallsign::SetDmrid(uint32 dmrid, bool UpdateCallsign)
{
    m_uiDmrid = dmrid;
    if ( UpdateCallsign )
    {
    	g_DmridDir.Lock();
    	{
			const CCallsign *callsign = g_DmridDir.FindCallsign(dmrid);
			if ( callsign != NULL )
			{
				::memcpy(m_Callsign, callsign->m_Callsign, sizeof(m_Callsign));
			}
		}
		g_DmridDir.Unlock();
    }
}

void CCallsign::SetDmrid(const uint8 *buffer, bool UpdateCallsign)
{
    char sz[9];
    ::memcpy(sz, buffer, 8);
    sz[8] = 0;
    SetDmrid((uint32)::strtol(sz, NULL, 16), UpdateCallsign);
}

void CCallsign::SetModule(char c)
{
    m_Module = c;
}


void CCallsign::SetSuffix(const char *sz)
{
    ::memset(m_Suffix, ' ', sizeof(m_Suffix));
    ::memcpy(m_Suffix, sz, MIN(strlen(sz), sizeof(m_Suffix)));
}

void CCallsign::SetSuffix(const uint8 *buffer, int len)
{
    len = MIN(len, sizeof(m_Suffix));
    ::memset(m_Suffix, ' ', sizeof(m_Suffix));
    ::memcpy(m_Suffix, buffer, len);
}

////////////////////////////////////////////////////////////////////////////////////////
// modify

void CCallsign::PatchCallsign(int off, const uint8 *patch, int len)
{
    if ( off < sizeof(m_Callsign) )
    {
        ::memcpy(m_Callsign, patch, MIN(len, sizeof(m_Callsign) - off));
    }
}


////////////////////////////////////////////////////////////////////////////////////////
// get

void CCallsign::GetCallsign(uint8 *buffer) const
{
    ::memcpy(buffer, m_Callsign, sizeof(m_Callsign));
    if ( HasModule() )
    {
        buffer[sizeof(m_Callsign)-1] = m_Module;
    }
}

void CCallsign::GetCallsignString(char *sz) const
{
    int i;
    for ( i = 0; (i < sizeof(m_Callsign)) && (m_Callsign[i] != ' '); i++ )
    {
        sz[i] = m_Callsign[i];
    }
    sz[i] = 0;
}

void CCallsign::GetSuffix(uint8 *buffer) const
{
    ::memcpy(buffer, m_Suffix, sizeof(m_Suffix));
}

////////////////////////////////////////////////////////////////////////////////////////
// compare

bool CCallsign::HasSameCallsign(const CCallsign &Callsign) const
{
    return (::memcmp(m_Callsign, Callsign.m_Callsign, sizeof(m_Callsign)) == 0);
}

bool CCallsign::HasSameCallsignWithWildcard(const CCallsign &callsign) const
{
    bool same = true;
    bool done = false;
    
    for ( int i = 0; (i < sizeof(m_Callsign)) && same && !done; i++ )
    {
        if ( !(done = ((m_Callsign[i] == '*') || (callsign[i] == '*'))) )
        {
            same &= (m_Callsign[i] == callsign[i]);
        }
    }
    return same;
}

bool CCallsign::HasLowerCallsign(const CCallsign &Callsign) const
{
    return (::memcmp(m_Callsign, Callsign.m_Callsign, sizeof(m_Callsign)) < 0);
}

bool CCallsign::HasSameModule(const CCallsign &Callsign) const
{
    return (m_Module == Callsign.m_Module);
}


////////////////////////////////////////////////////////////////////////////////////////
// operators

bool CCallsign::operator ==(const CCallsign &callsign) const
{
    return ((::memcmp(callsign.m_Callsign, m_Callsign, sizeof(m_Callsign)) == 0) &&
            (m_Module == callsign.m_Module) &&
            (::memcmp(callsign.m_Suffix, m_Suffix, sizeof(m_Suffix)) == 0) &&
            (m_uiDmrid == callsign.m_uiDmrid) );
}

CCallsign::operator const char *() const
{
	char *sz = (char *)(const char *)m_sz;
	
	// empty
	::memset(sz, 0, sizeof(m_sz));	
    // callsign
    sz[CALLSIGN_LEN] = 0;
    ::memcpy(sz, m_Callsign, sizeof(m_Callsign));
    // module
    if ( HasModule() )
    {
    	sz[sizeof(m_Callsign)] = m_Module;
    }
    // suffix
    if ( HasSuffix() )
    {
        ::strcat(sz, " / ");
        ::strncat(sz, m_Suffix, sizeof(m_Suffix));
    }  
      
    // done
    return m_sz;
}

////////////////////////////////////////////////////////////////////////////////////////
// helper

bool CCallsign::IsNumber(char c) const
{
    return ((c >= '0') && (c <= '9'));
}

bool CCallsign::IsLetter(char c) const
{
    return ((c >= 'A') && (c <= 'Z'));
}

bool CCallsign::IsSpace(char c) const
{
    return (c == ' ');
}
