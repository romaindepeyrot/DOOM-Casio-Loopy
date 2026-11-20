// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//	Endianess handling, swapping 16bit and 32bit.
//
//-----------------------------------------------------------------------------


#ifndef __M_SWAP__
#define __M_SWAP__


#ifdef __GNUG__
#pragma interface
#endif


// WAD fields are little-endian and some records are not naturally aligned.
#include <stdint.h>

static inline uint16_t WadU16(const void *field)
{
    const uint8_t *b = field;
    return (uint16_t)(b[0] | (uint16_t)b[1] << 8);
}

static inline uint32_t WadU32(const void *field)
{
    const uint8_t *b = field;
    return (uint32_t)b[0] | (uint32_t)b[1] << 8
         | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
}

#define SHORT(field) ((int16_t)WadU16(&(field)))
#define LONG(field)  ((int32_t)WadU32(&(field)))

#endif
