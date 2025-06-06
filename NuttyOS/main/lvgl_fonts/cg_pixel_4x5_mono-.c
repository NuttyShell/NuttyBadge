/*******************************************************************************
 * Size: 5 px
 * Bpp: 1
 * Opts: --bpp 1 --size 5 --no-compress --font CG pixel 4x5 mono.ttf --range 32-126 --format lvgl -o cg_pixel_4x5_mono.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef CG_PIXEL_4X5_MONO
#define CG_PIXEL_4X5_MONO 1
#endif

#if CG_PIXEL_4X5_MONO

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xe8,

    /* U+0022 "\"" */
    0xb4,

    /* U+0023 "#" */
    0xaf, 0xaf, 0xa0,

    /* U+0024 "$" */
    0x7a, 0x65, 0xe0,

    /* U+0025 "%" */
    0xc1, 0x68, 0x30,

    /* U+0026 "&" */
    0x4a, 0x4a, 0x50,

    /* U+0027 "'" */
    0xc0,

    /* U+0028 "(" */
    0x6a, 0x40,

    /* U+0029 ")" */
    0x95, 0x80,

    /* U+002A "*" */
    0x96, 0xf6, 0x90,

    /* U+002B "+" */
    0x5d, 0x0,

    /* U+002C "," */
    0x60,

    /* U+002D "-" */
    0xe0,

    /* U+002E "." */
    0x80,

    /* U+002F "/" */
    0x25, 0x48,

    /* U+0030 "0" */
    0x6b, 0xd9, 0x60,

    /* U+0031 "1" */
    0x75, 0x40,

    /* U+0032 "2" */
    0x69, 0x24, 0xf0,

    /* U+0033 "3" */
    0xe1, 0x61, 0xe0,

    /* U+0034 "4" */
    0x99, 0xf1, 0x10,

    /* U+0035 "5" */
    0xf8, 0xe1, 0xe0,

    /* U+0036 "6" */
    0x68, 0xe9, 0x60,

    /* U+0037 "7" */
    0xf1, 0x22, 0x20,

    /* U+0038 "8" */
    0x69, 0x69, 0x60,

    /* U+0039 "9" */
    0x69, 0x71, 0x60,

    /* U+003A ":" */
    0xa0,

    /* U+003B ";" */
    0xb0,

    /* U+003C "<" */
    0x2a, 0x22,

    /* U+003D "=" */
    0xe3, 0x80,

    /* U+003E ">" */
    0x88, 0xa8,

    /* U+003F "?" */
    0xc5, 0x4,

    /* U+0040 "@" */
    0x69, 0xb8, 0x70,

    /* U+0041 "A" */
    0x69, 0xf9, 0x90,

    /* U+0042 "B" */
    0xe9, 0xe9, 0xe0,

    /* U+0043 "C" */
    0x69, 0x89, 0x60,

    /* U+0044 "D" */
    0xe9, 0x99, 0xe0,

    /* U+0045 "E" */
    0xf8, 0xe8, 0xf0,

    /* U+0046 "F" */
    0xf8, 0xe8, 0x80,

    /* U+0047 "G" */
    0x68, 0xb9, 0x60,

    /* U+0048 "H" */
    0x99, 0xf9, 0x90,

    /* U+0049 "I" */
    0xf8,

    /* U+004A "J" */
    0x11, 0x19, 0x60,

    /* U+004B "K" */
    0x9a, 0xca, 0x90,

    /* U+004C "L" */
    0x88, 0x88, 0xf0,

    /* U+004D "M" */
    0x9f, 0x99, 0x90,

    /* U+004E "N" */
    0x9d, 0xb9, 0x90,

    /* U+004F "O" */
    0x69, 0x99, 0x60,

    /* U+0050 "P" */
    0xe9, 0xe8, 0x80,

    /* U+0051 "Q" */
    0x69, 0x9b, 0x70,

    /* U+0052 "R" */
    0xe9, 0xea, 0x90,

    /* U+0053 "S" */
    0x78, 0x61, 0xe0,

    /* U+0054 "T" */
    0xe9, 0x24,

    /* U+0055 "U" */
    0x99, 0x99, 0x60,

    /* U+0056 "V" */
    0xb6, 0xd4,

    /* U+0057 "W" */
    0x99, 0x9f, 0x90,

    /* U+0058 "X" */
    0x99, 0x69, 0x90,

    /* U+0059 "Y" */
    0xb5, 0x24,

    /* U+005A "Z" */
    0xf1, 0x68, 0xf0,

    /* U+005B "[" */
    0xea, 0xc0,

    /* U+005C "\\" */
    0x91, 0x12,

    /* U+005D "]" */
    0xd5, 0xc0,

    /* U+005E "^" */
    0x54,

    /* U+005F "_" */
    0xf0,

    /* U+0060 "`" */
    0x90,

    /* U+0061 "a" */
    0x69, 0xf9, 0x90,

    /* U+0062 "b" */
    0xe9, 0xe9, 0xe0,

    /* U+0063 "c" */
    0x69, 0x89, 0x60,

    /* U+0064 "d" */
    0xe9, 0x99, 0xe0,

    /* U+0065 "e" */
    0xf8, 0xe8, 0xf0,

    /* U+0066 "f" */
    0xf8, 0xe8, 0x80,

    /* U+0067 "g" */
    0x68, 0xb9, 0x60,

    /* U+0068 "h" */
    0x99, 0xf9, 0x90,

    /* U+0069 "i" */
    0xf8,

    /* U+006A "j" */
    0x11, 0x19, 0x60,

    /* U+006B "k" */
    0x9a, 0xca, 0x90,

    /* U+006C "l" */
    0x88, 0x88, 0xf0,

    /* U+006D "m" */
    0x9f, 0x99, 0x90,

    /* U+006E "n" */
    0x9d, 0xb9, 0x90,

    /* U+006F "o" */
    0x69, 0x99, 0x60,

    /* U+0070 "p" */
    0xe9, 0xe8, 0x80,

    /* U+0071 "q" */
    0x69, 0x9b, 0x70,

    /* U+0072 "r" */
    0xe9, 0xea, 0x90,

    /* U+0073 "s" */
    0x78, 0x61, 0xe0,

    /* U+0074 "t" */
    0xe9, 0x24,

    /* U+0075 "u" */
    0x99, 0x99, 0x60,

    /* U+0076 "v" */
    0xb6, 0xd4,

    /* U+0077 "w" */
    0x99, 0x9f, 0x90,

    /* U+0078 "x" */
    0x99, 0x69, 0x90,

    /* U+0079 "y" */
    0xb5, 0x24,

    /* U+007A "z" */
    0xf1, 0x68, 0xf0,

    /* U+007B "{" */
    0x6b, 0x26,

    /* U+007C "|" */
    0xd8,

    /* U+007D "}" */
    0xc9, 0xac,

    /* U+007E "~" */
    0x5a
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 80, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 80, .box_w = 1, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 2, .adv_w = 80, .box_w = 3, .box_h = 2, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 3, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 6, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 9, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 12, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 15, .adv_w = 80, .box_w = 1, .box_h = 2, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 16, .adv_w = 80, .box_w = 2, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 18, .adv_w = 80, .box_w = 2, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 20, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 23, .adv_w = 80, .box_w = 3, .box_h = 3, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 25, .adv_w = 80, .box_w = 2, .box_h = 2, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 26, .adv_w = 80, .box_w = 3, .box_h = 1, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 27, .adv_w = 80, .box_w = 1, .box_h = 1, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 28, .adv_w = 80, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 30, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 33, .adv_w = 80, .box_w = 2, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 35, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 38, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 41, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 44, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 47, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 50, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 53, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 56, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 59, .adv_w = 80, .box_w = 1, .box_h = 3, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 60, .adv_w = 80, .box_w = 1, .box_h = 4, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 61, .adv_w = 80, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 63, .adv_w = 80, .box_w = 3, .box_h = 3, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 65, .adv_w = 80, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 67, .adv_w = 80, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 69, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 72, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 75, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 78, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 81, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 84, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 87, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 90, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 93, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 96, .adv_w = 80, .box_w = 1, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 97, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 100, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 103, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 106, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 109, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 112, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 115, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 118, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 121, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 124, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 127, .adv_w = 80, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 129, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 132, .adv_w = 80, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 134, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 137, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 140, .adv_w = 80, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 142, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 145, .adv_w = 80, .box_w = 2, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 147, .adv_w = 80, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 149, .adv_w = 80, .box_w = 2, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 151, .adv_w = 80, .box_w = 3, .box_h = 2, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 152, .adv_w = 80, .box_w = 4, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 153, .adv_w = 80, .box_w = 2, .box_h = 2, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 154, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 157, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 160, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 163, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 166, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 169, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 172, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 175, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 178, .adv_w = 80, .box_w = 1, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 179, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 182, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 185, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 188, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 191, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 194, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 197, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 200, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 203, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 206, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 209, .adv_w = 80, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 211, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 214, .adv_w = 80, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 216, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 219, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 222, .adv_w = 80, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 224, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 227, .adv_w = 80, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 229, .adv_w = 80, .box_w = 1, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 230, .adv_w = 80, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 232, .adv_w = 80, .box_w = 4, .box_h = 2, .ofs_x = 0, .ofs_y = 2}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 95, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t cg_pixel_4x5_mono = {
#else
lv_font_t cg_pixel_4x5_mono = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 5,          /*The maximum line height required by the font*/
    .base_line = 0,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = 0,
    .underline_thickness = 0,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if CG_PIXEL_4X5_MONO*/

