/*
 *	Copyright (C) 2009,2014,2015,2016 Jonathan Naylor, G4KLX
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; version 2 of the License.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 */

#include "cutils.h"

#include <cstdio>
#include <cassert>

void CUtils::byteToBitsBE(unsigned char byte, bool* bits)
{
	assert(bits != NULL);

	bits[0U] = (byte & 0x80U) == 0x80U;
	bits[1U] = (byte & 0x40U) == 0x40U;
	bits[2U] = (byte & 0x20U) == 0x20U;
	bits[3U] = (byte & 0x10U) == 0x10U;
	bits[4U] = (byte & 0x08U) == 0x08U;
	bits[5U] = (byte & 0x04U) == 0x04U;
	bits[6U] = (byte & 0x02U) == 0x02U;
	bits[7U] = (byte & 0x01U) == 0x01U;
}

void CUtils::byteToBitsLE(unsigned char byte, bool* bits)
{
	assert(bits != NULL);

	bits[0U] = (byte & 0x01U) == 0x01U;
	bits[1U] = (byte & 0x02U) == 0x02U;
	bits[2U] = (byte & 0x04U) == 0x04U;
	bits[3U] = (byte & 0x08U) == 0x08U;
	bits[4U] = (byte & 0x10U) == 0x10U;
	bits[5U] = (byte & 0x20U) == 0x20U;
	bits[6U] = (byte & 0x40U) == 0x40U;
	bits[7U] = (byte & 0x80U) == 0x80U;
}

void CUtils::bitsToByteBE(const bool* bits, unsigned char& byte)
{
	assert(bits != NULL);

	byte  = bits[0U] ? 0x80U : 0x00U;
	byte |= bits[1U] ? 0x40U : 0x00U;
	byte |= bits[2U] ? 0x20U : 0x00U;
	byte |= bits[3U] ? 0x10U : 0x00U;
	byte |= bits[4U] ? 0x08U : 0x00U;
	byte |= bits[5U] ? 0x04U : 0x00U;
	byte |= bits[6U] ? 0x02U : 0x00U;
	byte |= bits[7U] ? 0x01U : 0x00U;
}

void CUtils::bitsToByteLE(const bool* bits, unsigned char& byte)
{
	assert(bits != NULL);

	byte  = bits[0U] ? 0x01U : 0x00U;
	byte |= bits[1U] ? 0x02U : 0x00U;
	byte |= bits[2U] ? 0x04U : 0x00U;
	byte |= bits[3U] ? 0x08U : 0x00U;
	byte |= bits[4U] ? 0x10U : 0x00U;
	byte |= bits[5U] ? 0x20U : 0x00U;
	byte |= bits[6U] ? 0x40U : 0x00U;
	byte |= bits[7U] ? 0x80U : 0x00U;
}
