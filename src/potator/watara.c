#pragma GCC optimize("Ofast")
#include "pico.h"

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
extern uint8 regs[0x2000];
extern uint8 upperRam[0x2000];
extern int ghostCount;

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
    //regs = memorymap_getRegisters();
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

static uint8_t expected_screen[200*240] = { 0 };
static uint8_t rich_screen[200*240] = { 0 }; // current extended value

static inline uint8_t __time_critical_func(convert_to_rich_format)(uint8_t v) {
    return ((v & 1) ? 0b1111 : 0) | (((v >> 1) & 1) ? 0b11110000: 0);
}

void __time_critical_func(supervision_exec_ex)(uint8 *backbuffer, uint32 backbufferWidth, BOOL skipFrame, uint8_t ghosting)
{
    // Number of iterations = 256 * 256 / m6502_registers.IPeriod
    for (uint32 i = 0; i < 256; ++i) {
        Run6502(&m6502_registers);
        timer_exec(m6502_registers.IPeriod);
    }
    if (!skipFrame) {
        uint32 scanline = regs[XPOS] / 4 + regs[YPOS] * 0x30;
        uint8 innerx = regs[XPOS] & 3;
        uint8 size   = regs[XSIZE]; // regs[XSIZE] <= SV_W
        if (size > SV_W)
            size = SV_W; // 192: Chimera, Matta Blatta, Tennis Pro '92

        uint8* p_exp = expected_screen;
        for (uint32 i = 0; i < SV_H; ++i) {
            if (scanline >= 0x1fe0) {
                scanline -= 0x1fe0; // SSSnake
            }
            uint8 *vram_line = upperRam + scanline;
            uint8 x = 0;
            uint8 b = *vram_line++;
            if (innerx) {
                b >>= innerx * 2;
            }
#pragma GCC unroll 4
            while (x < size) {
                p_exp[x++] = convert_to_rich_format(b); b >>= 2;
                p_exp[x++] = convert_to_rich_format(b); b >>= 2;
                p_exp[x++] = convert_to_rich_format(b); b >>= 2;
                p_exp[x++] = convert_to_rich_format(b); b = *vram_line++;
            }
            p_exp += backbufferWidth;
            scanline += 0x30;
        }
        uint8_t ghost_speed = ghosting < 7 ? (7 - ghosting) : 1;
        ghosting = (0xFF >> (ghosting + 1)); // mask to extend values
        size_t bytes_written = p_exp - expected_screen;
        p_exp = expected_screen;
        uint8_t* p_out = backbuffer;
        uint8_t* p_rich = rich_screen;
        for (int i = 0; i < bytes_written; ++i) {
            uint8_t new_v = *p_exp++;
            uint8_t pre_v = *p_rich;
            uint8_t v;
            if (new_v > pre_v) {
                v = (pre_v << ghost_speed) | ghosting;
                if (v > new_v) v = new_v;
            } else {
                v = pre_v >> ghost_speed;
                if (v < new_v) v = new_v;
            }
            *p_rich++ = v;
            // back to output style format -> 0bxy0000
            *p_out++ = (((v >> 4) ? 0b10 : 0) | ((v & 0b1111) ? 0b01 : 0)) << 4;
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
