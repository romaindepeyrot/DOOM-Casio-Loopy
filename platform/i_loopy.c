/* Casio Loopy platform interface for the DOOM engine. */

#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#include "d_main.h"
#include "doomdef.h"
#include "doomstat.h"
#include "i_system.h"
#include "i_video.h"
#include "loopy.h"
#include "m_argv.h"
#include "serial.h"
#include "v_video.h"

#define LOOPY_VDP (*(volatile struct vdp *)&VDP)

enum {
    bitmap_width = 256,
    bitmap_page_rows = 256,
    video_height = 224,
    vcount_blank = 0x100,
    pad_queue_size = 32,
    itu1_start = 1 << 1,
    timer_match_a = 1 << 0,
    timer_div8_clear_on_match_a = 0x23,
    serial_tx_empty = 0x80
};

_Static_assert(SCREENWIDTH == 320 || SCREENWIDTH == bitmap_width,
               "Unsupported DOOM framebuffer width");
_Static_assert(SCREENHEIGHT <= video_height, "The framebuffer must fit the VDP");

extern unsigned char __bss_link_end[] __asm__("__bss_link_end");
extern unsigned char __zone_end[] __asm__("__zone_end");
extern int levelstarttic;
extern void D_PostEvent(event_t *event);

static volatile unsigned int doom_tics;
static volatile uint16_t sampled_buttons;
static volatile uint16_t pad_queue[pad_queue_size];
static volatile unsigned int pad_read;
static volatile unsigned int pad_write;
static ticcmd_t empty_ticcmd;
static uint16_t palette_rgb555[256];
static bool palette_dirty;
static bool serial_ready;
static unsigned int displayed_page;
static uint16_t last_buttons;
static uint16_t blocked_actions;
static bool last_menu_active;
static int last_level_tic;
static int weapon_key;

static const uint16_t gamepad_buttons[] = {
    GAMEPAD_BTN_START,
    GAMEPAD_BTN_UP, GAMEPAD_BTN_DOWN, GAMEPAD_BTN_LEFT, GAMEPAD_BTN_RIGHT,
    GAMEPAD_BTN_A, GAMEPAD_BTN_B, GAMEPAD_BTN_C,
    GAMEPAD_BTN_LTRIG, GAMEPAD_BTN_RTRIG
};

static int active_keys[sizeof(gamepad_buttons) / sizeof(gamepad_buttons[0])];

void loopy_log_putc(char character)
{
    if (!serial_ready)
        return;

    if (character == '\n')
        serial_write('\r');
    serial_write(character);
}

static uint16_t read_gamepad(void)
{
    const uint16_t raw0 = LOOPY_VDP.IO_GAMEPAD[0];
    const uint16_t raw1 = LOOPY_VDP.IO_GAMEPAD[1];

    return ((raw1 & 0x000f) << 8) | ((raw0 & 0x0f00) >> 4) | (raw0 & 0x000f);
}

__attribute__((interrupt_handler))
void doom_timer_interrupt(void)
{
    /* Reading the flag before writing a zero is required by the SH7021. */
    ITU_TSR1 &= (uint8_t)~timer_match_a;
    ++doom_tics;

    /* Preserve taps that occur while the main loop is rendering a frame. */
    uint16_t buttons = read_gamepad();
    if (buttons != sampled_buttons) {
        unsigned int write = pad_write;
        unsigned int next = (write + 1) % pad_queue_size;
        if (next != pad_read) {
            pad_queue[write] = buttons;
            pad_write = next;
        }
        sampled_buttons = buttons;
    }
}

void I_Init(void)
{
    ITU_TSTR &= (uint8_t)~itu1_start;
    ITU_TCR1 = timer_div8_clear_on_match_a;
    // The BIOS uses an 8-bit write to the high byte of the timer counter.
    // After that write, the first compare is at most 255 timer counts early.
    *(volatile uint8_t *)&ITU_TCNT1 = 0;
    ITU_GRA1 = (F_CPU / 8 + TICRATE / 2) / TICRATE;
    ITU_TSR1 &= (uint8_t)~timer_match_a;
    ITU_TIER1 = timer_match_a;
    /* ITU1 priority occupies bits 0-3 of IPRC. */
    INTC_IPRC |= 0x000f;
    sys_setInterruptMask(14);
    ITU_TSTR |= itu1_start;
}

int I_GetTime(void)
{
    return (int)doom_tics;
}

byte *I_ZoneBase(int *size)
{
    uintptr_t start = ((uintptr_t)__bss_link_end + 3) & ~(uintptr_t)3;
    uintptr_t end = (uintptr_t)__zone_end & ~(uintptr_t)3;

    if (start >= end)
        I_Error("No cartridge work RAM remains for the DOOM zone");

    *size = (int)(end - start);
    return (byte *)start;
}

ticcmd_t *I_BaseTiccmd(void)
{
    return &empty_ticcmd;
}

void I_Tactile(int on, int off, int total)
{
    (void)on;
    (void)off;
    (void)total;
}

static void post_key(evtype_t type, int key)
{
    event_t event = {type, key, 0, 0};
    D_PostEvent(&event);
}

static int next_weapon_key(void)
{
    static const weapontype_t weapon_order[] = {
        wp_fist, wp_pistol, wp_supershotgun, wp_shotgun, wp_chaingun,
        wp_missile, wp_plasma, wp_bfg, wp_chainsaw
    };
    const player_t *player = &players[consoleplayer];
    unsigned int start = 0;

    if (player->pendingweapon != wp_nochange)
        return 0;

    while (start < NUMWEAPONS && weapon_order[start] != player->readyweapon)
        ++start;

    for (unsigned int step = 1; step < NUMWEAPONS; ++step) {
        weapontype_t weapon = weapon_order[(start + step) % NUMWEAPONS];

        if (!player->weaponowned[weapon])
            continue;
        /* Vanilla Doom selects both kinds of shotgun with key 3. */
        if (weapon == wp_supershotgun)
            return '1' + wp_shotgun;
        if (weapon == wp_shotgun && player->weaponowned[wp_supershotgun]
            && player->readyweapon != wp_supershotgun)
            continue;
        if (weapon == wp_fist && player->weaponowned[wp_chainsaw]
            && (player->readyweapon != wp_chainsaw || !player->powers[pw_strength]))
            continue;
        return '1' + weapon;
    }

    return 0;
}

void I_StartTic(void)
{
    /* The interrupt produces controller changes while game ticks consume them. */
    unsigned int read = pad_read;
    uint16_t buttons = sampled_buttons;
    if (read != pad_write) {
        buttons = pad_queue[read];
        pad_read = (read + 1) % pad_queue_size;
    }
    const uint16_t map_chord = GAMEPAD_BTN_LTRIG | GAMEPAD_BTN_RTRIG;
    const bool map_pressed = (buttons & map_chord) == map_chord;
    const bool start_pressed = (buttons & ~last_buttons & GAMEPAD_BTN_START) != 0;
    const bool menu = menuactive;
    const bool new_level = last_level_tic != levelstarttic;

    blocked_actions &= buttons;
    if (last_menu_active != menu || new_level || start_pressed)
        blocked_actions |= buttons & (GAMEPAD_BTN_A | GAMEPAD_BTN_B);

    if (new_level) {
        /* G_DoLoadLevel resets the engine's held key state. */
        memset(active_keys, 0, sizeof(active_keys));
        last_level_tic = levelstarttic;
    }

    if (!menu && !start_pressed && gamestate == GS_LEVEL && map_pressed
        && (last_buttons & map_chord) != map_chord) {
        post_key(ev_keydown, KEY_TAB);
        post_key(ev_keyup, KEY_TAB);
    }

    if (weapon_key) {
        post_key(ev_keyup, weapon_key);
        weapon_key = 0;
    }
    if (!menu && !start_pressed && gamestate == GS_LEVEL
        && (buttons & ~last_buttons & GAMEPAD_BTN_D)) {
        weapon_key = next_weapon_key();
        if (weapon_key)
            post_key(ev_keydown, weapon_key);
    }

    const int mapped_keys[] = {
        KEY_ESCAPE,
        KEY_UPARROW, KEY_DOWNARROW, KEY_LEFTARROW, KEY_RIGHTARROW,
        menu ? KEY_ENTER : KEY_RCTRL,
        menu ? KEY_BACKSPACE : ' ',
        menu ? 0 : KEY_RSHIFT,
        menu || map_pressed ? 0 : ',',
        menu || map_pressed ? 0 : '.'
    };

    for (unsigned int i = 0; i < sizeof(active_keys) / sizeof(active_keys[0]); ++i) {
        uint16_t button = gamepad_buttons[i];
        int key = 0;

        if (buttons & ~blocked_actions & button)
            key = mapped_keys[i];

        if (key != active_keys[i]) {
            if (active_keys[i])
                post_key(ev_keyup, active_keys[i]);
            if (key)
                post_key(ev_keydown, key);
            active_keys[i] = key;
        }
    }

    last_menu_active = menu;
    last_buttons = buttons;
}

void I_StartFrame(void)
{
}

static void wait_for_vblank(void)
{
    /* VCOUNT is signed; it becomes negative after the last active line. */
    while (LOOPY_VDP.VCOUNT & vcount_blank) {}
    while (!(LOOPY_VDP.VCOUNT & vcount_blank)) {}
}

void I_InitGraphics(void)
{
    LOOPY_VDP.SCREENPRIO = 0;
    LOOPY_VDP.BACKDROP_A = 0;
    LOOPY_VDP.BACKDROP_B = 0;
    LOOPY_VDP.BLEND = BLEND_MATH;
    LOOPY_VDP.BM_SCROLLX[0] = 0;
    LOOPY_VDP.BM_SCROLLY[0] = 0;
    LOOPY_VDP.BM_SCREENX[0] = 0;
    LOOPY_VDP.BM_SCREENY[0] = (video_height - SCREENHEIGHT) / 2;
    LOOPY_VDP.BM_WIDTH[0] = bitmap_width - 1;
    LOOPY_VDP.BM_HEIGHT[0] = SCREENHEIGHT - 1;
    LOOPY_VDP.BM_CTRL = BM_MODE_8BPP_VSPLIT;
    LOOPY_VDP.BM_SUBPAL = 0;
    LOOPY_VDP.BM_COL_LATCH[0] = 0;
    LOOPY_VDP.LAYER_CTRL = LAYER_SCREEN(LAYER_SCREEN_A, LAYER_SCREEN_A,
                                       LAYER_SCREEN_A, LAYER_SCREEN_A) | LAYER_ENABLE_BM0;
    LOOPY_VDP.SCREENPRIO = SCREEN_A_ENABLE;
    displayed_page = 0;
}

void I_SetPalette(byte *palette)
{
    const byte *gamma = gammatable[usegamma];

    for (int i = 0; i < 256; ++i) {
        palette_rgb555[i] = RGB555(gamma[palette[0]] >> 3, gamma[palette[1]] >> 3,
                                   gamma[palette[2]] >> 3);
        palette += 3;
    }

    palette_dirty = true;
}

void I_FinishUpdate(void)
{
    typedef uint16_t aligned_word __attribute__((may_alias));
    const unsigned int page = displayed_page ^ 1;
    volatile uint16_t *destination = LOOPY_VDP.BITMAP_VRAM
                                  + page * bitmap_page_rows * bitmap_width / 2;
    /* V_Init aligns the framebuffer to four bytes. The SH-1 is big-endian. */
    const aligned_word *source = (const aligned_word *)screens[0];

#if SCREENWIDTH == 320
    /* Keep pixels 0-1, 3-6, and 8-9 of each ten-pixel group. */
    for (unsigned int i = 0; i < SCREENWIDTH * SCREENHEIGHT / 10; ++i) {
        uint16_t center = source[2];
        *destination++ = source[0];
        *destination++ = (source[1] << 8) | (center >> 8);
        *destination++ = (center << 8) | (source[3] >> 8);
        *destination++ = source[4];
        source += 5;
    }
#else
    for (unsigned int i = 0; i < SCREENWIDTH * SCREENHEIGHT / 2; ++i)
        *destination++ = *source++;
#endif

    wait_for_vblank();
    if (palette_dirty) {
        for (int i = 1; i < 256; ++i)
            LOOPY_VDP.PALETTE[i] = palette_rgb555[i];
        /* Bitmap index zero is transparent and shows the backdrop. */
        LOOPY_VDP.BACKDROP_A = palette_rgb555[0];
        palette_dirty = false;
    }
    LOOPY_VDP.BM_SCROLLY[0] = page * bitmap_page_rows;
    displayed_page = page;
}

void I_ShutdownGraphics(void)
{
    LOOPY_VDP.LAYER_CTRL = 0;
    LOOPY_VDP.BACKDROP_A = 0;
    LOOPY_VDP.BLEND = BLEND_MATH;
    LOOPY_VDP.SCREENPRIO = SCREEN_A_ENABLE;
}

void I_ReadScreen(byte *destination)
{
    memcpy(destination, screens[0], SCREENWIDTH * SCREENHEIGHT);
}

void I_UpdateNoBlit(void)
{
}

void I_BeginRead(void)
{
}

void I_EndRead(void)
{
}

void I_WaitVBL(int count)
{
    /* Doom calls this with a delay measured in the original 70 Hz ticks. */
    int frames = (count * 60 + 69) / 70;

    while (frames-- > 0)
        wait_for_vblank();
}

static _Noreturn void wait_for_reset(void)
{
    for (;;)
        wait_for_vblank();
}

void I_Quit(void)
{
    I_ShutdownGraphics();
    wait_for_reset();
}

void I_Error(char *format, ...)
{
    va_list args;

    fprintf(stderr, "DOOM error: ");
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");

    I_ShutdownGraphics();
    LOOPY_VDP.BACKDROP_A = RGB555(24, 0, 0);
    wait_for_reset();
}

int main(void)
{
    static char *argv[] = {"doom-loopy", NULL};

    serial_begin(38400);
    // Older emulators omit the status flags. Skip diagnostics when polling
    // an idle transmitter would otherwise block startup.
    serial_ready = (SCI_SSR0 & serial_tx_empty) != 0;
    bios_vdpMode(CONTROL_MODE_GAMEPAD, VIDEO_HEIGHT_224P);
    myargc = 1;
    myargv = argv;
    D_DoomMain();
    return 0;
}
