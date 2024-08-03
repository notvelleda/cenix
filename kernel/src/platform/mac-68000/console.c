#include "console.h"
#include "font.h"
#include "heap.h"
#include <stdint.h>
#include "string.h"

#define SCRN_BASE (*(void **) 0x0824)
#define SCREEN_ROW (*(uint16_t *) 0x0106)
#define J_SCRN_SIZE (*(uint32_t *) 0x0810)
#define MAIN_DEVICE (*(uint32_t *) 0x08a4)

static int console_x = 0;
static int console_y = 0;

static uint16_t screen_width;
static uint16_t screen_height;

static uint16_t console_height;

static uint16_t row_bytes;
static uint32_t video_memory_length;

static uint16_t bits_per_pixel;

static void (*putchar_fn)(char) = NULL;

static void mono_putchar(char c) {
    int column_offset = console_x & 3;
    uint8_t *dest = SCRN_BASE + console_y * FONT_HEIGHT * row_bytes + (console_x >> 2) * 3;
    const uint8_t *char_data = &font[(c - 32) * FONT_HEIGHT];

    for (int row = 0; row < FONT_HEIGHT; row ++, dest += row_bytes) {
        uint8_t data = ~*(char_data ++);

        switch (column_offset) {
        case 0:
            *(dest + 0) = (*(dest + 0) & ~0b11111100) | (data & 0b11111100);
            break;
        case 1:
            *(dest + 0) = (*(dest + 0) & ~0b00000011) | (data >> 6);
            *(dest + 1) = (*(dest + 1) & ~0b11110000) | ((data << 2) & 0b11110000);
            break;
        case 2:
            *(dest + 1) = (*(dest + 1) & ~0b00001111) | (data >> 4);
            *(dest + 2) = (*(dest + 2) & ~0b11000000) | ((data << 4) & 0b11000000);
            break;
        case 3:
            *(dest + 2) = (*(dest + 2) & ~0b00111111) | (data >> 2);
            break;
        }
    }
}

static void color_putchar_2bpp(char c) {
    int column_offset = console_x & 1;
    uint8_t *dest = SCRN_BASE + console_y * FONT_HEIGHT * row_bytes + (console_x >> 1) * 3;
    const uint8_t *char_data = &font[(c - 32) * FONT_HEIGHT];

    for (int row = 0; row < FONT_HEIGHT; row ++, dest += row_bytes) {
        uint8_t data = ~*(char_data ++);

        switch (column_offset) {
        case 0:
            *(dest + 0) =
                (data & 0b10000000) | ((data >> 1) & 0b01000000)
                | ((data >> 1) & 0b00100000) | ((data >> 2) & 0b00010000)
                | ((data >> 2) & 0b00001000) | ((data >> 3) & 0b00000100)
                | ((data >> 3) & 0b00000010) | ((data >> 4) & 0b00000001);
            *(dest + 1) =
                (*(dest + 1) & 0x0f)
                | ((data << 4) & 0b10000000) | ((data << 3) & 0b01000000)
                | ((data << 3) & 0b00100000) | ((data << 2) & 0b00010000);
            break;
        case 1:
            *(dest + 1) =
                (*(dest + 1) & 0xf0)
                | ((data >> 4) & 0b00001000) | ((data >> 5) & 0b00000100)
                | ((data >> 5) & 0b00000010) | ((data >> 6) & 0b00000001);
            *(dest + 2) =
                ((data << 2) & 0b10000000) | ((data << 1) & 0b01000000)
                | ((data << 1) & 0b00100000) | (data & 0b00010000)
                | (data & 0b00001000) | ((data >> 1) & 0b00000100)
                | ((data >> 1) & 0b00000010) | ((data >> 2) & 0b00000001);
            break;
        }
    }
}

static void color_putchar_4bpp(char c) {
    uint8_t *dest = SCRN_BASE + console_y * FONT_HEIGHT * row_bytes + console_x * FONT_WIDTH / 2;
    const uint8_t *char_data = &font[(c - 32) * FONT_HEIGHT];

    for (int row = 0; row < FONT_HEIGHT; row ++, dest += row_bytes) {
        uint8_t data = *(char_data ++);
        uint8_t *pixel = dest;

        for (int col = 7; col >= 2; col -= 2, pixel ++) {
            *pixel = (((data >> col) & 1) ? 0x00 : 0xf0) | (((data >> (col - 1)) & 1) ? 0x00 : 0x0f);
        }
    }
}

static void color_putchar_8bpp(char c) {
    uint8_t *dest = SCRN_BASE + console_y * FONT_HEIGHT * row_bytes + console_x * FONT_WIDTH;
    const uint8_t *char_data = &font[(c - 32) * FONT_HEIGHT];

    for (int row = 0; row < FONT_HEIGHT; row ++, dest += row_bytes) {
        uint8_t data = *(char_data ++);
        uint8_t *pixel = dest;

        for (int col = 7; col >= 2; col --, pixel ++) {
            *pixel = ((data >> col) & 1) ? 0x00 : 0xff;
        }
    }
}

static void color_putchar_16bpp(char c) {
    uint16_t *dest = (uint16_t *) (SCRN_BASE + console_y * FONT_HEIGHT * row_bytes + console_x * FONT_WIDTH * 2);
    const uint8_t *char_data = &font[(c - 32) * FONT_HEIGHT];

    for (int row = 0; row < FONT_HEIGHT; row ++, dest += row_bytes / 2) {
        uint8_t data = *(char_data ++);
        uint16_t *pixel = dest;

        for (int col = 7; col >= 2; col --, pixel ++) {
            *pixel = ((data >> col) & 1) ? 0x0000 : 0xffff;
        }
    }
}

static void color_putchar_32bpp(char c) {
    uint32_t *dest = (uint32_t *) (SCRN_BASE + console_y * FONT_HEIGHT * row_bytes + console_x * FONT_WIDTH * 4);
    const uint8_t *char_data = &font[(c - 32) * FONT_HEIGHT];

    for (int row = 0; row < FONT_HEIGHT; row ++, dest += row_bytes / 4) {
        uint8_t data = *(char_data ++);
        uint32_t *pixel = dest;

        for (int col = 7; col >= 2; col --, pixel ++) {
            *pixel = ((data >> col) & 1) ? 0x00000000 : 0xffffffff;
        }
    }
}

void init_console(uint16_t screen_width_from_bootloader, uint16_t screen_height_from_bootloader) {
    screen_width = screen_width_from_bootloader;
    screen_height = screen_height_from_bootloader;

    console_height = ((screen_height / FONT_HEIGHT) * FONT_HEIGHT); // console height has to be a multiple of the font height!;

    row_bytes = SCREEN_ROW;

    if (row_bytes == 0xffff) {
        row_bytes = screen_width / 8;
    }

    bits_per_pixel = (row_bytes * 8) / screen_width;

    video_memory_length = row_bytes * screen_height;

    switch (bits_per_pixel) {
    case 1:
        putchar_fn = &mono_putchar;
        break;
    case 2:
        putchar_fn = &color_putchar_2bpp;
        break;
    case 4:
        putchar_fn = &color_putchar_4bpp;
        break;
    case 8:
        putchar_fn = &color_putchar_8bpp;
        break;
    case 16:
        putchar_fn = &color_putchar_16bpp;
        break;
    case 32:
        putchar_fn = &color_putchar_32bpp;
        break;
    }

    printk(
        "screen is %dx%d (console is %dx%d), %d bit(s) per pixel at 0x%x\n",
        screen_width,
        screen_height,
        screen_width / FONT_WIDTH,
        screen_height / FONT_HEIGHT,
        bits_per_pixel,
        SCRN_BASE
    );
}

void lock_video_memory(struct heap *heap) {
    heap_lock_existing_region(heap, SCRN_BASE, (uint8_t *) SCRN_BASE + video_memory_length);
}

static void newline(void) {
    console_x = 0;
    console_y ++;

    if (console_y >= (console_height / FONT_HEIGHT)) {
        size_t move_distance = row_bytes * FONT_HEIGHT;

        memmove((uint8_t *) SCRN_BASE, (uint8_t *) SCRN_BASE + move_distance, video_memory_length - move_distance);
        memset((uint8_t *) SCRN_BASE + video_memory_length - move_distance, 0xff, move_distance);

        console_y = (console_height / FONT_HEIGHT) - 1;
    }
}

void _putchar(char c) {
    if (c == '\n') {
        newline();
        return;
    }

    if (putchar_fn != NULL) {
        putchar_fn(c);
    }
    console_x ++;

    if (console_x >= screen_width / FONT_WIDTH) {
        newline();
    }
}
