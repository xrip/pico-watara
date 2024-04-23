#pragma GCC optimize("Ofast")
#include "pico/platform.h"

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "supervision.h"

#include "controls.h"
#include "gpu.h"
#include "memorymap.h"
#include "sound.h"
#include "timer.h"
#include "./m6502/m6502.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static M6502 m6502_registers;
static BOOL irq = FALSE;

void m6502_set_irq_line(BOOL assertLine)
{
    m6502_registers.IRequest = assertLine ? INT_IRQ : INT_NONE;
    irq = assertLine;
}

byte Loop6502(register M6502 *R)
{
    if (irq) {
        irq = FALSE;
        return INT_IRQ;
    }
    return INT_QUIT;
}

void supervision_init(void)
{
    gpu_init();
    memorymap_init();
    // 256 * 256 -- 1 frame (61 FPS)
    // 256 - 4MHz,
    // 512 - 8MHz, ...
    m6502_registers.IPeriod = 256;
}

void supervision_reset(void)
{
    controls_reset();
    gpu_reset();
    memorymap_reset();
    sound_reset();
    timer_reset();

    Reset6502(&m6502_registers);
    irq = FALSE;
}

void supervision_done(void)
{
    gpu_done();
    memorymap_done();
}

BOOL supervision_load(const uint8 *rom, uint32 romSize)
{
    if (!memorymap_load(rom, romSize))
        return FALSE;
    supervision_reset();
    return TRUE;
}

void __time_critical_func(supervision_exec_ex)(uint8 *backbuffer, int16 backbufferWidth, BOOL skipFrame)
{
    uint32 i, scan;
    uint8 *regs = memorymap_getRegisters();
    uint8 innerx, size;

    // Number of iterations = 256 * 256 / m6502_registers.IPeriod
    for (i = 0; i < 256; i++) {
        Run6502(&m6502_registers);
        timer_exec(m6502_registers.IPeriod);
    }

    if (!skipFrame) {
        scan   = regs[XPOS] / 4 + regs[YPOS] * 0x30;
        innerx = regs[XPOS] & 3;
        size   = regs[XSIZE]; // regs[XSIZE] <= SV_W
        if (size > SV_W)
            size = SV_W; // 192: Chimera, Matta Blatta, Tennis Pro '92

        for (i = 0; i < SV_H; i++) {
            if (scan >= 0x1fe0)
                scan -= 0x1fe0; // SSSnake
            gpu_render_scanline(scan, backbuffer, innerx, SV_W);
            backbuffer += backbufferWidth;
            scan += 0x30;
        }
    }

    if (Rd6502(0x2026) & 0x01)
        Int6502(&m6502_registers, INT_NMI);

    sound_decrement();
}

void supervision_set_map_func(SV_MapRGBFunc func)
{
    gpu_set_map_func(func);
}

void supervision_set_color_scheme(int colorScheme)
{
    gpu_set_color_scheme(colorScheme);
}

void supervision_set_ghosting(int frameCount)
{
    gpu_set_ghosting(frameCount);
}

void supervision_set_input(uint8 data)
{
    controls_state_write(data);
}

#define EXPAND_M6502 \
    X(uint8, A) \
    X(uint8, P) \
    X(uint8, X) \
    X(uint8, Y) \
    X(uint8, S) \
    X(uint8, PC.B.l) \
    X(uint8, PC.B.h) \
    X(int32, IPeriod) \
    X(int32, ICount) \
    X(uint8, IRequest) \
    X(uint8, AfterCLI) \
    X(int32, IBackup)

uint32 supervision_save_state_buf_size(void)
{
    return memorymap_save_state_buf_size() +
           sound_save_state_buf_size() +
           timer_save_state_buf_size() +
           sizeof(uint8) +
           sizeof(uint8) +
           sizeof(uint8) +
           sizeof(uint8) +
           sizeof(uint8) +
           sizeof(uint8) +
           sizeof(uint8) +
           sizeof(int32) +
           sizeof(int32) +
           sizeof(uint8) +
           sizeof(uint8) +
           sizeof(int32) +
           sizeof(uint8) +
           128; /* Add some padding, just in case... */
}

BOOL supervision_save_state_buf(uint8 *data, uint32 size)
{
    if (!data || size < supervision_save_state_buf_size()) {
        return FALSE;
    }

    memorymap_save_state_buf(data);
    data += memorymap_save_state_buf_size();

    sound_save_state_buf(data);
    data += sound_save_state_buf_size();

    timer_save_state_buf(data);
    data += timer_save_state_buf_size();

#define X(type, member) WRITE_BUF_##type(m6502_registers.member, data);
    EXPAND_M6502
#undef X
    WRITE_BUF_BOOL(irq, data);

    return TRUE;
}

BOOL supervision_load_state_buf(const uint8 *data, uint32 size)
{
    if (!data || size < supervision_save_state_buf_size()) {
        return FALSE;
    }

    memorymap_load_state_buf(data);
    data += memorymap_save_state_buf_size();

    sound_load_state_buf(data);
    data += sound_save_state_buf_size();

    timer_load_state_buf(data);
    data += timer_save_state_buf_size();

#define X(type, member) READ_BUF_##type(m6502_registers.member, data);
    EXPAND_M6502
#undef X
    READ_BUF_BOOL(irq, data);

    return TRUE;
}
