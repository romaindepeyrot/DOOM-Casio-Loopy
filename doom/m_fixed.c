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
// $Log:$
//
// DESCRIPTION:
//	Fixed point implementation.
//
//-----------------------------------------------------------------------------


static const char
rcsid[] = "$Id: m_bbox.c,v 1.1 1997/02/03 22:45:10 b1 Exp $";

#include <stdint.h>

#include "doomtype.h"
#include "i_system.h"

#ifdef __GNUG__
#pragma implementation "m_fixed.h"
#endif
#include "m_fixed.h"




static inline uint32_t MultiplyHalfWords(uint16_t a, uint16_t b)
{
#ifdef __sh__
    uint32_t product;
    __asm__("mulu.w %1,%2\n\tsts macl,%0"
            : "=r"(product) : "r"(a), "r"(b) : "macl");
    return product;
#else
    return (uint32_t)a * b;
#endif
}

// SH-1 provides 16-bit multiply instructions. The four half-word
// products reconstruct the fixed-point result without a 64-bit multiply.
fixed_t FixedMul(fixed_t a, fixed_t b)
{
    uint16_t alo = a;
    uint16_t blo = b;
    uint16_t ahi = (uint32_t)a >> 16;
    uint16_t bhi = (uint32_t)b >> 16;
    uint32_t high = MultiplyHalfWords(ahi, bhi);

    if (a < 0)
        high -= blo;
    if (b < 0)
        high -= alo;

    return (fixed_t)((high << 16) + MultiplyHalfWords(ahi, blo) +
                     MultiplyHalfWords(bhi, alo) + ((MultiplyHalfWords(alo, blo)) >> 16));
}

static uint32_t Magnitude(fixed_t value)
{
    return value < 0 ? 0u - (uint32_t)value : (uint32_t)value;
}

fixed_t FixedDiv(fixed_t a, fixed_t b)
{
    if ((Magnitude(a) >> 14) >= Magnitude(b))
        return (a ^ b) < 0 ? MININT : MAXINT;
    return FixedDiv2(a, b);
}

fixed_t FixedDiv2(fixed_t a, fixed_t b)
{
    uint64_t quotient;
    int negative = (a ^ b) < 0;

    if (!b)
        I_Error("FixedDiv: divide by zero");

    quotient = ((uint64_t)Magnitude(a) << FRACBITS) / Magnitude(b);
    if (quotient > (uint32_t)MAXINT + negative)
        I_Error("FixedDiv: result out of range");

    return negative ? (fixed_t)(0u - (uint32_t)quotient) : (fixed_t)quotient;
}
