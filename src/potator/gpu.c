#pragma GCC optimize("Ofast")

#include "pico.h"
#include "pico/platform.h"
#include "gpu.h"

#include "memorymap.h"
#include "graphics.h"

#include <stdlib.h>
#include <string.h>

// RGB555 or RGBA5551
/*
#define RGB555(R, G, B) (((int)B << 10) | ((int)G << 5) | ((int)R) | ( 1 << 15))

static uint32 rgb555(uint8 r, uint8 g, uint8 b)
{
    return RGB555(r >> 3, g >> 3, b >> 3);
}

static SV_MapRGBFunc mapRGB = rgb555;
*/

static const uint8 palettes[SV_COLOR_SCHEME_COUNT][12] = {
        {   /* SV_COLOR_SCHEME_DEFAULT */
                252,  252,  252,
                168,  168,  168,
                84,   84,   84,
                0,    0,    0,
        },
        {   /* SV_COLOR_SCHEME_AMBER */
                252,  154,  0,
                168,  102,  0,
                84,   51,   0,
                0,    0,    0,
        },
        {   /* SV_COLOR_SCHEME_GREEN */
                50,   227,  50,
                34,   151,  34,
                17,   76,   17,
                0,    0,    0,
        },
        {   /* SV_COLOR_SCHEME_BLUE */
                0,    154,  252,
                0,    102,  168,
                0,    51,   84,
                0,    0,    0,
        },
        {   /* SV_COLOR_SCHEME_BGB */
                224,  248,  208,
                136,  192,  112,
                52,   104,  86,
                8,    24,   32,
        },
        {   /* SV_COLOR_SCHEME_WATAROO */
                0x7b, 0xc7, 0x7b,
                0x52, 0xa6, 0x8c,
                0x2e, 0x62, 0x60,
                0x0d, 0x32, 0x2e,
        },
        {   /* SV_COLOR_SCHEME_GB_DMG */
                0x57, 0x82, 0x00,
                0x31, 0x74, 0x00,
                0x00, 0x51, 0x21,
                0x00, 0x42, 0x0C,
        },
        {   /* SV_COLOR_SCHEME_GB_POCKET */
                0xA7, 0xB1, 0x9A,
                0x86, 0x92, 0x7C,
                0x53, 0x5f, 0x49,
                0x2A, 0x33, 0x25,
        },
        {   /* SV_COLOR_SCHEME_GB_LIGHT */
                0x01, 0xCB, 0xDF,
                0x01, 0xB6, 0xD5,
                0x26, 0x9B, 0xAD,
                0x00, 0x77, 0x8D,
        },
        {   /* SV_COLOR_SCHEME_BLOSSOM_PINK */
                0xF0, 0x98, 0x98,
                0xA8, 0x6A, 0x6A,
                0x60, 0x3C, 0x3C,
                0x18, 0x0F, 0x0F,
        },
        {   /* SV_COLOR_SCHEME_BUBBLES_BLUE */
                0x88, 0xD0, 0xF0,
                0x5F, 0x91, 0xA8,
                0x36, 0x53, 0x60,
                0x0D, 0x14, 0x18,
        },
        {   /* SV_COLOR_SCHEME_BUTTERCUP_GREEN */
                0xB8, 0xE0, 0x88,
                0x80, 0x9C, 0x5F,
                0x49, 0x59, 0x36,
                0x12, 0x16, 0x0D,
        },
        {   /* SV_COLOR_SCHEME_DIGIVICE */
                0x8C, 0x8C, 0x73,
                0x64, 0x64, 0x53,
                0x38, 0x38, 0x2E,
                0x00, 0x00, 0x00,
        },
        {   /* SV_COLOR_SCHEME_GAME_COM */
                0xA7, 0xBF, 0x6B,
                0x6F, 0x8F, 0x4F,
                0x0F, 0x4F, 0x2F,
                0x00, 0x00, 0x00,
        },
        {   /* SV_COLOR_SCHEME_GAMEKING */
                0x8C, 0xCE, 0x94,
                0x6B, 0x9C, 0x63,
                0x50, 0x65, 0x41,
                0x18, 0x42, 0x21,
        },
        {   /* SV_COLOR_SCHEME_GAME_MASTER */
                0x82, 0x9F, 0xA6,
                0x5A, 0x78, 0x7E,
                0x38, 0x4A, 0x50,
                0x2D, 0x2D, 0x2B,
        },
        {   /* SV_COLOR_SCHEME_GOLDEN_WILD */
                0xB9, 0x9F, 0x65,
                0x81, 0x6F, 0x46,
                0x4A, 0x3F, 0x28,
                0x12, 0x0F, 0x0A,
        },
        {   /* SV_COLOR_SCHEME_GREENSCALE */
                0x9C, 0xBE, 0x0C,
                0x6E, 0x87, 0x0A,
                0x2C, 0x62, 0x34,
                0x0C, 0x36, 0x0C,
        },
        {   /* SV_COLOR_SCHEME_HOKAGE_ORANGE */
                0xEA, 0x83, 0x52,
                0xA3, 0x5B, 0x39,
                0x5D, 0x34, 0x20,
                0x17, 0x0D, 0x08,
        },
        {   /* SV_COLOR_SCHEME_LABO_FAWN */
                0xD7, 0xAA, 0x73,
                0x96, 0x76, 0x50,
                0x56, 0x44, 0x2E,
                0x15, 0x11, 0x0B,
        },
        {   /* SV_COLOR_SCHEME_LEGENDARY_SUPER_SAIYAN */
                0xA5, 0xDB, 0x5A,
                0x73, 0x99, 0x3E,
                0x42, 0x57, 0x24,
                0x10, 0x15, 0x09,
        },
        {   /* SV_COLOR_SCHEME_MICROVISION */
                0xA0, 0xA0, 0xA0,
                0x78, 0x78, 0x78,
                0x50, 0x50, 0x50,
                0x30, 0x30, 0x30,
        },
        {   /* SV_COLOR_SCHEME_MILLION_LIVE_GOLD */
                0xCD, 0xB2, 0x61,
                0x8F, 0x7C, 0x43,
                0x52, 0x47, 0x26,
                0x14, 0x11, 0x09,
        },
        {   /* SV_COLOR_SCHEME_ODYSSEY_GOLD */
                0xC2, 0xA0, 0x00,
                0x87, 0x70, 0x00,
                0x4D, 0x40, 0x00,
                0x13, 0x10, 0x00,
        },
        {   /* SV_COLOR_SCHEME_SHINY_SKY_BLUE */
                0x8C, 0xB6, 0xDF,
                0x62, 0x7F, 0x9C,
                0x38, 0x48, 0x59,
                0x0E, 0x12, 0x16,
        },
        {   /* SV_COLOR_SCHEME_SLIME_BLUE */
                0x2F, 0x8C, 0xCC,
                0x20, 0x62, 0x8E,
                0x12, 0x38, 0x51,
                0x04, 0x0E, 0x14,
        },
        {   /* SV_COLOR_SCHEME_TI_83 */
                0x9C, 0xA6, 0x84,
                0x72, 0x7C, 0x5A,
                0x46, 0x4A, 0x35,
                0x18, 0x18, 0x10,
        },
        {   /* SV_COLOR_SCHEME_TRAVEL_WOOD */
                0xF8, 0xD8, 0xB0,
                0xA0, 0x80, 0x58,
                0x70, 0x50, 0x30,
                0x48, 0x28, 0x10,
        },
        {   /* SV_COLOR_SCHEME_VIRTUAL_BOY */
                0xE3, 0x00, 0x00,
                0x95, 0x00, 0x00,
                0x56, 0x00, 0x00,
                0x00, 0x00, 0x00,
        },
        {   /* SV_TVLINK */
                0x00, 0x00, 0x00,
                0x00, 0x48, 0x55,
                0x00, 0x91, 0xaa,
                0xff, 0xff, 0xff,
        },
};

//uint32 watara_palette[4];
static int paletteIndex;

int ghostCount = 8;

/*
static uint8 screenBuffer8[SV_H][SV_W] = { 0 };

static void add_ghosting(uint32 scanline, uint8 *backbuffer, uint8 innerx, uint8 size)
{
    static int line = 0;
    uint8* pln = screenBuffer8[line];
    for (size_t x = 0; x < size; ++x) {
        int c8 = pln[x] * (ghostCount - 1) / ghostCount; // prev. state in 8 bit format (reduced to 1/gh)
        int c2 = backbuffer[x]; // curr. state in 2 bits format
        int c8n = c2 << 6; // new state in 8 bit format
        if (c8n < c8) c8n = c8;
        pln[x] = c8n; // save it to next step
        backbuffer[x] = c8n >> 6; // convert to 2-bits
    }

    line = (line + 1) % SV_H;
}
*/
void gpu_init(void) {
    //   watara_palette = (uint32*)malloc(4 * sizeof(int32));
}

void gpu_reset(void) {
    gpu_set_map_func(NULL);
    gpu_set_color_scheme(SV_COLOR_SCHEME_DEFAULT);
    // gpu_set_ghosting(0);
}

void gpu_done(void) {
//    free(watara_palette); watara_palette = NULL;
    // gpu_set_ghosting(0);
}

void gpu_set_map_func(SV_MapRGBFunc func) {
    //   mapRGB = func;
    // if (mapRGB == NULL)
    //   mapRGB = rgb555;
}

void gpu_set_color_scheme(int colorScheme) {
    int i;
    if (colorScheme < 0 || colorScheme >= SV_COLOR_SCHEME_COUNT)
        return;
    for (i = 0; i < 4; i++) {
        graphics_set_palette(i, RGB888(palettes[colorScheme][i * 3 + 0],
                                       palettes[colorScheme][i * 3 + 1],
                                       palettes[colorScheme][i * 3 + 2])
        );
    }
    paletteIndex = colorScheme;
}
/*

void __time_critical_func(gpu_render_scanline2)(uint32 scanline, uint8 *backbuffer, uint8 innerx, uint8 size) {
    uint8 *vram_line = memorymap_getUpperRamPointer() + scanline;
    uint8 x, j = innerx, b = 0;

    // #1
    if (j & 3) {
        b = *vram_line++;
        b >>= (j & 3) * 2;
    }
//#pragma GCC unroll 320
    if (ghostCount) {
        for (x = 0; x < size; x++, j++) {
            if (!(j & 3)) {
                b = *(vram_line++);
            }
            //int color = (b & 3); // curr. state in 2 bits format
            {
                int c8 = backbuffer[x] - 4; // prev. state in 8 bit format (reduced to 1/gh)
                int c8n = (b & 3) << 4; // new state in 8 bit format
                if (c8n < c8) c8n = c8;
                backbuffer[x] = c8n;

                // save it to next step
                b >>= 2;
            }
        }
    } else {
        for (x = 0; x < size; x++, j++) {
            if (!(j & 3)) {
                b = *(vram_line++);
            }
            backbuffer[x] = (b & 3) << 4;
            // save it to next step
            b >>= 2;
        }
    }

}

*/
void __time_critical_func(gpu_render_scanline)(uint32 scanline, uint8 *backbuffer, uint8 innerx, uint8 size) {
    uint8 *vram_line = memorymap_getUpperRamPointer() + scanline;
    uint8 x = 0, j = innerx & 3;
    static int previous_color, current_color = 0;

    uint8 b = *vram_line++;

    if (j) {
        b >>= j * 2;
    }

    if (ghostCount) {
#pragma GCC unroll 4
        while (x < size) {
            backbuffer[x++] = (b & 3) << 4;
            b >>= 2;
            backbuffer[x++] = (b & 3) << 4;
            b >>= 2;
            backbuffer[x++] = (b & 3) << 4;
            b >>= 2;
            backbuffer[x++] = (b & 3) << 4;

            b = *vram_line++;
        }
    } else {
#pragma GCC unroll 4
        while (x < size) {
            previous_color = backbuffer[x] - 4; // prev. state in 8 bit format (reduced to 1/gh)
            current_color = (b & 3) << 4; // new state in 8 bit format
            if (current_color < previous_color) current_color = previous_color;
            backbuffer[x++] = current_color;
            b >>= 2;

            previous_color = backbuffer[x] - 4; // prev. state in 8 bit format (reduced to 1/gh)
            current_color = (b & 3) << 4; // new state in 8 bit format
            if (current_color < previous_color) current_color = previous_color;
            backbuffer[x++] = current_color;
            b >>= 2;

            previous_color = backbuffer[x] - 4; // prev. state in 8 bit format (reduced to 1/gh)
            current_color = (b & 3) << 4; // new state in 8 bit format
            if (current_color < previous_color) current_color = previous_color;
            backbuffer[x++] = current_color;
            b >>= 2;

            previous_color = backbuffer[x] - 4; // prev. state in 8 bit format (reduced to 1/gh)
            current_color = (b & 3) << 4; // new state in 8 bit format
            if (current_color < previous_color) current_color = previous_color;
            backbuffer[x++] = current_color;

            b = *vram_line++;
        }
    }
}

void gpu_set_ghosting(int frameCount) {
    int i;
    if (frameCount < 0)
        ghostCount = 0;
    else if (frameCount > SV_GHOSTING_MAX)
        ghostCount = SV_GHOSTING_MAX;
    else
        ghostCount = frameCount;
}
