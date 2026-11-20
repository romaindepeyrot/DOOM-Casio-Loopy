#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "doomtype.h"
#include "i_system.h"
#include "w_wad.h"

extern const uint8_t loopy_iwad_start[];

enum
{
    ROM_BASE = 0x0e000000,
    ROM_CAPACITY = 4 * 1024 * 1024,
    WAD_HEADER_SIZE = 12,
    WAD_ENTRY_SIZE = 16,
    WAD_NAME_SIZE = 8
};

static uintptr_t wad_end;
static const uint8_t *wad_directory;
int numlumps;

static uint32_t ReadLittle32(const uint8_t *data)
{
    return (uint32_t)data[0]
        | (uint32_t)data[1] << 8
        | (uint32_t)data[2] << 16
        | (uint32_t)data[3] << 24;
}

static const uint8_t *LumpEntry(int lump)
{
    if (lump < 0 || lump >= numlumps)
        I_Error("Invalid WAD lump number %i", lump);

    return wad_directory + (size_t)lump * WAD_ENTRY_SIZE;
}

void W_InitMultipleFiles(char **filenames)
{
    const volatile uint32_t *header = (const volatile uint32_t *)ROM_BASE;
    uintptr_t wad_start = (uintptr_t)loopy_iwad_start;
    uint32_t last_word = header[1];
    size_t wad_size;
    uint32_t count;
    uint32_t offset;

    (void)filenames;
    if (last_word < wad_start || last_word >= ROM_BASE + ROM_CAPACITY
        || (last_word & 1))
        I_Error("No IWAD in cartridge");
    wad_end = (uintptr_t)last_word + 2;
    wad_size = wad_end - wad_start;
    if (wad_size < WAD_HEADER_SIZE || memcmp(loopy_iwad_start, "IWAD", 4))
        I_Error("No IWAD in cartridge");
    count = ReadLittle32(loopy_iwad_start + 4);
    offset = ReadLittle32(loopy_iwad_start + 8);
    if (!count || offset < WAD_HEADER_SIZE || offset > wad_size
        || count > (wad_size - offset) / WAD_ENTRY_SIZE)
        I_Error("Invalid cartridge WAD directory");

    wad_directory = loopy_iwad_start + offset;
    numlumps = (int)count;
    for (int lump = 0; lump < numlumps; ++lump)
    {
        const uint8_t *entry = LumpEntry(lump);
        uint32_t position = ReadLittle32(entry);
        uint32_t size = ReadLittle32(entry + 4);

        if (position > offset || size > offset - position
            || (size && ((position & 3) || position < WAD_HEADER_SIZE)))
            I_Error("Invalid cartridge WAD lump %i", lump);
    }
}

void W_Reload(void)
{
    /* The cartridge directory is immutable. */
}

const char *W_LumpName(int lump)
{
    return (const char *)LumpEntry(lump) + 8;
}

int W_CheckNumForName(char *name)
{
    char upper[WAD_NAME_SIZE] = { 0 };

    for (size_t i = 0; i < sizeof upper && name[i]; ++i)
    {
        char ch = name[i];
        upper[i] = (ch >= 'a' && ch <= 'z') ? ch - ('a' - 'A') : ch;
    }

    for (int lump = numlumps - 1; lump >= 0; --lump)
        if (!memcmp(W_LumpName(lump), upper, sizeof upper))
            return lump;

    return -1;
}

int W_GetNumForName(char *name)
{
    int lump = W_CheckNumForName(name);

    if (lump < 0)
        I_Error("WAD lump %.8s not found", name);

    return lump;
}

int W_LumpLength(int lump)
{
    return (int)ReadLittle32(LumpEntry(lump) + 4);
}

const void *W_LumpData(int lump)
{
    return loopy_iwad_start + ReadLittle32(LumpEntry(lump));
}

int W_ROMContains(const void *ptr)
{
    uintptr_t address = (uintptr_t)ptr;

    return address >= (uintptr_t)loopy_iwad_start
        && address < wad_end;
}

void W_ReadLump(int lump, void *dest)
{
    memcpy(dest, W_LumpData(lump), W_LumpLength(lump));
}

void *W_CacheLumpNum(int lump, int tag)
{
    /* Preserve the engine's WAD API for callers with legacy pointer types. */
    (void)tag;
    return (void *)W_LumpData(lump);
}

void *W_CacheLumpName(char *name, int tag)
{
    return W_CacheLumpNum(W_GetNumForName(name), tag);
}
