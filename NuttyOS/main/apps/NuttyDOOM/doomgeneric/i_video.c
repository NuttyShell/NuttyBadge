// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// $Log:$
//
// DESCRIPTION:
//	DOOM graphics stuff for X11, UNIX.
//
//-----------------------------------------------------------------------------

static const char
rcsid[] __attribute__((unused)) = "$Id: i_x.c,v 1.6 1997/02/03 22:45:10 b1 Exp $";

#include "config.h"
#include "v_video.h"
#include "m_argv.h"
#include "d_event.h"
#include "d_main.h"
#include "i_video.h"
#include "i_system.h"
#include "z_zone.h"

#include "tables.h"
#include "doomkeys.h"

#include "doomstat.h"
#include "d_player.h"
#include "doomdef.h"
#include "hu_stuff.h"
#include "m_menu.h"

#include "doomgeneric.h"

#include <stdbool.h>
#include <stdlib.h>

#include <fcntl.h>

#include <stdarg.h>
#include <string.h>

#include <sys/types.h>

//#define CMAP256

struct FB_BitField
{
	uint32_t offset;			/* beginning of bitfield	*/
	uint32_t length;			/* length of bitfield		*/
};

struct FB_ScreenInfo
{
	uint32_t xres;			/* visible resolution		*/
	uint32_t yres;
	uint32_t xres_virtual;		/* virtual resolution		*/
	uint32_t yres_virtual;

	uint32_t bits_per_pixel;		/* guess what			*/
	
							/* >1 = FOURCC			*/
	struct FB_BitField red;		/* bitfield in s_Fb mem if true color, */
	struct FB_BitField green;	/* else only length is significant */
	struct FB_BitField blue;
	struct FB_BitField transp;	/* transparency			*/
};

static struct FB_ScreenInfo s_Fb;
int fb_scaling = 1;
int usemouse = 0;


#ifdef CMAP256

boolean palette_changed;
struct color colors[256];

#else  // CMAP256

static struct color colors[256];


#endif  // CMAP256


void I_GetEvent(void);

// The screen buffer; this is modified to draw things to the screen

byte *I_VideoBuffer = NULL;

// If true, game is running as a screensaver

boolean screensaver_mode = false;

// Flag indicating whether the screen is currently visible:
// when the screen isnt visible, don't render the screen

boolean screenvisible;

// Mouse acceleration
//
// This emulates some of the behavior of DOS mouse drivers by increasing
// the speed when the mouse is moved fast.
//
// The mouse input values are input directly to the game, but when
// the values exceed the value of mouse_threshold, they are multiplied
// by mouse_acceleration to increase the speed.

float mouse_acceleration = 2.0;
int mouse_threshold = 10;

// Gamma correction level to use

int usegamma = 0;

typedef struct
{
	byte r;
	byte g;
	byte b;
} col_t;

// Palette converted to RGB565

static uint16_t rgb565_palette[256];

void cmap_to_rgb565(uint16_t * out, uint8_t * in, int in_pixels)
{
    int i, j;
    struct color c;
    uint16_t r, g, b;

    for (i = 0; i < in_pixels; i++)
    {
        c = colors[*in]; 
        r = ((uint16_t)(c.r >> 3)) << 11;
        g = ((uint16_t)(c.g >> 2)) << 5;
        b = ((uint16_t)(c.b >> 3)) << 0;
        *out = (r | g | b);

        in++;
        for (j = 0; j < fb_scaling; j++) {
            out++;
        }
    }
}

void cmap_to_fb(uint8_t *out, uint8_t *in, int in_pixels)
{
    int i, k;
    struct color c;
    uint32_t pix;

    for (i = 0; i < in_pixels; i++)
    {
        c = colors[*in];  // R:8 G:8 B:8

        if (s_Fb.bits_per_pixel == 16)
        {
            // RGB565 packing
            uint16_t p = ((c.r & 0xF8) << 8) |
                         ((c.g & 0xFC) << 3) |
                         (c.b >> 3);

#ifdef SYS_BIG_ENDIAN
            p = swapeLE16(p); // can't use SHORT() because this needs to stay unsigned
#endif
            for (k = 0; k < fb_scaling; k++) {
                *(uint16_t *)out = p;
                out += 2;
            }
        }
        else if (s_Fb.bits_per_pixel == 32)
        {
            // Assuming RGBA8888
            pix = (c.r << s_Fb.red.offset) |
                  (c.g << s_Fb.green.offset) |
                  (c.b << s_Fb.blue.offset);

#ifdef SYS_BIG_ENDIAN
            pix = swapLE32(pix);
#endif
            for (k = 0; k < fb_scaling; k++) {
                *(uint32_t *)out = pix;
                out += 4;
            }
        }
        else {
            // no clue how to convert this
            I_Error("No idea how to convert %d bpp pixels", s_Fb.bits_per_pixel);
        }

        in++;
    }
}

void I_InitGraphics (void)
{
    int i, gfxmodeparm;
    char *mode;

	memset(&s_Fb, 0, sizeof(struct FB_ScreenInfo));
	s_Fb.xres = DOOMGENERIC_RESX;
	s_Fb.yres = DOOMGENERIC_RESY;
	s_Fb.xres_virtual = s_Fb.xres;
	s_Fb.yres_virtual = s_Fb.yres;

#ifdef CMAP256

	s_Fb.bits_per_pixel = 8;

#else  // CMAP256

	gfxmodeparm = M_CheckParmWithArgs("-gfxmode", 1);

	if (gfxmodeparm) {
		mode = myargv[gfxmodeparm + 1];
	}
	else {
		// default to rgba8888 like the old behavior, for compatibility
		// maybe could warn here?
		mode = "rgba8888";
	}

	if (strcmp(mode, "rgba8888") == 0) {
		// default mode
		s_Fb.bits_per_pixel = 32;

		s_Fb.blue.length = 8;
		s_Fb.green.length = 8;
		s_Fb.red.length = 8;
		s_Fb.transp.length = 8;

		s_Fb.blue.offset = 0;
		s_Fb.green.offset = 8;
		s_Fb.red.offset = 16;
		s_Fb.transp.offset = 24;
	}

	else if (strcmp(mode, "rgb565") == 0) {
		s_Fb.bits_per_pixel = 16;

		s_Fb.blue.length = 5;
		s_Fb.green.length = 6;
		s_Fb.red.length = 5;
		s_Fb.transp.length = 0;

		s_Fb.blue.offset = 11;
		s_Fb.green.offset = 5;
		s_Fb.red.offset = 0;
		s_Fb.transp.offset = 16;
	}
	else
		I_Error("Unknown gfxmode value: %s\n", mode);


#endif  // CMAP256

    printf("I_InitGraphics: framebuffer: x_res: %lu, y_res: %lu, x_virtual: %lu, y_virtual: %lu, bpp: %lu\n",
            (unsigned long)s_Fb.xres, (unsigned long)s_Fb.yres, (unsigned long)s_Fb.xres_virtual, (unsigned long)s_Fb.yres_virtual, (unsigned long)s_Fb.bits_per_pixel);

    printf("I_InitGraphics: framebuffer: RGBA: %lu%lu%lu%lu, red_off: %lu, green_off: %lu, blue_off: %lu, transp_off: %lu\n",
            (unsigned long)s_Fb.red.length, (unsigned long)s_Fb.green.length, (unsigned long)s_Fb.blue.length, (unsigned long)s_Fb.transp.length, (unsigned long)s_Fb.red.offset, (unsigned long)s_Fb.green.offset, (unsigned long)s_Fb.blue.offset, (unsigned long)s_Fb.transp.offset);

    printf("I_InitGraphics: DOOM screen size: w x h: %d x %d\n", SCREENWIDTH, SCREENHEIGHT);


    i = M_CheckParmWithArgs("-scaling", 1);
    if (i > 0) {
        i = atoi(myargv[i + 1]);
        fb_scaling = i;
        printf("I_InitGraphics: Scaling factor: %d\n", fb_scaling);
    } else {
        fb_scaling = s_Fb.xres / SCREENWIDTH;
        if (s_Fb.yres / SCREENHEIGHT < fb_scaling)
            fb_scaling = s_Fb.yres / SCREENHEIGHT;
        printf("I_InitGraphics: Auto-scaling factor: %d\n", fb_scaling);
    }


    /* Allocate screen to draw to */
	I_VideoBuffer = (byte*)Z_Malloc (SCREENWIDTH * SCREENHEIGHT, PU_STATIC, NULL);  // For DOOM to draw on

	screenvisible = true;

    extern void I_InitInput(void);
    I_InitInput();
}

void I_ShutdownGraphics (void)
{
	Z_Free (I_VideoBuffer);
}

void I_StartFrame (void)
{

}

void I_StartTic (void)
{
	I_GetEvent();
}

void I_UpdateNoBlit (void)
{
}

#ifndef CHAR_WIDTH
#define CHAR_WIDTH 4
#endif // CHAR_WIDTH

#ifndef CHAR_HEIGHT
#define CHAR_HEIGHT 6
#endif // CHAR_HEIGHT

// ─── 4×6 bitmap font ────────────────────────────────────────────────────
// Each character is 6 bytes (rows). Lower 4 bits of each byte = the 4
// column pixels (bit3=leftmost … bit0=rightmost).
// Index: 0=' ', 1='0'…10='9', 11='A'…36='Z'.
static const uint8_t font4x6[41][CHAR_HEIGHT] = {
	{0,0,0,0,0,0},           //  0 space
	{6,9,9,9,9,6},           //  1 0  .##. #..# #..# #..# #..# .##.
	{2,6,2,2,2,7},           //  2 1  ..#. .##. ..#. ..#. ..#. .###
	{6,1,2,4,8,15},          //  3 2  .##. ...# ..#. .#.. #... ####
	{6,1,6,1,9,6},           //  4 3  .##. ...# .##. ...# #..# .##.
	{9,9,15,1,1,1},          //  5 4  #..# #..# #### ...# ...# ...#
	{15,8,14,1,9,6},         //  6 5  #### #... ###. ...# #..# .##.
	{6,8,14,9,9,6},          //  7 6  .##. #... ###. #..# #..# .##.
	{15,1,2,2,4,4},          //  8 7  #### ...# ..#. ..#. .#.. .#..
	{6,9,6,9,9,6},           //  9 8  .##. #..# .##. #..# #..# .##.
	{6,9,6,1,1,6},           // 10 9  .##. #..# .##. ...# ...# .##.
	{6,9,15,9,9,9},          // 11 A  .##. #..# #### #..# #..# #..#
	{14,9,14,9,9,14},        // 12 B  ###. #..# ###. #..# #..# ###.
	{6,9,8,8,9,6},           // 13 C  .##. #..# #... #... #..# .##.
	{14,9,9,9,9,14},         // 14 D  ###. #..# #..# #..# #..# ###.
	{15,8,14,8,8,15},        // 15 E  #### #... ###. #... #... ####
	{15,8,14,8,8,8},         // 16 F  #### #... ###. #... #... #...
	{6,9,8,11,9,7},          // 17 G  .##. #..# #... #.## #..# .###
	{9,9,15,9,9,9},          // 18 H  #..# #..# #### #..# #..# #..#
	{7,2,2,2,2,7},           // 19 I  .### ..#. ..#. ..#. ..#. .###
	{1,1,1,1,9,6},           // 20 J  ...# ...# ...# ...# #..# .##.
	{9,10,12,12,10,9},       // 21 K  #..# #.#. ##.. ##.. #.#. #..#
	{8,8,8,8,8,15},          // 22 L  #... #... #... #... #... ####
	{9,15,15,9,9,9},         // 23 M  #..# #### #### #..# #..# #..#
	{9,13,11,9,9,9},         // 24 N  #..# ##.# #.## #..# #..# #..#
	{6,9,9,9,9,6},           // 25 O  .##. #..# #..# #..# #..# .##.
	{14,9,14,8,8,8},         // 26 P  ###. #..# ###. #... #... #...
	{6,9,9,13,10,5},         // 27 Q  .##. #..# #..# #.## #.#. .#.#
	{14,9,14,10,9,9},        // 28 R  ###. #..# ###. #.#. #..# #..#
	{6,9,8,6,1,6},           // 29 S  .##. #..# #... .##. ...# .##.
	{7,2,2,2,2,2},           // 30 T  .### ..#. ..#. ..#. ..#. ..#.
	{9,9,9,9,9,6},           // 31 U  #..# #..# #..# #..# #..# .##.
	{9,9,9,9,6,6},           // 32 V  #..# #..# #..# #..# .##. .##.
	{9,9,9,15,15,9},         // 33 W  #..# #..# #..# #### #### #..#
	{9,9,6,6,9,9},           // 34 X  #..# #..# .##. .##. #..# #..#
	{9,9,6,2,2,2},           // 35 Y  #..# #..# .##. ..#. ..#. ..#.
	{15,1,2,4,8,15},         // 36 Z  #### ...# ..#. .#.. #... ####
	{8,4,2,2,4,8},           // 37 >  #... .#.. ..#. ..#. .#.. #...
	{0,0,0,0,0,1},           // 38 .  .... .... .... .... .... ...#
	{0,0,0,0,1,3},           // 39 ,  .... .... .... .... ...# ..##
	{0,0,2,0,2,0},           // 40 :  .... .... ..#. .... ..#. ....
};

// Map ASCII to font index; returns space for unknown chars.
static int charToIdx(char c)
{
	if (c >= '0' && c <= '9') return 1 + (c - '0');
	if (c >= 'A' && c <= 'Z') return 11 + (c - 'A');
	if (c >= 'a' && c <= 'z') return 11 + (c - 'a'); // lowercase = same as uppercase
	if (c == '.') return 38;
	if (c == ',') return 39;
	if (c == '!') return 0;
	if (c == '?') return 0;
	if (c == '\'') return 0;
	if (c == ':') return 40;
	if (c == '>') return 37;
	if (c == ' ') return 0;
	return 0; // space
}

// Draw a single glyph (6 rows, 4 bits per row) at (x,y).
static void drawGlyph(uint32_t* buf, int x, int y, const uint8_t glyph[CHAR_HEIGHT])
{
	for (int row = 0; row < CHAR_HEIGHT; row++)
	{
		uint8_t bits = glyph[row];
		int py = y + row;
		if (py < 0 || py >= DOOMGENERIC_RESY) continue;
		if (bits & 0b1000) { int px = x;     if (px >= 0 && px < DOOMGENERIC_RESX) buf[py * DOOMGENERIC_RESX + px] = 0x00FFFFFF; }
		if (bits & 0b0100) { int px = x + 1; if (px >= 0 && px < DOOMGENERIC_RESX) buf[py * DOOMGENERIC_RESX + px] = 0x00FFFFFF; }
		if (bits & 0b0010) { int px = x + 2; if (px >= 0 && px < DOOMGENERIC_RESX) buf[py * DOOMGENERIC_RESX + px] = 0x00FFFFFF; }
		if (bits & 0b0001) { int px = x + 3; if (px >= 0 && px < DOOMGENERIC_RESX) buf[py * DOOMGENERIC_RESX + px] = 0x00FFFFFF; }
	}
}

static void drawChar(uint32_t* buf, int x, int y, char c)
{
	drawGlyph(buf, x, y, font4x6[charToIdx(c)]);
}

// Draw a null-terminated string at (x,y).
static void drawStr(uint32_t* buf, int x, int y, const char* s)
{
	while (*s)
	{
		drawChar(buf, x, y, *s);
		x += 5; // 4 wide + 1 space
		s++;
	}
}

// Draw a decimal number at (x,y), right-aligned to width digits.
static void drawNum(uint32_t* buf, int x, int y, int num, int width)
{
	char tmp[8];
	int len = 0;
	if (num < 0) { num = 0; }
	do {
		tmp[len++] = '0' + (num % 10);
		num /= 10;
	} while (num > 0);
	while (len < width) tmp[len++] = '0';
	// Draw right-aligned: most-significant digit goes leftmost
	int dx = x - len * 5;
	for (int i = len - 1; i >= 0; i--)
	{
		drawChar(buf, dx, y, tmp[i]);
		dx += 5;
	}
}

// ─── 4×6 sidebar icons ─────────────────────────────────────────────────
// Drawn the same way as font chars (4px wide).
enum { ICON_HEART, ICON_SHIELD, ICON_BULLET, ICON_KEY };
static const uint8_t icons[4][6] = {
	{0,0,2,7,2,0},     // 0 Heart
	{0,9,15,15,15,6},  // 1 Shield
	{6,6,13,13,13,15}, // 2 Bullet
	{6,14,14,11,11,15}, // 3 Key
};

static void drawIcon(uint32_t* buf, int x, int y, int idx)
{
	drawGlyph(buf, x, y, icons[idx]);
}

// ─── 128x64 sidebar HUD ─────────────────────────────────────────────────
// Draws into the rightmost columns of DG_ScreenBuffer,
// AFTER the downscale+dither but BEFORE DG_DrawFrame().
static void DG_DrawHUD(void)
{
	uint32_t* buf = (uint32_t*)DG_ScreenBuffer;
	player_t* p = &players[consoleplayer];
	int sx = DOOMGENERIC_RESX - NUTTYDOOM_HUDRESX + 1; // sidebar content start x+1
	// Icon (4px) + gap (1px) + 3-digit number (15px) = 20px.

	// Row 0: Health  ♥ 100
	drawIcon(buf, sx, 0, ICON_HEART);
	drawNum(buf, sx + 20, 0, p->health, 3);

	// Row 1: Armor  🛡 050
	drawIcon(buf, sx, (CHAR_HEIGHT + 1), ICON_SHIELD);
	drawNum(buf, sx + 20, (CHAR_HEIGHT + 1)*1, p->armorpoints, 3);

	// Row 2: Ammo  ● 200
	int ammotype = weaponinfo[p->readyweapon].ammo;
	int ammo = (ammotype == am_noammo) ? 0 : p->ammo[ammotype];
	drawIcon(buf, sx, (CHAR_HEIGHT + 1)*2, ICON_BULLET);
	drawNum(buf, sx + 20, (CHAR_HEIGHT + 1)*2, ammo, 3);

	// Row 3: Weapon name  (e.g. "SHOTG" = 3 chars = 15px)
	static const char* wnames[] = {
		"FIST", "PSTL", "SHOT", "CHAI", "ROKT", "PLSM", "BFG9", "CSAW",  "SSHG"
	};
	int wpn = p->readyweapon;
	const char* wname = (wpn >= 0 && wpn < (int)(sizeof(wnames)/sizeof(wnames[0])))
		? wnames[wpn] : "????";
	drawStr(buf, sx, (CHAR_HEIGHT + 1)*3, wname);

	// Row 4: Keys  (key icon then "Y B R")
	drawIcon(buf, sx, (CHAR_HEIGHT + 1)*4, ICON_KEY);
	for (int i = 0; i < 3; i++)
	{
		if (p->cards[i] || p->cards[i + 3])
		{
			static const char klabels[] = { 'Y', 'B', 'R' };
			// I have no idea why this 5 cannot be CHAR_WIDTH+1, it's so weird
			drawChar(buf, sx + 5*(i+1), (CHAR_HEIGHT + 1)*4, klabels[i]);
		}
	}

	// Full-width message bar at the bottom (rows 56-63)
	// Clear the message bar area to black first
	for (int row = DOOMGENERIC_RESY - CHAR_HEIGHT; row < DOOMGENERIC_RESY; row++)
	{
		for (int col = 0; col < DOOMGENERIC_RESX; col++)
		{
			buf[row * DOOMGENERIC_RESX + col] = 0x00000000;
		}
	}
	// Draw the message if there is one, starting from left edge
	const char* msg = HU_CurrentMessage();
	if (msg)
	{
		drawStr(buf, 1, DOOMGENERIC_RESY - CHAR_HEIGHT, msg);
	}
}

// ─── 128x64 menu overlay ────────────────────────────────────────────────
// Map WAD lump names to readable display text.
static const char* menuLabel(const char* lump)
{
	// Skip "M_" prefix if present
	if (lump[0] == 'M' && lump[1] == '_') lump += 2;

	// ── Main menu ──
	if (!strcmp(lump, "NGAME"))  return "NEW GAME";
	if (!strcmp(lump, "OPTION")) return "OPTIONS";
	if (!strcmp(lump, "LOADG"))  return "LOAD GAME";
	if (!strcmp(lump, "SAVEG"))  return "SAVE GAME";
	if (!strcmp(lump, "RDTHIS")) return "READ THIS";
	if (!strcmp(lump, "QUITG"))  return "QUIT GAME";

	// ── Episode select ──
	if (!strcmp(lump, "EPI1"))   return "KNEE-DEEP";
	if (!strcmp(lump, "EPI2"))   return "THE SHORES";
	if (!strcmp(lump, "EPI3"))   return "INFERNO";
	if (!strcmp(lump, "EPI4"))   return "THY FLESH";

	// ── Difficulty ──
	if (!strcmp(lump, "JKILL"))  return "I'M TOO YOUNG";
	if (!strcmp(lump, "ROUGH"))  return "NOT TOO ROUGH";
	if (!strcmp(lump, "HURT"))   return "HURT ME PLENTY";
	if (!strcmp(lump, "ULTRA"))  return "ULTRA-VIOLENCE";
	if (!strcmp(lump, "NMARE"))  return "NIGHTMARE";

	// ── Options ──
	if (!strcmp(lump, "ENDGAM")) return "END GAME";
	if (!strcmp(lump, "MESSG"))  return "MESSAGES";
	if (!strcmp(lump, "DETAIL")) return "DETAIL";
	if (!strcmp(lump, "SCRNSZ")) return "SCREEN SIZE";
	if (!strcmp(lump, "MSENS"))  return "MOUSE SENSE";
	if (!strcmp(lump, "SVOL"))   return "SOUND VOLUME";

	// ── Sound submenu ──
	if (!strcmp(lump, "SFXVOL")) return "SFX VOLUME";
	if (!strcmp(lump, "MUSVOL")) return "MUSIC VOLUME";

	return lump; // fallback: show raw name
}

// Externs for submenu values
extern int showMessages;
extern int detailLevel;
extern int mouseSensitivity;
extern int screenSize;
extern int sfxVolume;
extern int musicVolume;
extern int messageToPrint;
extern char* messageString;
extern int messageNeedsInput;

// Draw a simple horizontal bar (value out of max) at (x,y).
static void drawBar(uint32_t* buf, int x, int y, int val, int max)
{
	int bw = 20; // bar width in pixels
	int filled = (val * bw + max / 2) / max;
	for (int row = 2; row < 5; row++)
	{
		int py = y + row;
		if (py < 0 || py >= DOOMGENERIC_RESY) continue;
		for (int col = 0; col < bw; col++)
		{
			int px = x + col;
			if (px >= DOOMGENERIC_RESX) break;
			if (col < filled)
				buf[py * DOOMGENERIC_RESX + px] = 0x00FFFFFF;
			else
				buf[py * DOOMGENERIC_RESX + px] = 0x00000000;
		}
	}
}

// Draw current value for status==2 items (toggles/sliders).
static void drawMenuValue(uint32_t* buf, int x, int y, int i)
{
	const char* name = currentMenu->menuitems[i].name;
	if (!strcmp(name, "M_MESSG"))
		drawStr(buf, x, y, showMessages ? ":ON" : ":OFF");
	else if (!strcmp(name, "M_DETAIL"))
		drawStr(buf, x, y, detailLevel ? ":LOW" : ":HIGH");
	else if (!strcmp(name, "M_SCRNSZ"))
		drawBar(buf, x, y, screenSize, 8);
	else if (!strcmp(name, "M_MSENS"))
		drawBar(buf, x, y, mouseSensitivity, 9);
	else if (!strcmp(name, "M_SFXVOL"))
		drawBar(buf, x, y, sfxVolume, 15);
	else if (!strcmp(name, "M_MUSVOL"))
		drawBar(buf, x, y, musicVolume, 15);
}

// Replaces the original DOOM menu rendering with our own 4x6 font.
static void DG_DrawMenu(void)
{
	uint32_t* buf = (uint32_t*)DG_ScreenBuffer;
	int w = DOOMGENERIC_RESX;
	int h = DOOMGENERIC_RESY;

	// Clear full screen to black
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++)
			buf[y * w + x] = 0x00000000;

	// Show pending confirmation messages, splitting on \n
	if (messageToPrint && messageString)
	{
		const char* p = messageString;
		int ly = 2;
		while (*p) {
			const char* nl = p;
			while (*nl && *nl != '\n') nl++;
			if (nl > p) {
				char tmp[64];
				int len = (nl - p < 63) ? (nl - p) : 63;
				memcpy(tmp, p, len);
				tmp[len] = '\0';
				drawStr(buf, 1, ly, tmp);
				ly += 7;
			}
			p = (*nl == '\n') ? nl + 1 : nl;
		}
		if (messageNeedsInput)
			drawStr(buf, 1, ly, "Y/N?");
		return;
	}

	if (!currentMenu) return;

	// Title for known menus based on their item count/location
	const char* title = NULL;
	if (currentMenu->x == 97)  title = "DOOM";
	if (currentMenu->x == 48 && currentMenu->y == 63) title = "NEW GAME";
	if (currentMenu->x == 60)  title = "OPTIONS";
	if (currentMenu->x == 80 && currentMenu->numitems == 6) title = "LOAD GAME";
	if (currentMenu->x == 80 && currentMenu->numitems == 4) title = "SOUND";
	if (title) drawStr(buf, 1, 0, title);

	// Menu items
	int y = title ? 8 : 2;
	for (int i = 0; i < currentMenu->numitems; i++)
	{
		const char* name = currentMenu->menuitems[i].name;
		if (name[0])
		{
			if (i == itemOn)
				drawStr(buf, 1, y, ">");
			drawStr(buf, 8, y, menuLabel(name));

			// Draw current value for toggles/sliders
			drawMenuValue(buf, 100, y, i);

			y += 7;
		}
	}
}

// ─── 128x64 intermission overlay ────────────────────────────────────────
static void DG_DrawIntermission(void)
{
	uint32_t* buf = (uint32_t*)DG_ScreenBuffer;
	int w = DOOMGENERIC_RESX;
	int h = DOOMGENERIC_RESY;

	// Clear to black
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++)
			buf[y * w + x] = 0x00000000;

	// Level names
	extern char* mapnames[];
	extern char* mapnames_commercial[];
	int ep = gameepisode - 1;
	int lev = gamemap - 1;
	const char* lname = (gamemode == commercial)
		? mapnames_commercial[lev]
		: mapnames[ep * 9 + lev];

	drawStr(buf, 1, 0, "LEVEL");
	drawStr(buf, 1, 7, lname);

	// Stats from wminfo
	wbplayerstruct_t* p = &wminfo.plyr[consoleplayer];
	drawStr(buf, 1, 21, "KILLS");
	drawNum(buf, 55, 21, p->skills, 3);
	drawStr(buf, 70, 21, "ITEMS");
	drawNum(buf, 125, 21, p->sitems, 3);
	drawStr(buf, 1, 28, "SECRET");
	drawNum(buf, 55, 28, p->ssecret, 2);
	drawStr(buf, 70, 28, "TIME");
	int tics = p->stime, sec = tics / 35, min = sec / 60; sec %= 60;
	char tb[8];
	tb[0]='0'+(min/10); tb[1]='0'+(min%10); tb[2]=':';
	tb[3]='0'+(sec/10); tb[4]='0'+(sec%10); tb[5]=0;
	drawStr(buf, 100, 28, tb);
	if (wminfo.partime) {
		int pt = wminfo.partime; sec = pt/35; min = sec/60; sec %= 60;
		tb[0]='0'+(min/10); tb[1]='0'+(min%10); tb[2]=':';
		tb[3]='0'+(sec/10); tb[4]='0'+(sec%10); tb[5]=0;
		drawStr(buf, 1, 35, "PAR");
		drawStr(buf, 30, 35, tb);
	}
}

//
// I_FinishUpdate
//

void I_FinishUpdate (void)
{
    int y;
    int x_offset, y_offset, x_offset_end;
    unsigned char *line_in, *line_out;

    /* Offsets in case FB is bigger than DOOM */
    /* 600 = s_Fb heigt, 200 screenheight */
    /* 600 = s_Fb heigt, 200 screenheight */
    /* 2048 =s_Fb width, 320 screenwidth */
    y_offset     = (((s_Fb.yres - (SCREENHEIGHT * fb_scaling)) * s_Fb.bits_per_pixel/8)) / 2;
    x_offset     = (((s_Fb.xres - (SCREENWIDTH  * fb_scaling)) * s_Fb.bits_per_pixel/8)) / 2; // XXX: siglent FB hack: /4 instead of /2, since it seems to handle the resolution in a funny way
    //x_offset     = 0;
    x_offset_end = ((s_Fb.xres - (SCREENWIDTH  * fb_scaling)) * s_Fb.bits_per_pixel/8) - x_offset;

	// Downscale I_VideoBuffer (320x200) into the left 102 columns of
	// DG_ScreenBuffer (128x64) with area-averaging, then Floyd-Steinberg
	// dither to 1-bit black and white. The right 26 columns are reserved
	// for the sidebar HUD.
	if (s_Fb.bits_per_pixel == 32)
	{
		int src_w = SCREENWIDTH;
		int src_h = SCREENHEIGHT;
		int game_w = (gamestate == GS_LEVEL) ? DOOMGENERIC_RESX - NUTTYDOOM_HUDRESX : DOOMGENERIC_RESX;   // sidebar only in-game
		int dst_h = DOOMGENERIC_RESY;
		uint32_t* pixels = (uint32_t*)DG_ScreenBuffer;
		byte* src = (byte*)I_VideoBuffer;

		// Temporary buffer for grayscale values with error diffusion
		int* gray = (int*)malloc(game_w * dst_h * sizeof(int));
		if (gray)
		{
			// Area-averaged downscale: only fill left 102 columns
			for (int dy = 0; dy < dst_h; dy++)
			{
				int src_y0 = (dy * src_h) / dst_h;
				int src_y1 = ((dy + 1) * src_h) / dst_h;

				for (int dx = 0; dx < game_w; dx++)
				{
					int src_x0 = (dx * src_w) / game_w;
					int src_x1 = ((dx + 1) * src_w) / game_w;

					int total = 0;
					int count = 0;

					for (int sy = src_y0; sy < src_y1; sy++)
					{
						for (int sx = src_x0; sx < src_x1; sx++)
						{
							struct color c = colors[src[sy * src_w + sx]];
							total += (77 * c.r + 150 * c.g + 29 * c.b) >> 8;
							count++;
						}
					}

					gray[dy * game_w + dx] = total / count;
				}
			}

			// Floyd-Steinberg dithering on game region
			for (int dy = 0; dy < dst_h; dy++)
			{
				for (int dx = 0; dx < game_w; dx++)
				{
					int idx = dy * game_w + dx;
					int oldPixel = gray[idx];
					int newPixel = oldPixel < 128 ? 0 : 255;
					pixels[dy * DOOMGENERIC_RESX + dx] =
						newPixel ? 0x00FFFFFF : 0x00000000;

					int error = oldPixel - newPixel;

					if (dx + 1 < game_w)
						gray[idx + 1]              += (error * 7) / 16;
					if (dy + 1 < dst_h)
					{
						if (dx > 0)
							gray[(dy + 1) * game_w + dx - 1] += (error * 3) / 16;
						gray[(dy + 1) * game_w + dx]     += (error * 5) / 16;
						if (dx + 1 < game_w)
							gray[(dy + 1) * game_w + dx + 1] += (error * 1) / 16;
					}
				}
			}

			free(gray);
		}

		// Clear rightmost HUD columns (sidebar area) to black (in-game only)
		if (gamestate == GS_LEVEL)
		{
			for (int dy = 0; dy < dst_h; dy++)
			{
				for (int dx = game_w; dx < DOOMGENERIC_RESX; dx++)
				{
					pixels[dy * DOOMGENERIC_RESX + dx] = 0x00000000;
				}
			}
		}
	}

	// Draw 128x64 overlay: menu (highest priority), then intermission, then HUD
	if (menuactive)
		DG_DrawMenu();
	else if (gamestate == GS_INTERMISSION)
		DG_DrawIntermission();
	else if (gamestate == GS_LEVEL)
		DG_DrawHUD();

	DG_DrawFrame();
}

//
// I_ReadScreen
//
void I_ReadScreen (byte* scr)
{
    memcpy (scr, I_VideoBuffer, SCREENWIDTH * SCREENHEIGHT);
}

//
// I_SetPalette
//
#define GFX_RGB565(r, g, b)			((((r & 0xF8) >> 3) << 11) | (((g & 0xFC) >> 2) << 5) | ((b & 0xF8) >> 3))
#define GFX_RGB565_R(color)			((0xF800 & color) >> 11)
#define GFX_RGB565_G(color)			((0x07E0 & color) >> 5)
#define GFX_RGB565_B(color)			(0x001F & color)

void I_SetPalette (byte* palette)
{
	int i;
	//col_t* c;

	//for (i = 0; i < 256; i++)
	//{
	//	c = (col_t*)palette;

	//	rgb565_palette[i] = GFX_RGB565(gammatable[usegamma][c->r],
	//								   gammatable[usegamma][c->g],
	//								   gammatable[usegamma][c->b]);

	//	palette += 3;
	//}
    

    /* performance boost:
     * map to the right pixel format over here! */

    for (i=0; i<256; ++i ) {
        colors[i].a = 0;
        colors[i].r = gammatable[usegamma][*palette++];
        colors[i].g = gammatable[usegamma][*palette++];
        colors[i].b = gammatable[usegamma][*palette++];
    }

#ifdef CMAP256

    palette_changed = true;

#endif  // CMAP256
}

// Given an RGB value, find the closest matching palette index.

int I_GetPaletteIndex (int r, int g, int b)
{
    int best, best_diff, diff;
    int i;
    col_t color;

    printf("I_GetPaletteIndex\n");

    best = 0;
    best_diff = INT_MAX;

    for (i = 0; i < 256; ++i)
    {
    	color.r = GFX_RGB565_R(rgb565_palette[i]);
    	color.g = GFX_RGB565_G(rgb565_palette[i]);
    	color.b = GFX_RGB565_B(rgb565_palette[i]);

        diff = (r - color.r) * (r - color.r)
             + (g - color.g) * (g - color.g)
             + (b - color.b) * (b - color.b);

        if (diff < best_diff)
        {
            best = i;
            best_diff = diff;
        }

        if (diff == 0)
        {
            break;
        }
    }

    return best;
}

void I_BeginRead (void)
{
}

void I_EndRead (void)
{
}

void I_SetWindowTitle (char *title)
{
	DG_SetWindowTitle(title);
}

void I_GraphicsCheckCommandLine (void)
{
}

void I_SetGrabMouseCallback (grabmouse_callback_t func)
{
}

void I_EnableLoadingDisk(void)
{
}

void I_BindVideoVariables (void)
{
}

void I_DisplayFPSDots (boolean dots_on)
{
}

void I_CheckIsScreensaver (void)
{
}
