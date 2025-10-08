#pragma once

#include <stdbool.h>
#include <stdint.h>

struct input_bits_t {
    bool right: true;
    bool left: true;
    bool down: true;
    bool up: true;
    bool b: true;
    bool a: true;
    bool select: true;
    bool start: true;
};

typedef struct kbd_s {
    input_bits_t bits;
    int8_t h_code;
} kbd_t;

typedef union {
    input_bits_t bits;
    uint8_t state;
} controller;

extern controller gamepad1;