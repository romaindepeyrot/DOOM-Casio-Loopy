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
//
// $Log:$
//
// DESCRIPTION:
//	Main loop menu stuff.
//	Default Config File.
//	PCX Screenshots.
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: m_misc.c,v 1.6 1997/02/03 22:45:10 b1 Exp $";

#include <ctype.h>


#include "doomdef.h"

#include "z_zone.h"

#include "m_swap.h"
#include "m_argv.h"

#include "w_wad.h"

#include "i_system.h"
#include "i_video.h"
#include "v_video.h"

#include "hu_stuff.h"

// State.
#include "doomstat.h"

// Data.
#include "dstrings.h"

#include "m_misc.h"

//
// M_DrawText
// Returns the final X coordinate
// HU_Init must have been called to init the font
//
extern patch_t*		hu_font[HU_FONTSIZE];

int
M_DrawText
( int		x,
  int		y,
  boolean	direct,
  char*		string )
{
    int 	c;
    int		w;

    while (*string)
    {
	c = toupper(*string) - HU_FONTSTART;
	string++;
	if (c < 0 || c >= HU_FONTSIZE)
	{
	    x += 4;
	    continue;
	}
		
	w = SHORT (hu_font[c]->width);
	if (x+w > SCREENWIDTH)
	    break;
	if (direct)
	    V_DrawPatchDirect(x, y, 0, hu_font[c]);
	else
	    V_DrawPatch(x, y, 0, hu_font[c]);
	x+=w;
    }

    return x;
}




// Defaults for the controller bindings supplied by the platform.
extern int key_right, key_left, key_up, key_down;
extern int key_strafeleft, key_straferight;
extern int key_fire, key_use, key_strafe, key_speed;
extern int showMessages, screenblocks, detailLevel;

void M_LoadDefaults(void)
{
    key_right = KEY_RIGHTARROW;
    key_left = KEY_LEFTARROW;
    key_up = KEY_UPARROW;
    key_down = KEY_DOWNARROW;
    key_strafeleft = ',';
    key_straferight = '.';
    key_fire = KEY_RCTRL;
    key_use = ' ';
    key_strafe = KEY_RALT;
    key_speed = KEY_RSHIFT;
    showMessages = 1;
    screenblocks = 10;
    detailLevel = 1;
}
