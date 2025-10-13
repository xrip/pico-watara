#include <pico.h>

#include "main.h"
#ifdef PICO_RP2350
#include <hardware/regs/qmi.h>
#include <hardware/structs/qmi.h>
#endif

#include <cstdio>
#include <cstring>
#include <hardware/flash.h>
#include <hardware/clocks.h>
#include <hardware/vreg.h>
#include <pico/multicore.h>
#include <pico/stdlib.h>
#include <hardware/watchdog.h>

extern "C" {
#include <potator/supervision.h>
#include "potator/sound.h"
}
#include "frame.h"
#include <graphics.h>
#include "audio.h"

#include "nespad.h"
#include "ff.h"
#include "ps2kbd_mrmltr.h"

#define HOME_DIR "\\WATARA"
extern char __flash_binary_end;
#define FLASH_TARGET_OFFSET (((((uintptr_t)&__flash_binary_end - XIP_BASE) / FLASH_SECTOR_SIZE) + 4) * FLASH_SECTOR_SIZE)
static const uintptr_t rom = XIP_BASE + FLASH_TARGET_OFFSET;

#define AUDIO_SAMPLE_RATE SV_SAMPLE_RATE
#define AUDIO_BUFFER_SIZE ((SV_SAMPLE_RATE / 60) << 1)

char __uninitialized_ram(filename[256]);
static uint32_t __uninitialized_ram(rom_size) = 0;

static FATFS fs;
bool volatile reboot = false;
bool limit_fps = true;
semaphore vga_start_semaphore;

uint8_t SCREEN[200][240];
uint8_t TEXT_BUFFER[TEXTMODE_COLS*TEXTMODE_ROWS*2];

SETTINGS settings = {
    .version = 1,
    .swap_ab = false,
    .aspect_ratio = false,
    .ghosting = 4,
    .palette = SV_COLOR_SCHEME_WATAROO,
    .save_slot = 0,
    .rgb0 = 0xCCFFFF,
    .rgb1 = 0xFFB266,
    .rgb2 = 0xCC0066,
    .rgb3 = 0x663300,
    .instant_ignition = false
};

uint32_t rgb0;
uint32_t rgb1;
uint32_t rgb2;
uint32_t rgb3;

static kbd_t keyboard = {
    .bits = { false, false, false, false, false, false, false, false },
    .h_code = -1
};
controller gamepad1 = { false, false, false, false, false, false, false, false };
//static input_bits_t gamepad2_bits = { false, false, false, false, false, false, false, false };

bool swap_ab = false;

void gamepad1_update() {
    nespad_read();
    
    if (settings.swap_ab) {
        gamepad1.bits.b = keyboard.bits.a || (nespad_state & DPAD_A) != 0;
        gamepad1.bits.a = keyboard.bits.b || (nespad_state & DPAD_B) != 0;
    } else {
        gamepad1.bits.a = keyboard.bits.a || (nespad_state & DPAD_A) != 0;
        gamepad1.bits.b = keyboard.bits.b || (nespad_state & DPAD_B) != 0;
    }

    gamepad1.bits.select = keyboard.bits.select || (nespad_state & DPAD_SELECT) != 0;
    gamepad1.bits.start = keyboard.bits.start || (nespad_state & DPAD_START) != 0;
    gamepad1.bits.up = keyboard.bits.up || (nespad_state & DPAD_UP) != 0;
    gamepad1.bits.down = keyboard.bits.down ||(nespad_state & DPAD_DOWN) != 0;
    gamepad1.bits.left = keyboard.bits.left || (nespad_state & DPAD_LEFT) != 0;
    gamepad1.bits.right = keyboard.bits.right || (nespad_state & DPAD_RIGHT) != 0;
}

static bool isInReport(hid_keyboard_report_t const* report, const unsigned char keycode) {
    for (unsigned char i: report->keycode) {
        if (i == keycode) {
            return true;
        }
    }
    return false;
}

static volatile bool altPressed = false;
static volatile bool ctrlPressed = false;
static volatile uint8_t fxPressedV = 0;

void
__not_in_flash_func(process_kbd_report)(hid_keyboard_report_t const* report, hid_keyboard_report_t const* prev_report) {
    /* printf("HID key report modifiers %2.2X report ", report->modifier);
    for (unsigned char i: report->keycode)
        printf("%2.2X", i);
    printf("\r\n");
     */
    uint8_t h_code = -1;
    if ( isInReport(report, HID_KEY_0) || isInReport(report, HID_KEY_KEYPAD_0)) h_code = 0;
    else if ( isInReport(report, HID_KEY_1) || isInReport(report, HID_KEY_KEYPAD_1)) h_code = 1;
    else if ( isInReport(report, HID_KEY_2) || isInReport(report, HID_KEY_KEYPAD_2)) h_code = 2;
    else if ( isInReport(report, HID_KEY_3) || isInReport(report, HID_KEY_KEYPAD_3)) h_code = 3;
    else if ( isInReport(report, HID_KEY_4) || isInReport(report, HID_KEY_KEYPAD_4)) h_code = 4;
    else if ( isInReport(report, HID_KEY_5) || isInReport(report, HID_KEY_KEYPAD_5)) h_code = 5;
    else if ( isInReport(report, HID_KEY_6) || isInReport(report, HID_KEY_KEYPAD_6)) h_code = 6;
    else if ( isInReport(report, HID_KEY_7) || isInReport(report, HID_KEY_KEYPAD_7)) h_code = 7;
    else if ( isInReport(report, HID_KEY_8) || isInReport(report, HID_KEY_KEYPAD_8)) h_code = 8;
    else if ( isInReport(report, HID_KEY_9) || isInReport(report, HID_KEY_KEYPAD_9)) h_code = 9;
    else if ( isInReport(report, HID_KEY_A)) h_code = 10;
    else if ( isInReport(report, HID_KEY_B)) h_code = 11;
    else if ( isInReport(report, HID_KEY_C)) h_code = 12;
    else if ( isInReport(report, HID_KEY_D)) h_code = 13;
    else if ( isInReport(report, HID_KEY_E)) h_code = 14;
    else if ( isInReport(report, HID_KEY_F)) h_code = 15;
    keyboard.h_code = h_code;
    keyboard.bits.start = isInReport(report, HID_KEY_ENTER) || isInReport(report, HID_KEY_KEYPAD_ENTER);
    keyboard.bits.select = isInReport(report, HID_KEY_BACKSPACE) || isInReport(report, HID_KEY_ESCAPE) || isInReport(report, HID_KEY_KEYPAD_ADD);

    keyboard.bits.a = isInReport(report, HID_KEY_Z) || isInReport(report, HID_KEY_O) || isInReport(report, HID_KEY_KEYPAD_0);
    keyboard.bits.b = isInReport(report, HID_KEY_X) || isInReport(report, HID_KEY_P) || isInReport(report, HID_KEY_KEYPAD_DECIMAL);

    bool b7 = isInReport(report, HID_KEY_KEYPAD_7);
    bool b9 = isInReport(report, HID_KEY_KEYPAD_9);
    bool b1 = isInReport(report, HID_KEY_KEYPAD_1);
    bool b3 = isInReport(report, HID_KEY_KEYPAD_3);

    keyboard.bits.up = b7 || b9 || isInReport(report, HID_KEY_ARROW_UP) || isInReport(report, HID_KEY_W) || isInReport(report, HID_KEY_KEYPAD_8);
    keyboard.bits.down = b1 || b3 || isInReport(report, HID_KEY_ARROW_DOWN) || isInReport(report, HID_KEY_S) || isInReport(report, HID_KEY_KEYPAD_2) || isInReport(report, HID_KEY_KEYPAD_5);
    keyboard.bits.left = b7 || b1 || isInReport(report, HID_KEY_ARROW_LEFT) || isInReport(report, HID_KEY_A) || isInReport(report, HID_KEY_KEYPAD_4);
    keyboard.bits.right = b9 || b3 || isInReport(report, HID_KEY_ARROW_RIGHT)  || isInReport(report, HID_KEY_D) || isInReport(report, HID_KEY_KEYPAD_6);

    altPressed = isInReport(report, HID_KEY_ALT_LEFT) || isInReport(report, HID_KEY_ALT_RIGHT);
    ctrlPressed = isInReport(report, HID_KEY_CONTROL_LEFT) || isInReport(report, HID_KEY_CONTROL_RIGHT);
    
    if (altPressed && ctrlPressed && isInReport(report, HID_KEY_DELETE)) {
        watchdog_enable(10, true);
        while(true) {
            tight_loop_contents();
        }
    }
    if (ctrlPressed || altPressed) {
        uint8_t fxPressed = 0;
        if (isInReport(report, HID_KEY_F1)) fxPressed = 1;
        else if (isInReport(report, HID_KEY_F2)) fxPressed = 2;
        else if (isInReport(report, HID_KEY_F3)) fxPressed = 3;
        else if (isInReport(report, HID_KEY_F4)) fxPressed = 4;
        else if (isInReport(report, HID_KEY_F5)) fxPressed = 5;
        else if (isInReport(report, HID_KEY_F6)) fxPressed = 6;
        else if (isInReport(report, HID_KEY_F7)) fxPressed = 7;
        else if (isInReport(report, HID_KEY_F8)) fxPressed = 8;
        fxPressedV = fxPressed;
    }
}

Ps2Kbd_Mrmltr ps2kbd(
    pio1,
    PS2KBD_GPIO_FIRST,
    process_kbd_report
);

static const uint8 palettes[SV_COLOR_SCHEME_COUNT][12] = {
        {   /* SV_COLOR_SCHEME_DEFAULT */
                252,  252,  252,
                168,  168,  168,
                84,   84,   84,
                0,    0,    0,
        },
        {   /* SV_COLOR_SCHEME_WATAROO */
                0x7b, 0xc7, 0x7b,
                0x52, 0xa6, 0x8c,
                0x2e, 0x62, 0x60,
                0x0d, 0x32, 0x2e,
        },
        {   /* SV_COLOR_SCHEME_BGB */
                224,  248,  208,
                136,  192,  112,
                52,   104,  86,
                8,    24,   32,
        },
        { // OCEAN SAND  
            0xD4, 0xFF, 0xF3,
            0x34, 0x9B, 0xC0,
            0xF7, 0x90, 0x36,
            0x00, 0x52, 0x52
        },
        { // AUTUMN FOREST  
            0xD4, 0xFF, 0xFD,
            0x13, 0x95, 0x66,
            0xF7, 0x90, 0x36,
            0x00, 0x52, 0x52
        },
        { // RED FOX
            0xD4, 0xFF, 0xFD,
            0xE8, 0xAE, 0x74,
            0xBD, 0x5F, 0x00,
            0x6B, 0x04, 0x00
        },
        { // MINT SAND  
            0xD4, 0xFF, 0xFD,
            0xFF, 0x80, 0x00,
            0x00, 0x99, 0x99,
            0x00, 0x52, 0x52
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

const int base_bezel = 128;

static inline uint32_t fast1of32(uint32_t v, int i) {
///    return (uint32_t)((v / 32.0) * (i + 1)) & 0xFF;
    v -= (31 - i);
    if (v > 0xFF) v = 0;
    return v;
}

static inline void update_palette() {
    if (SV_COLOR_SCHEME_COUNT <= settings.palette) {
        rgb0 = settings.rgb0;
        rgb1 = settings.rgb1;
        rgb2 = settings.rgb2;
        rgb3 = settings.rgb3;
    } else {
        const uint8_t* palette = palettes[settings.palette];
        rgb0 = RGB888(palette[0], palette[1], palette[2]);
        rgb1 = RGB888(palette[3], palette[4], palette[5]);
        rgb2 = RGB888(palette[6], palette[7], palette[8]);
        rgb3 = RGB888(palette[9], palette[10], palette[11]);
    }
    uint32_t r, g, b;
    r = rgb0 >> 16;
    g = (rgb0 >> 8) & 0xFF;
    b = rgb0 & 0xFF;
    for (int i = 0; i < 32; ++i) {
        graphics_set_palette(i, RGB888(fast1of32(r, i), fast1of32(g, i), fast1of32(b, i)));
    }
    r = rgb1 >> 16;
    g = (rgb1 >> 8) & 0xFF;
    b = rgb1 & 0xFF;
    for (int i = 0; i < 32; ++i) {
        graphics_set_palette(i + 32, RGB888(fast1of32(r, i), fast1of32(g, i), fast1of32(b, i)));
    }
    r = rgb2 >> 16;
    g = (rgb2 >> 8) & 0xFF;
    b = rgb2 & 0xFF;
    for (int i = 0; i < 32; ++i) {
        graphics_set_palette(i + 64, RGB888(fast1of32(r, i), fast1of32(g, i), fast1of32(b, i)));
    }
    r = rgb3 >> 16;
    g = (rgb3 >> 8) & 0xFF;
    b = rgb3 & 0xFF;
    for (int i = 0; i < 32; ++i) {
        graphics_set_palette(i + 96, RGB888(fast1of32(r, i), fast1of32(g, i), fast1of32(b, i)));
    }

    graphics_set_palette((base_bezel + 0), RGB888(0xff, 0xff, 0xff));
    graphics_set_palette((base_bezel + 1), RGB888(0x00, 0x00, 0x00));
    graphics_set_palette((base_bezel + 2), RGB888(0xff, 0x00, 0x00));
    graphics_set_palette((base_bezel + 3), RGB888(0xff, 0xff, 0x00));
    graphics_set_palette((base_bezel + 4), RGB888(0x00, 0xff, 0x00));
    graphics_set_palette((base_bezel + 5), RGB888(0x00, 0xff, 0xff));
    graphics_set_palette((base_bezel + 6), RGB888(0x00, 0x00, 0xff));
    graphics_set_palette((base_bezel + 7), RGB888(0xff, 0x00, 0xff));
}

uint_fast32_t frames = 0;
uint64_t start_time;

i2s_config_t i2s_config;
#define AUDIO_FREQ SV_SAMPLE_RATE

typedef struct __attribute__((__packed__)) {
    bool is_directory;
    bool is_executable;
    size_t size;
    char filename[79];
} file_item_t;

constexpr int max_files = 600;
file_item_t * fileItems = (file_item_t *)(&SCREEN[0][0]);

int compareFileItems(const void* a, const void* b) {
    const auto* itemA = (file_item_t *)a;
    const auto* itemB = (file_item_t *)b;
    // Directories come first
    if (itemA->is_directory && !itemB->is_directory)
        return -1;
    if (!itemA->is_directory && itemB->is_directory)
        return 1;
    // Sort files alphabetically
    return strcmp(itemA->filename, itemB->filename);
}

bool isExecutable(const char pathname[255],const char *extensions) {
    char *pathCopy = strdup(pathname);
    const char* token = strrchr(pathCopy, '.');

    if (token == nullptr) {
        return false;
    }

    token++;

    while (token != NULL) {
        if (strstr(extensions, token) != NULL) {
            free(pathCopy);
            return true;
        }
        token = strtok(NULL, ",");
    }
    free(pathCopy);
    return false;
}

bool filebrowser_loadfile(const char pathname[256]) {
    UINT bytes_read = 0;
    FIL file;

    constexpr int window_y = (TEXTMODE_ROWS - 5) / 2;
    constexpr int window_x = (TEXTMODE_COLS - 43) / 2;

    draw_window("Loading ROM", window_x, window_y, 43, 5);

    FILINFO fileinfo;
    f_stat(pathname, &fileinfo);
    rom_size = fileinfo.fsize;
    if (16384 - 64 << 10 < fileinfo.fsize) {
        draw_text("ERROR: ROM too large! Canceled!!", window_x + 1, window_y + 2, 13, 1);
        sleep_ms(5000);
        return false;
    }


    draw_text("Loading...", window_x + 1, window_y + 2, 10, 1);
    sleep_ms(500);


    multicore_lockout_start_blocking();
    auto flash_target_offset = FLASH_TARGET_OFFSET;
    const uint32_t ints = save_and_disable_interrupts();
    size_t count = fileinfo.fsize;
    count += 4096;
    count &= ~4095;
    flash_range_erase(flash_target_offset, count);
    restore_interrupts(ints);

    if (FR_OK == f_open(&file, pathname, FA_READ)) {
        uint8_t buffer[FLASH_PAGE_SIZE];

        do {
            f_read(&file, &buffer, FLASH_PAGE_SIZE, &bytes_read);

            if (bytes_read) {
                const uint32_t ints = save_and_disable_interrupts();
                flash_range_program(flash_target_offset, buffer, FLASH_PAGE_SIZE);
                restore_interrupts(ints);

                gpio_put(PICO_DEFAULT_LED_PIN, flash_target_offset >> 13 & 1);

                flash_target_offset += FLASH_PAGE_SIZE;
            }
        }
        while (bytes_read != 0);

        gpio_put(PICO_DEFAULT_LED_PIN, true);
    }
    f_close(&file);
    multicore_lockout_end_blocking();
    // restore_interrupts(ints);

    strcpy(filename, fileinfo.fname);

    return true;
}

void filebrowser(const char pathname[256], const char executables[11]) {
    bool debounce = true;
    char basepath[256];
    char tmp[TEXTMODE_COLS + 1];
    strcpy(basepath, pathname);
    constexpr int per_page = TEXTMODE_ROWS - 3;

    update_palette();

    DIR dir;
    FILINFO fileInfo;

    if (FR_OK != f_mount(&fs, "SD", 1)) {
        draw_text("SD Card not inserted or SD Card error!", 0, 0, 12, 0);
        while (true);
    }

    while (true) {
        memset(fileItems, 0, sizeof(file_item_t) * max_files);
        int total_files = 0;

        snprintf(tmp, TEXTMODE_COLS, "SD:\\%s", basepath);
        draw_window(tmp, 0, 0, TEXTMODE_COLS, TEXTMODE_ROWS - 1);
        memset(tmp, ' ', TEXTMODE_COLS);


        draw_text(tmp, 0, 29, 0, 0);
        auto off = 0;
        draw_text("START", off, 29, 7, 0);
        off += 5;
        draw_text(" Run at cursor ", off, 29, 0, 3);
        off += 16;
        draw_text("SELECT", off, 29, 7, 0);
        off += 6;
        draw_text(" Run previous  ", off, 29, 0, 3);
#ifndef TFT
        off += 16;
        draw_text("ARROWS", off, 29, 7, 0);
        off += 6;
        draw_text(" Navigation    ", off, 29, 0, 3);
        off += 16;
        draw_text("A/F10", off, 29, 7, 0);
        off += 5;
        draw_text(" USB DRV ", off, 29, 0, 3);
#endif

        if (FR_OK != f_opendir(&dir, basepath)) {
            draw_text("Failed to open directory", 1, 1, 4, 0);
            while (true);
        }

        if (strlen(basepath) > 0) {
            strcpy(fileItems[total_files].filename, "..\0");
            fileItems[total_files].is_directory = true;
            fileItems[total_files].size = 0;
            total_files++;
        }

        while (f_readdir(&dir, &fileInfo) == FR_OK &&
               fileInfo.fname[0] != '\0' &&
               total_files < max_files
        ) {
            // Set the file item properties
            fileItems[total_files].is_directory = fileInfo.fattrib & AM_DIR;
            fileItems[total_files].size = fileInfo.fsize;
            fileItems[total_files].is_executable = isExecutable(fileInfo.fname, executables);
            strncpy(fileItems[total_files].filename, fileInfo.fname, 78);
            total_files++;
        }
        f_closedir(&dir);

        qsort(fileItems, total_files, sizeof(file_item_t), compareFileItems);

        if (total_files > max_files) {
            draw_text(" Too many files!! ", TEXTMODE_COLS - 17, 0, 12, 3);
        }

        int offset = 0;
        int current_item = 0;

        while (true) {
            sleep_ms(100);

            if (!debounce) {
                debounce = !gamepad1.bits.start;
            }

            // ESCAPE
            if (gamepad1.bits.select) {
                return;
            }

            if (gamepad1.bits.down) {
                if (offset + (current_item + 1) < total_files) {
                    if (current_item + 1 < per_page) {
                        current_item++;
                    }
                    else {
                        offset++;
                    }
                }
            }

            if (gamepad1.bits.up) {
                if (current_item > 0) {
                    current_item--;
                }
                else if (offset > 0) {
                    offset--;
                }
            }

            if (gamepad1.bits.right) {
                offset += per_page;
                if (offset + (current_item + 1) > total_files) {
                    offset = total_files - (current_item + 1);
                }
            }

            if (gamepad1.bits.left) {
                if (offset > per_page) {
                    offset -= per_page;
                }
                else {
                    offset = 0;
                    current_item = 0;
                }
            }

            if (debounce && gamepad1.bits.start) {
                auto file_at_cursor = fileItems[offset + current_item];

                if (file_at_cursor.is_directory) {
                    if (strcmp(file_at_cursor.filename, "..") == 0) {
                        const char* lastBackslash = strrchr(basepath, '\\');
                        if (lastBackslash != nullptr) {
                            const size_t length = lastBackslash - basepath;
                            basepath[length] = '\0';
                        }
                    }
                    else {
                        sprintf(basepath, "%s\\%s", basepath, file_at_cursor.filename);
                    }
                    debounce = false;
                    break;
                }

                if (file_at_cursor.is_executable) {
                    sprintf(tmp, "%s\\%s", basepath, file_at_cursor.filename);

                    filebrowser_loadfile(tmp);
                    return;
                }
            }

            for (int i = 0; i < per_page; i++) {
                uint8_t color = 11;
                uint8_t bg_color = 1;

                if (offset + i < max_files) {
                    const auto item = fileItems[offset + i];


                    if (i == current_item) {
                        color = 0;
                        bg_color = 3;
                        memset(tmp, 0xCD, TEXTMODE_COLS - 2);
                        tmp[TEXTMODE_COLS - 2] = '\0';
                        draw_text(tmp, 1, per_page + 1, 11, 1);
                        snprintf(tmp, TEXTMODE_COLS - 2, " Size: %iKb, File %lu of %i ", item.size / 1024,
                                 offset + i + 1,
                                 total_files);
                        draw_text(tmp, 2, per_page + 1, 14, 3);
                    }

                    const auto len = strlen(item.filename);
                    color = item.is_directory ? 15 : color;
                    color = item.is_executable ? 10 : color;
                    //color = strstr((char *)rom_filename, item.filename) != nullptr ? 13 : color;

                    memset(tmp, ' ', TEXTMODE_COLS - 2);
                    tmp[TEXTMODE_COLS - 2] = '\0';
                    memcpy(&tmp, item.filename, len < TEXTMODE_COLS - 2 ? len : TEXTMODE_COLS - 2);
                }
                else {
                    memset(tmp, ' ', TEXTMODE_COLS - 2);
                }
                draw_text(tmp, 1, i + 1, color, bg_color);
            }
        }
    }
}

enum menu_type_e {
    NONE,
    INT,
    HEX,
    TEXT,
    ARRAY,

    SAVE,
    LOAD,
    ROM_SELECT,
    RETURN,
};

typedef bool (*menu_callback_t)();

typedef struct __attribute__((__packed__)) {
    const char* text;
    menu_type_e type;
    const void* value;
    menu_callback_t callback;
    uint32_t max_value;
    char value_list[45][20];
} MenuItem;

uint16_t frequencies[] = { 252, 362, 366, 378, 396, 404, 408, 412, 416, 420, 424, 432 };
#ifdef PICO_RP2040
uint8_t frequency_index = 3;
#else
uint8_t frequency_index = 0;
#endif

#ifndef PICO_RP2040
static void __not_in_flash_func(flash_timings)() {
        const int max_flash_freq = 88 * MHZ;
        const int clock_hz = frequencies[frequency_index] * MHZ;
        int divisor = (clock_hz + max_flash_freq - 1) / max_flash_freq;
        if (divisor == 1 && clock_hz > 100000000) {
            divisor = 2;
        }
        int rxdelay = divisor;
        if (clock_hz / divisor > 100000000) {
            rxdelay += 1;
        }
        qmi_hw->m[0].timing = 0x60007000 |
                            rxdelay << QMI_M0_TIMING_RXDELAY_LSB |
                            divisor << QMI_M0_TIMING_CLKDIV_LSB;
}
#endif

bool __not_in_flash_func(overclock)() {
#ifndef PICO_RP2040
    vreg_disable_voltage_limit();
    vreg_set_voltage(VREG_VOLTAGE_1_60);
    sleep_ms(33);
    flash_timings();
#else
    hw_set_bits(&vreg_and_chip_reset_hw->vreg, VREG_AND_CHIP_RESET_VREG_VSEL_BITS);
    sleep_ms(10);
#endif
    bool res = set_sys_clock_khz(frequencies[frequency_index] * KHZ, 0);
    if (res) {
        adjust_clk();
    }
    return res;
}

bool save() {
    char pathname[255];
    const size_t size = supervision_save_state_buf_size();
    auto * data = (uint8_t *)(malloc(size));

    if (settings.save_slot > 0) {
        sprintf(pathname, "%s\\%s_%d.save",  HOME_DIR, filename, settings.save_slot);
    }
    else {
        sprintf(pathname, "%s\\%s.save",  HOME_DIR, filename);
    }

    FRESULT fr = f_mount(&fs, "", 1);
    FIL fd;
    fr = f_open(&fd, pathname, FA_CREATE_ALWAYS | FA_WRITE);
    UINT bytes_writen;

    supervision_save_state_buf((uint8*)data, (uint32)size);
    f_write(&fd, data, size, &bytes_writen);
    f_close(&fd);
    free(data);
    return true;
}

bool load() {
    char pathname[255];
    const size_t size = supervision_save_state_buf_size();
    auto * data = (uint8_t *)(malloc(size));

    if (settings.save_slot > 0) {
        sprintf(pathname, "%s\\%s_%d.save",  HOME_DIR, filename, settings.save_slot);
    }
    else {
        sprintf(pathname, "%s\\%s.save",  HOME_DIR, filename);
    }

    FRESULT fr = f_mount(&fs, "", 1);
    FIL fd;
    fr = f_open(&fd, pathname, FA_READ);
    UINT bytes_read;

    f_read(&fd, data, size, &bytes_read);
    supervision_load_state_buf((uint8*)data, (uint32)size);
    f_close(&fd);

    free(data);
    return true;
}

void load_config() {
    FIL file;
    char pathname[256];
    sprintf(pathname, "%s\\emulator.cfg", HOME_DIR);
    if (FR_OK == f_mount(&fs, "", 1) && FR_OK == f_open(&file, pathname, FA_READ)) {
        UINT bytes_read;
        f_read(&file, &settings, sizeof(settings), &bytes_read);
        f_close(&file);
    }
    rgb0 = settings.rgb0;
    rgb1 = settings.rgb1;
    rgb2 = settings.rgb2;
    rgb3 = settings.rgb3;
    if (settings.ghosting > 6) settings.ghosting = 4;
}

void save_config() {
    FIL file;
    char pathname[256];
    sprintf(pathname, "%s\\emulator.cfg", HOME_DIR);

    if (FR_OK == f_mount(&fs, "", 1) && FR_OK == f_open(&file, pathname, FA_CREATE_ALWAYS | FA_WRITE)) {
        UINT bytes_writen;
        f_write(&file, &settings, sizeof(settings), &bytes_writen);
        f_close(&file);
    }
}
#if SOFTTV
typedef struct tv_out_mode_t {
    // double color_freq;
    float color_index;
    COLOR_FREQ_t c_freq;
    enum graphics_mode_t mode_bpp;
    g_out_TV_t tv_system;
    NUM_TV_LINES_t N_lines;
    bool cb_sync_PI_shift_lines;
    bool cb_sync_PI_shift_half_frame;
} tv_out_mode_t;
extern tv_out_mode_t tv_out_mode;

bool color_mode=true;
bool toggle_color() {
    color_mode=!color_mode;
    if(color_mode) {
        tv_out_mode.color_index= 1.0f;
    } else {
        tv_out_mode.color_index= 0.0f;
    }

    return true;
}
#endif
const MenuItem menu_items[] = {
        {"Swap AB <> BA: %s",     ARRAY, &settings.swap_ab,  nullptr, 1, {"NO ",       "YES"}},
        {},
        { "Ghosting pix: %i ", INT, &settings.ghosting, nullptr, 5 },
        { "Palette: %s ", ARRAY, &settings.palette, nullptr, SV_COLOR_SCHEME_COUNT, {
                  "DEFAULT          " // 0
                , "WATAROO          " // 1
                , "BGB              " // 2
                , "OCEAN SAND       "
                , "AUTUMN FOREST    "
                , "RED FOX          "
                , "MINT SAND        "
                , "AMBER            "
                , "GREEN            "
                , "BLUE             "
                , "GB_DMG           "
                , "GB_POCKET        "
                , "GB_LIGHT         "
                , "BLOSSOM_PINK     "
                , "BUBBLES_BLUE     "
                , "BUTTERCUP_GREEN  "
                , "DIGIVICE         "
                , "GAME_COM         "
                , "GAMEKING         "
                , "GAME_MASTER      "
                , "GOLDEN_WILD      "
                , "GREENSCALE       "
                , "HOKAGE_ORANGE    "
                , "LABO_FAWN        "
                , "SUPER_SAIYAN     "
                , "MICROVISION      "
                , "MILLION_LIVE_GOLD"
                , "ODYSSEY_GOLD     "
                , "SHINY_SKY_BLUE   "
                , "SLIME_BLUE       "
                , "TI_83            "
                , "TRAVEL_WOOD      "
                , "VIRTUAL_BOY      "
                , "TV-LINK          "
                , "CUSTOM           "
         }},
        { "RGB0: %06Xh ", HEX, &rgb0, nullptr, 0xFFFFFF },
        { "RGB1: %06Xh ", HEX, &rgb1, nullptr, 0xFFFFFF },
        { "RGB2: %06Xh ", HEX, &rgb2, nullptr, 0xFFFFFF },
        { "RGB3: %06Xh ", HEX, &rgb3, nullptr, 0xFFFFFF },
#if VGA
        { "Keep aspect ratio: %s",     ARRAY, &settings.aspect_ratio,  nullptr, 1, {"NO ",       "YES"}},
#endif
        { "Instant ignition simulation: %s",     ARRAY, &settings.instant_ignition,  nullptr, 1, {"NO ",       "YES"}},
#if SOFTTV
        { "" },
        { "TV system %s", ARRAY, &tv_out_mode.tv_system, nullptr, 1, { "PAL ", "NTSC" } },
        { "TV Lines %s", ARRAY, &tv_out_mode.N_lines, nullptr, 3, { "624", "625", "524", "525" } },
        { "Freq %s", ARRAY, &tv_out_mode.c_freq, nullptr, 1, { "3.579545", "4.433619" } },
        { "Colors: %s", ARRAY, &color_mode, &toggle_color, 1, { "NO ", "YES" } },
        { "Shift lines %s", ARRAY, &tv_out_mode.cb_sync_PI_shift_lines, nullptr, 1, { "NO ", "YES" } },
        { "Shift half frame %s", ARRAY, &tv_out_mode.cb_sync_PI_shift_half_frame, nullptr, 1, { "NO ", "YES" } },
#endif
    //{ "Player 1: %s",        ARRAY, &player_1_input, 2, { "Keyboard ", "Gamepad 1", "Gamepad 2" }},
    //{ "Player 2: %s",        ARRAY, &player_2_input, 2, { "Keyboard ", "Gamepad 1", "Gamepad 2" }},
    {},
    { "Save state: %i", INT, &settings.save_slot, &save, 5 },
    { "Load state: %i", INT, &settings.save_slot, &load, 5 },
{},
{
    "Overclocking: %s MHz", ARRAY, &frequency_index, &overclock, count_of(frequencies) - 1,
    { "252", "362", "366", "378", "396", "404", "408", "412", "416", "420", "424", "432" }
},
{ "Press START / Enter to apply", NONE },
    { "Reset to ROM select", ROM_SELECT },
    { "Return to game", RETURN }
};
#define MENU_ITEMS_NUMBER (sizeof(menu_items) / sizeof (MenuItem))

void menu() {
    bool exit = false;
    graphics_set_mode(TEXTMODE_DEFAULT);
    memset(TEXT_BUFFER, 0, sizeof(TEXT_BUFFER));
    char footer[TEXTMODE_COLS];
    snprintf(footer, TEXTMODE_COLS, ":: %s ::", PICO_PROGRAM_NAME);
    draw_text(footer, TEXTMODE_COLS / 2 - strlen(footer) / 2, 0, 11, 1);
    snprintf(footer, TEXTMODE_COLS, ":: %s build %s %s ::", PICO_PROGRAM_VERSION_STRING, __DATE__,
             __TIME__);
    draw_text(footer, TEXTMODE_COLS / 2 - strlen(footer) / 2, TEXTMODE_ROWS - 1, 11, 1);
    uint current_item = 0;
    int8_t hex_digit = -1;
    bool blink = false;

    while (!exit) {
        blink = !blink;
        bool hex_edit_mode = false;
        int8_t h_code = keyboard.h_code;
        for (int i = 0; i < MENU_ITEMS_NUMBER; i++) {
            uint8_t y = i + (TEXTMODE_ROWS - MENU_ITEMS_NUMBER >> 1);
            uint8_t x = TEXTMODE_COLS / 2 - 10;
            uint8_t color = 0xFF;
            uint8_t bg_color = 0x00;
            if (current_item == i) {
                color = 0x01;
                bg_color = 0xFF;
            }
            int pal = settings.palette;
            const MenuItem* item = &menu_items[i];
            if (i == current_item) {
                switch (item->type) {
                    case HEX:
                        if (item->max_value != 0 && SV_COLOR_SCHEME_COUNT <= settings.palette) {
                            uint32_t* value = (uint32_t *)item->value;
                            if (h_code >= 0) {
                                if (hex_digit < 0) hex_digit = 0;
                                uint32_t vc = *value;
                                vc &= ~(0xF << (5 - hex_digit) * 4);
                                vc |= ((uint32_t)h_code << (5 - hex_digit) * 4);
                                if (vc < item->max_value) *value = vc;
                                if (++hex_digit == 6) {
                                    h_code = -1;
                                    hex_digit = -1;
                                    keyboard.h_code = -1;
                                    current_item++;
                                }
                                settings.rgb0 = rgb0;
                                settings.rgb1 = rgb1;
                                settings.rgb2 = rgb2;
                                settings.rgb3 = rgb3;
                                update_palette();
                                sleep_ms(125);
                                break;
                            }
                            if (gamepad1.bits.right && hex_digit == 5) {
                                hex_digit = -1;
                            } else if (gamepad1.bits.right && hex_digit < 6) {
                                hex_digit++;
                            }
                            if (h_code != 0xA) { // W/A for 'A' pressed
                                if (gamepad1.bits.left && hex_digit == -1) {
                                    hex_digit = 5;
                                } else if (gamepad1.bits.left && hex_digit >= 0) {
                                    hex_digit--;
                                }
                            }
                            if (gamepad1.bits.up && hex_digit >= 0 && hex_digit <= 5) {
                                uint32_t vc = *value + (1 << (5 - hex_digit) * 4);
                                if (vc < item->max_value) *value = vc;
                            }
                            if (gamepad1.bits.down && hex_digit >= 0 && hex_digit <= 5) {
                                uint32_t vc = *value - (1 << (5 - hex_digit) * 4);
                                if (vc < item->max_value) *value = vc;
                            }
                        }
                        break;
                    case INT:
                    case ARRAY:
                        if (item->max_value != 0) {
                            auto* value = (uint8_t *)item->value;
                            if (gamepad1.bits.right && *value < item->max_value) {
                                (*value)++;
                            }
                            if (gamepad1.bits.left && *value > 0) {
                                (*value)--;
                            }
                        }
                        break;
                    case RETURN:
                        if (gamepad1.bits.start)
                            exit = true;
                        break;

                    case ROM_SELECT:
                        if (gamepad1.bits.start) {
                            reboot = true;
                            return;
                        }
                        break;
                    default:
                        break;
                }

                if (nullptr != item->callback && gamepad1.bits.start) {
                    exit = item->callback();
                }
            }
            if (pal != settings.palette) {
                update_palette();
            }
            static char result[TEXTMODE_COLS];
            switch (item->type) {
                case HEX:
                    snprintf(result, TEXTMODE_COLS, item->text, *(uint32_t*)item->value);
                    if (i == current_item && hex_digit >= 0 && hex_digit < 6) {
                        hex_edit_mode = true;
                        if (blink) {
                            result[hex_digit+6] = ' ';
                        }
                    }
                    break;
                case INT:
                    snprintf(result, TEXTMODE_COLS, item->text, *(uint8_t *)item->value);
                    break;
                case ARRAY:
                    snprintf(result, TEXTMODE_COLS, item->text, item->value_list[*(uint8_t *)item->value]);
                    break;
                case TEXT:
                    snprintf(result, TEXTMODE_COLS, item->text, item->value);
                    break;
                case NONE:
                    color = 6;
                default:
                    snprintf(result, TEXTMODE_COLS, "%s", item->text);
            }
            draw_text(result, x, y, color, bg_color);
        }

        if (gamepad1.bits.b || (gamepad1.bits.select && !gamepad1.bits.start))
            exit = true;

        if (gamepad1.bits.down && hex_digit < 0) {
            current_item = (current_item + 1) % MENU_ITEMS_NUMBER;
            if (menu_items[current_item].type == NONE)
                current_item++;
        }
        if (gamepad1.bits.up && hex_digit < 0) {
            current_item = (current_item - 1 + MENU_ITEMS_NUMBER) % MENU_ITEMS_NUMBER;
            if (menu_items[current_item].type == NONE)
                current_item--;
        }

        sleep_ms(125);
    }

#if VGA
    if (settings.aspect_ratio) {
        graphics_set_offset(40, 20);
        graphics_set_buffer((uint8_t *)SCREEN, 240, 200);
        graphics_set_mode(GRAPHICSMODE_ASPECT);
    } else {
        graphics_set_buffer((uint8_t *)SCREEN, SV_W, SV_H);
        graphics_set_offset(0, 0);
        graphics_set_mode(GRAPHICSMODE_DEFAULT);
    }
#else
    graphics_set_mode(GRAPHICSMODE_DEFAULT);
#endif
    if (count_of(palettes) <= settings.palette) {
        settings.rgb0 = rgb0;
        settings.rgb1 = rgb1;
        settings.rgb2 = rgb2;
        settings.rgb3 = rgb3;
    }
    save_config();
}

/* Renderer loop on Pico's second core */
void __time_critical_func(render_core)() {
    multicore_lockout_victim_init();

    tuh_init(BOARD_TUH_RHPORT);
    ps2kbd.init_gpio();
    nespad_begin(clock_get_hz(clk_sys) / 1000, NES_GPIO_CLK, NES_GPIO_DATA, NES_GPIO_LAT);

    graphics_init();

    const auto buffer = (uint8_t *)SCREEN;
    graphics_set_buffer(buffer, 240, 200);
    graphics_set_textbuffer(TEXT_BUFFER);
    graphics_set_bgcolor(0x000000);
#if VGA
    graphics_set_offset(0, 0);
#else
    graphics_set_offset(40, 20);
#endif
    graphics_set_flashmode(false, false);
    sem_acquire_blocking(&vga_start_semaphore);

    // 60 FPS loop
    #define frame_tick (16666)
    uint64_t tick = time_us_64();
    uint64_t last_frame_tick = tick;

    while (true) {

        if (tick >= last_frame_tick + frame_tick) {
#ifdef TFT
            refresh_lcd();
#endif
            ps2kbd.tick();
            gamepad1_update();
            supervision_set_input(gamepad1.state);

            last_frame_tick = tick;
        }

        tick = time_us_64();

        tuh_task();
        // hid_app_task();
        tight_loop_contents();
    }

    __unreachable();
}

int frame, frame_cnt = 0;
int frame_timer_start = 0;

static int audio_buffer[AUDIO_FREQ / 60] = { 0 };
static uint8_t buffer[AUDIO_BUFFER_SIZE] = { 0 };

int __time_critical_func(main)() {
    overclock();

    sem_init(&vga_start_semaphore, 0, 1);
    multicore_launch_core1(render_core);
    sem_release(&vga_start_semaphore);


    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    for (int i = 0; i < 6; i++) {
        sleep_ms(33);
        gpio_put(PICO_DEFAULT_LED_PIN, true);
        sleep_ms(33);
        gpio_put(PICO_DEFAULT_LED_PIN, false);
    }

    load_config();

    i2s_config = i2s_get_default_config();
    i2s_config.sample_freq = AUDIO_FREQ;
    i2s_config.dma_trans_count = AUDIO_FREQ / 60;
    i2s_volume(&i2s_config, 0);
    i2s_init(&i2s_config);

    supervision_init();
    update_palette();

    while (true) {

        graphics_set_mode(TEXTMODE_DEFAULT);
        filebrowser(HOME_DIR, "sv,bin");

        if (supervision_load((uint8_t *)rom, rom_size) ) {
            update_palette();
        }

#if VGA
        if (settings.aspect_ratio) {
            graphics_set_offset(40, 20);
            graphics_set_buffer((uint8_t *)SCREEN, 240, 200);
            graphics_set_mode(GRAPHICSMODE_ASPECT);
            uint8_t* screen = (uint8_t*)SCREEN;
            for (int i = 0; i < sizeof(bezel); ++i) {
                screen[i] = bezel[i] + base_bezel;
            }
        } else {
            graphics_set_buffer((uint8_t *)SCREEN, SV_W, SV_H);
            graphics_set_offset(0, 0);
            graphics_set_mode(GRAPHICSMODE_DEFAULT);
        }
#else
        settings.aspect_ratio = false;
        graphics_set_buffer((uint8_t *)SCREEN, 240, 200);
        graphics_set_mode(GRAPHICSMODE_DEFAULT);
        uint8_t* screen = (uint8_t*)SCREEN;
        for (int i = 0; i < sizeof(bezel); ++i) {
            screen[i] = bezel[i] + base_bezel;
        }
#endif

        start_time = time_us_64();

        while (true) {
            if (fxPressedV) {
                if (altPressed) {
                    settings.save_slot = fxPressedV;
                    load();
                } else if (ctrlPressed) {
                    settings.save_slot = fxPressedV;
                    save();
                }
            }
#if VGA
            if (settings.aspect_ratio) {
                uint32_t sw_w = 240;
                supervision_exec_ex((uint8_t *) SCREEN + sw_w * 20 + 40, sw_w, 0, settings.ghosting);
            } else {
                supervision_exec_ex((uint8_t *) SCREEN, SV_W, 0, settings.ghosting);
            }
#else
                uint32_t sw_w = 240;
                supervision_exec_ex((uint8_t *) SCREEN + sw_w * 20 + 40, sw_w, 0, settings.ghosting);
#endif
            // for(int x = 0; x <64; x++) graphics_set_palette(x, RGB888(bitmap.pal.color[x][0], bitmap.pal.color[x][1], bitmap.pal.color[x][2]));

            if (gamepad1.bits.start && gamepad1.bits.select) {
                menu();
                if (reboot) { /// не работало нормально, видимо ресурсы где-то текут, пара ребутов и в даун, сделал, чтобы весь чип перегружало
                    watchdog_enable(10, true);
                    while(true) {
                        tight_loop_contents();
                    }
                }
                if (settings.aspect_ratio) {
                    uint8_t* screen = (uint8_t*)SCREEN;
                    for (int i = 0; i < sizeof(bezel); ++i) {
                        screen[i] = bezel[i] + base_bezel;
                    }
                }
            }


            frame++;
            if (limit_fps) {

                frame_cnt++;
                if (frame_cnt == 6) {
                    while (time_us_64() - frame_timer_start < 16666 * 6);  // 60 Hz
                    frame_timer_start = time_us_64();
                    frame_cnt = 0;
                }
            }
            tight_loop_contents();
            sound_stream_update(buffer, AUDIO_BUFFER_SIZE);
            // process audio
            auto * ptr = (unsigned short *)audio_buffer;
            for (unsigned char i : buffer)
                *ptr++ = i << (8 + 1);

            i2s_dma_write(&i2s_config, (const int16_t *) audio_buffer);
        }

        supervision_reset();
        update_palette();
    }
    __unreachable();
}
