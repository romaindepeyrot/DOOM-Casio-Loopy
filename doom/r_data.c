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
// Revision 1.3  1997/01/29 20:10
// DESCRIPTION:
//	Preparation of data for rendering,
//	generation of lookups, caching, retrieval by name.
//
//-----------------------------------------------------------------------------


static const char
rcsid[] = "$Id: r_data.c,v 1.4 1997/02/03 16:47:55 b1 Exp $";

#include "i_system.h"
#include "z_zone.h"

#include "m_swap.h"

#include "w_wad.h"

#include "doomdef.h"
#include "r_local.h"
#include "p_local.h"

#include "doomstat.h"
#include "r_sky.h"

#include <stdint.h>
#include <strings.h>
#include <stddef.h>
#include "r_data.h"

enum { COMPOSITE_COLUMN = 254, EMPTY_COLUMN = 255 };

typedef struct
{
    const patch_t *picture;
    short originx;
    short originy;
} texpatch_t;

typedef struct
{
    const char *name;
    uint16_t width;
    uint16_t height;
    uint16_t widthmask;
    uint16_t patchcount;
    byte *columnpatches;
    uint16_t *composite;
    int composite_attempt;
    texpatch_t patches[];
} texture_t;

int		firstflat;
int		lastflat;
int		numflats;

int		firstpatch;
int		lastpatch;
int		numpatches;

int		firstspritelump;
int		lastspritelump;
int		numspritelumps;

int		numtextures;
static texture_t **textures;


fixed_t* textureheight;

// for global animation
int*		flattranslation;
int*		texturetranslation;

// needed for pre rendering
fixed_t*	spritewidth;	
fixed_t*	spriteoffset;
fixed_t*	spritetopoffset;

lighttable_t	*colormaps;



static const column_t *PatchColumn(const texpatch_t *patch, int x)
{
    int column = x - patch->originx;

    if (column < 0 || column >= SHORT(patch->picture->width))
        return NULL;

    return (const column_t *)((const byte *)patch->picture +
                             LONG(patch->picture->columnofs[column]));
}

static void DrawPatchColumn(const column_t *post, byte *pixels, byte *opaque,
                            int origin, int height)
{
    while (post->topdelta != 0xff)
    {
        int top = origin + post->topdelta;
        int count = post->length;
        const byte *source = (const byte *)post + 3;

        if (top < 0)
        {
            source += -top < count ? -top : count;
            count += top;
            top = 0;
        }

        if (count > height - top)
            count = height - top;

        if (count > 0)
        {
            memcpy(pixels + top, source, count);
            if (opaque)
                memset(opaque + top, 1, count);
        }
        post = (const column_t *)((const byte *)post + post->length + 4);
    }
}

static void CompositeColumn(const texture_t *texture, int x, byte *pixels,
                            byte *opaque, int height)
{
    int i;

    if (height > texture->height)
        height = texture->height;
    for (i = 0; i < texture->patchcount; i++)
    {
        const texpatch_t *patch = &texture->patches[i];
        const column_t *post = PatchColumn(patch, x);

        if (post)
            DrawPatchColumn(post, pixels, opaque, patch->originy, height);
    }
}

static byte ColumnPatch(const texture_t *texture, int x)
{
    if (texture->columnpatches)
        return texture->columnpatches[x];
    return PatchColumn(&texture->patches[0], x) ? 0 : EMPTY_COLUMN;
}

// Wall column drawers wrap their texture coordinate at 128 pixels.
enum { WALL_COLUMN_HEIGHT = 128 };
extern int framecount;

static void GenerateComposite(texture_t *texture)
{
    int x;
    int count = 0;
    int width = texture->widthmask + 1;
    uint16_t *lookup;
    byte *pixels;

    for (x = 0; x < width; x++)
        if (texture->columnpatches[x] == COMPOSITE_COLUMN)
            count++;

    int bytes = width * sizeof(*lookup) + count * WALL_COLUMN_HEIGHT;
    if (bytes + (int)sizeof(memblock_t) > Z_FreeMemory())
        return;

    lookup = Z_TryMalloc(bytes, PU_CACHE, &texture->composite);
    if (!lookup)
        return;

    pixels = (byte *)(lookup + width);
    memset(pixels, 0, count * WALL_COLUMN_HEIGHT);

    count = 0;
    for (x = 0; x < width; x++)
        if (texture->columnpatches[x] == COMPOSITE_COLUMN)
        {
            lookup[x] = count;
            CompositeColumn(texture, x, pixels + count * WALL_COLUMN_HEIGHT,
                            NULL, WALL_COLUMN_HEIGHT);
            count++;
        }
}

byte *R_GetColumn(int texnum, int x)
{
    static const byte blank[WALL_COLUMN_HEIGHT];
    static byte uncached[WALL_COLUMN_HEIGHT];
    texture_t *texture = textures[texnum];
    byte patch;

    x &= texture->widthmask;
    patch = ColumnPatch(texture, x);
    if (patch == EMPTY_COLUMN)
        return (byte *)blank;
    if (patch != COMPOSITE_COLUMN)
        return (byte *)PatchColumn(&texture->patches[patch], x) + 3;

    if (!texture->composite && texture->composite_attempt != framecount)
    {
        texture->composite_attempt = framecount;
        GenerateComposite(texture);
    }

    if (texture->composite)
        return (byte *)(texture->composite + texture->widthmask + 1) +
               texture->composite[x] * WALL_COLUMN_HEIGHT;

    // The current renderer consumes this scratch column before asking for another.
    memset(uncached, 0, sizeof(uncached));
    CompositeColumn(texture, x, uncached, NULL, WALL_COLUMN_HEIGHT);
    return uncached;
}

column_t *R_GetMaskedColumn(int texnum, int x)
{
    static const byte end_post = 0xff;
    static byte posts[255 * 3 + 1];
    texture_t *texture = textures[texnum];
    byte patch;
    byte pixels[255];
    byte opaque[255];
    byte *out = posts;
    int y = 0;

    x &= texture->widthmask;
    patch = ColumnPatch(texture, x);
    if (patch == EMPTY_COLUMN)
        return (column_t *)&end_post;
    if (patch != COMPOSITE_COLUMN)
        return (column_t *)PatchColumn(&texture->patches[patch], x);
    if (texture->height > sizeof(pixels))
        I_Error("Masked texture is too tall");

    memset(opaque, 0, texture->height);
    CompositeColumn(texture, x, pixels, opaque, texture->height);
    while (y < texture->height)
    {
        int top;
        int count;

        while (y < texture->height && !opaque[y])
            y++;
        if (y == texture->height)
            break;
        top = y;
        while (y < texture->height && opaque[y])
            y++;
        count = y - top;

        *out++ = top;
        *out++ = count;
        *out++ = 0;
        memcpy(out, pixels + top, count);
        out += count;
        *out++ = 0;
    }
    *out = 0xff;
    return (column_t *)posts;
}

static unsigned TextureCount(const byte *data, unsigned length)
{
    if (length < 4 || WadU32(data) > (length - 4) / 4)
        I_Error("Invalid texture table");
    return WadU32(data);
}

static void InitTextureTable(const byte *data, unsigned length, int first,
                             unsigned count, int *patchlookup, unsigned patches)
{
    unsigned i;

    for (i = 0; i < count; i++)
    {
        unsigned offset = WadU32(data + 4 + i * 4);
        const byte *definition;
        unsigned width, height, patchcount, j, viewwidth;
        texture_t *texture;

        if (offset > length || length - offset < 22)
            I_Error("Invalid texture directory offset");
        definition = data + offset;
        width = WadU16(definition + 12);
        height = WadU16(definition + 14);
        patchcount = WadU16(definition + 20);
        if (!width || width > 32767 || !height || height > 32767 ||
            !patchcount || patchcount > COMPOSITE_COLUMN ||
            patchcount > (length - offset - 22) / 10)
            I_Error("Invalid texture dimensions or patch count");

        viewwidth = 1;
        while (viewwidth * 2 <= width)
            viewwidth *= 2;
        texture = Z_Malloc(sizeof(*texture) + patchcount * sizeof(texpatch_t) +
                           (patchcount > 1 ? viewwidth : 0), PU_STATIC, 0);
        textures[first + i] = texture;
        texture->name = (const char *)definition;
        texture->width = width;
        texture->height = height;
        texture->widthmask = viewwidth - 1;
        texture->patchcount = patchcount;
        texture->columnpatches = NULL;
        texture->composite = NULL;
        texture->composite_attempt = -1;
        textureheight[first + i] = height * FRACUNIT;

        if (patchcount > 1)
        {
            texture->columnpatches = (byte *)(texture->patches + patchcount);
            memset(texture->columnpatches, EMPTY_COLUMN, viewwidth);
        }

        for (j = 0; j < patchcount; j++)
        {
            const byte *record = definition + 22 + j * 10;
            unsigned patchnum = WadU16(record + 4);
            texpatch_t *patch = &texture->patches[j];
            int x, end;

            if (patchnum >= patches || patchlookup[patchnum] < 0)
                I_Error("Missing patch in texture %.8s", texture->name);

            patch->originx = (short)WadU16(record);
            patch->originy = (short)WadU16(record + 2);
            patch->picture = W_LumpData(patchlookup[patchnum]);
            if (!texture->columnpatches)
                continue;

            x = patch->originx < 0 ? 0 : patch->originx;
            end = patch->originx + SHORT(patch->picture->width);
            if (end > (int)viewwidth)
                end = viewwidth;

            for (; x < end; x++)
                texture->columnpatches[x] =
                    texture->columnpatches[x] == EMPTY_COLUMN ? j : COMPOSITE_COLUMN;
        }
    }
}

static void R_InitTextures(void)
{
    int pnames = W_GetNumForName("PNAMES");
    int tex1 = W_GetNumForName("TEXTURE1");
    int tex2 = W_CheckNumForName("TEXTURE2");
    const byte *names = W_LumpData(pnames);
    const byte *data1 = W_LumpData(tex1);
    const byte *data2 = tex2 >= 0 ? W_LumpData(tex2) : NULL;
    unsigned names_length = W_LumpLength(pnames);
    unsigned length1 = W_LumpLength(tex1);
    unsigned length2 = tex2 >= 0 ? W_LumpLength(tex2) : 0;
    unsigned count1 = TextureCount(data1, length1);
    unsigned count2 = data2 ? TextureCount(data2, length2) : 0;
    unsigned patches, i;
    int *patchlookup;
    char name[9];

    if (names_length < 4 || WadU32(names) > (names_length - 4) / 8)
        I_Error("Invalid patch names table");

    patches = WadU32(names);
    patchlookup = Z_Malloc(patches * sizeof(*patchlookup), PU_STATIC, 0);
    name[8] = 0;
    for (i = 0; i < patches; i++)
    {
        memcpy(name, names + 4 + i * 8, 8);
        patchlookup[i] = W_CheckNumForName(name);
    }

    numtextures = count1 + count2;
    textures = Z_Malloc(numtextures * sizeof(*textures), PU_STATIC, 0);
    textureheight = Z_Malloc(numtextures * sizeof(*textureheight), PU_STATIC, 0);
    InitTextureTable(data1, length1, 0, count1, patchlookup, patches);
    if (data2)
        InitTextureTable(data2, length2, count1, count2, patchlookup, patches);
    Z_Free(patchlookup);

    texturetranslation = Z_Malloc((numtextures + 1) * sizeof(*texturetranslation),
                                  PU_STATIC, 0);
    for (i = 0; i < (unsigned)numtextures; i++)
        texturetranslation[i] = i;
}


//
// R_InitFlats
//
void R_InitFlats (void)
{
    int		i;
	
    firstflat = W_GetNumForName ("F_START") + 1;
    lastflat = W_GetNumForName ("F_END") - 1;
    numflats = lastflat - firstflat + 1;
	
    // Create translation table for global animation.
    flattranslation = Z_Malloc ((numflats+1)*4, PU_STATIC, 0);
    
    for (i=0 ; i<numflats ; i++)
	flattranslation[i] = i;
}


//
// R_InitSpriteLumps
// Finds the width and hoffset of all sprites in the wad,
//  so the sprite does not need to be cached completely
//  just for having the header info ready during rendering.
//
void R_InitSpriteLumps (void)
{
    int		i;
    patch_t	*patch;
	
    firstspritelump = W_GetNumForName ("S_START") + 1;
    lastspritelump = W_GetNumForName ("S_END") - 1;
    
    numspritelumps = lastspritelump - firstspritelump + 1;
    spritewidth = Z_Malloc (numspritelumps*4, PU_STATIC, 0);
    spriteoffset = Z_Malloc (numspritelumps*4, PU_STATIC, 0);
    spritetopoffset = Z_Malloc (numspritelumps*4, PU_STATIC, 0);
	
    for (i=0 ; i< numspritelumps ; i++)
    {
	if (!(i&63))
	    printf (".");

	patch = W_CacheLumpNum (firstspritelump+i, PU_CACHE);
	spritewidth[i] = SHORT(patch->width)<<FRACBITS;
	spriteoffset[i] = SHORT(patch->leftoffset)<<FRACBITS;
	spritetopoffset[i] = SHORT(patch->topoffset)<<FRACBITS;
    }
}



//
// R_InitColormaps
//
void R_InitColormaps (void)
{
    // The cartridge packer aligns the ROM color maps to 256 bytes.
    colormaps = W_CacheLumpName("COLORMAP", PU_STATIC);
}



//
// R_InitData
// Locates all the lumps
//  that will be used by all views
// Must be called after W_Init.
//
void R_InitData (void)
{
    R_InitTextures ();
    printf ("\nInitTextures");
    R_InitFlats ();
    printf ("\nInitFlats");
    R_InitSpriteLumps ();
    printf ("\nInitSprites");
    R_InitColormaps ();
    printf ("\nInitColormaps");
}



//
// R_FlatNumForName
// Retrieval, get a flat number for a flat name.
//
int R_FlatNumForName (const char* name)
{
    int		i;
    char namet[9];
    unsigned length = 0;

    while (length < 8 && name[length])
    {
        namet[length] = name[length];
        length++;
    }
    namet[length] = 0;
    i = W_CheckNumForName(namet);

    if (i == -1)
        I_Error("R_FlatNumForName: %s not found", namet);

    return i - firstflat;
}




//
// R_CheckTextureNumForName
// Check whether texture is available.
// Filter out NoTexture indicator.
//
int	R_CheckTextureNumForName (const char *name)
{
    int		i;

    // "NoTexture" marker.
    if (name[0] == '-')		
	return 0;
		
    for (i=0 ; i<numtextures ; i++)
	if (!strncasecmp (textures[i]->name, name, 8) )
	    return i;
		
    return -1;
}



//
// R_TextureNumForName
// Calls R_CheckTextureNumForName,
//  aborts with error message.
//
int	R_TextureNumForName (const char* name)
{
    int		i;
	
    i = R_CheckTextureNumForName (name);

    if (i==-1)
    {
	I_Error ("R_TextureNumForName: %.8s not found",
		 name);
    }
    return i;
}
