#include "string.h"
#include "font.h"

#define SCRN_BASE (*(unsigned long *) 0x0824)
#define SCRN_LEN 0x5580
#define SCRN_LEN_CONSOLE 0x5400

#define SCREEN_WIDTH 512
#define SCREEN_HEIGHT 342
#define CONSOLE_HEIGHT 336 // console height has to be a multiple of 8!

void draw_string(const char *str, short x, short y);
void draw_char(char c, short x, short y);
void draw_num(unsigned int num, short x, short y);
void draw_string_fast(const char *str, short x, short y);
void draw_num_fast(unsigned int num, short x, short y);

void print(const char *str);
void println(const char *str);
void print_num(unsigned int num);
void clear();

int console_x = 0;
int console_y = 0;

void _start() {
    clear();
    println("Hellorld!\n");

    while (1);
}

#define SET_PIXEL(x, y) *((char *) SCRN_BASE + ((x) >> 3) + ((y) << 6)) &= ~(1 << (7 - ((x) & 0x7)))
#define CLEAR_PIXEL(x, y) *((char *) SCRN_BASE + ((x) >> 3) + ((y) << 6)) |= 1 << (7 - ((x) & 0x7))

void draw_string(const char *str, short x, short y) {
    short ptr = 0;
    char c;

    while ((c = str[ptr ++]) != 0) {
        draw_char(c, x, y);
        x += 6;
        if (x >= SCREEN_WIDTH - 6) {
            x = 0;
            y += 8;
        }
    }
}

void draw_char(char c, short x, short y) { // slow but functional!
    short index = (c - 0x20) * 5;
    char col, row;
    for (col = 0; col < 5; col ++) {
        short workingRow = y;
        for (row = 0; row < 7; row ++) {
            char pixel = (font[index] >> row) & 0x1;
            if (pixel) {
                SET_PIXEL(x, workingRow);
            } else {
                CLEAR_PIXEL(x, workingRow);
            }
            workingRow ++;
        }
        x ++;
        index ++;
    }
}

void draw_string_fast(const char *str, short x, short y) {
    short ptr = 0;
    int index_byte = SCRN_BASE + (x >> 3) + (y << 6);
    short index_sub = 7 - (x & 0x7);
    char c, col, row;
    short font_index;

    while ((c = str[ptr ++]) != 0) {
        font_index = (c - 0x20) * 5;
        for (col = 0; col < 5; col ++) {
            for (row = 0; row < 7; row ++) {
                char pixel = (font[font_index] >> row) & 0x1;
                if (pixel) {
                    *((char *) index_byte + (row << 6)) &= ~(1 << index_sub);
                }
                // don't bother clearing pixels, prolly doesnt matter
            }

            if (-- index_sub < 0) {
                index_sub = 7;
                index_byte ++;
            }
            font_index ++;
        }

        if (-- index_sub < 0) {
            index_sub = 7;
            index_byte ++;
        }

        x += 6;
        if (x >= SCREEN_WIDTH - 6) {
            x = 0;
            y += 8;

            index_byte = SCRN_BASE + (x >> 3) + (y << 6);
            index_sub = 7 - (x & 0x7);
        }
    }
}

void print(const char *str) {
    short ptr = 0;
    int index_byte = SCRN_BASE + (console_x >> 3) + (console_y << 6);
    short index_sub = 7 - (console_x & 0x7);
    char c, col, row;
    short font_index;

    while ((c = str[ptr ++]) != 0) {
        font_index = (c - 0x20) * 5;
        if (c == '\n') {
            console_x = 0;
            console_y += 8;

            if (console_y >= CONSOLE_HEIGHT) {
                memmove((void *) SCRN_BASE, (void *) SCRN_BASE + 512, SCRN_LEN_CONSOLE - 512);
                memset((void *) SCRN_BASE + SCRN_LEN_CONSOLE - 512, 0xff, 512);
                console_y = CONSOLE_HEIGHT - 8;
            }

            index_byte = SCRN_BASE + (console_x >> 3) + (console_y << 6);
            index_sub = 7 - (console_x & 0x7);
        } else {
            if (c == ' ') {
                console_x += 6;
                index_byte = SCRN_BASE + (console_x >> 3) + (console_y << 6);
                index_sub = 7 - (console_x & 0x7);
                console_x -= 6;
            } else {
                for (col = 0; col < 5; col ++) {
                    for (row = 0; row < 7; row ++) {
                        char pixel = (font[font_index] >> row) & 0x1;
                        if (pixel) {
                            *((char *) index_byte + (row << 6)) &= ~(1 << index_sub);
                        }
                        // don't bother clearing pixels, prolly doesnt matter
                    }

                    if (-- index_sub < 0) {
                        index_sub = 7;
                        index_byte ++;
                    }
                    font_index ++;
                }

                if (-- index_sub < 0) {
                    index_sub = 7;
                    index_byte ++;
                }
            }

            console_x += 6;
            if (console_x >= SCREEN_WIDTH - 6) {
                console_x = 0;
                console_y += 8;

                if (console_y >= CONSOLE_HEIGHT) {
                    memmove((void *) SCRN_BASE, (void *) SCRN_BASE + 512, SCRN_LEN_CONSOLE - 512);
                    memset((void *) SCRN_BASE + SCRN_LEN_CONSOLE - 512, 0xff, 512);
                    console_y = CONSOLE_HEIGHT - 8;
                }

                index_byte = SCRN_BASE + (console_x >> 3) + (console_y << 6);
                index_sub = 7 - (console_x & 0x7);
            }
        }
    }
}

void print_num(unsigned int num) {
    int i;
    char num_buf[9];
    for (i = 7; i >= 0; i --) {
        num_buf[i] = "0123456789abcdef"[num & 0xf];
        num >>= 4;
    }
    num_buf[8] = 0;
    print(num_buf);
}

void draw_num(unsigned int num, short x, short y) {
    int i;
    char num_buf[9];
    for (i = 7; i >= 0; i --) {
        num_buf[i] = "0123456789abcdef"[num & 0xf];
        num >>= 4;
    }
    num_buf[8] = 0;
    draw_string(num_buf, x, y);
}

void draw_num_fast(unsigned int num, short x, short y) {
    int i;
    char num_buf[9];
    for (i = 7; i >= 0; i --) {
        num_buf[i] = "0123456789abcdef"[num & 0xf];
        num >>= 4;
    }
    num_buf[8] = 0;
    draw_string_fast(num_buf, x, y);
}

void println(const char *str) {
    print(str);
    console_x = 0;
    console_y += 8;

    if (console_y >= CONSOLE_HEIGHT) {
        memmove((void *) SCRN_BASE, (void *) SCRN_BASE + 512, SCRN_LEN_CONSOLE - 512);
        memset((void *) SCRN_BASE + SCRN_LEN_CONSOLE - 512, 0xff, 512);
        console_y = CONSOLE_HEIGHT - 8;
    }
}

void clear() {
    memset((void *) SCRN_BASE, 0xff, SCRN_LEN);
    console_x = 0;
    console_y = 0;
}
