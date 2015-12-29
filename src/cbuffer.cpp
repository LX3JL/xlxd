//
//  cbuffer.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 02/11/2015.
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
#include "cprotocol.h"

////////////////////////////////////////////////////////////////////////////////////////
// constructor

CBuffer::CBuffer(uint8 *buffer, int len)
{
    resize(len);
    ::memcpy(data(), buffer, len);
}

////////////////////////////////////////////////////////////////////////////////////////
// set

void CBuffer::Set(uint8 *buffer, int len)
{
    resize(len);
    ::memcpy(data(), buffer, len);
}

void CBuffer::Set(const char *sz)
{
    resize(::strlen(sz)+1);
    ::strcpy((char *)data(), sz);
}

void CBuffer::Append(uint8 *buffer, int len)
{
    int n = (int)size();
    resize(n+len);
    ::memcpy(&(data()[n]), buffer, len);
}

void CBuffer::Append(uint8 ui, int len)
{
    int n = (int)size();
    resize(n+len);
    ::memset(&(data()[n]), ui, len);
}

void CBuffer::Append(uint8 ui)
{
    int n = (int)size();
    resize(n+sizeof(uint8));
    ::memcpy(&(data()[n]), &ui, sizeof(uint8));
}

void CBuffer::Append(uint16 ui)
{
    int n = (int)size();
    resize(n+sizeof(uint16));
    ::memcpy(&(data()[n]), &ui, sizeof(uint16));
}


////////////////////////////////////////////////////////////////////////////////////////
// operation

int CBuffer::Compare(uint8 *buffer, int len) const
{
    int result = -1;
    if ( size() >= len )
    {
        result = ::memcmp(data(), buffer, len);
    }
    return result;
}

int CBuffer::Compare(uint8 *buffer, int off, int len) const
{
    int result = -1;
    if ( size() >= (off+len) )
    {
        result = ::memcmp(&(data()[off]), buffer, len);
    }
    return result;
}


////////////////////////////////////////////////////////////////////////////////////////
// operator

bool CBuffer::operator ==(const CBuffer &Buffer) const
{
    if ( size() == Buffer.size() )
    {
        return (::memcmp((const char *)data(), (const char *)Buffer.data(), size()) == 0);
    }
    return false;
}

bool CBuffer::operator ==(const char *sz) const
{
    if ( size() == ::strlen(sz) )
    {
        return (::memcmp((const char *)data(), sz, size()) == 0);
    }
    return false;
}
