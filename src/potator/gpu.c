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

//uint32 watara_palette[4];

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
}

void gpu_done(void) {
//    free(watara_palette); watara_palette = NULL;
}

void gpu_set_map_func(SV_MapRGBFunc func) {
    //   mapRGB = func;
    // if (mapRGB == NULL)
    //   mapRGB = rgb555;
}
