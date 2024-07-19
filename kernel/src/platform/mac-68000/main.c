#include "string.h"
#include "font.h"
#include "printf.h"
#include "debug.h"
#include "heap.h"

#define SCRN_BASE (*(unsigned long *) 0x0824)
#define SCRN_LEN 0x5580
#define SCRN_LEN_CONSOLE 0x5400

#define MEM_TOP (*(unsigned long *) 0x0108)

#define SCREEN_WIDTH 512
#define SCREEN_HEIGHT 342
#define CONSOLE_HEIGHT 336 // console height has to be a multiple of 8!

int console_x = 0;
int console_y = 0;

struct heap the_heap = {NULL, 0, 0};

extern char _end;

void _start() {
    struct init_block init_block;

#ifdef DEBUG
    memset((void *) SCRN_BASE, 0xff, SCRN_LEN);
#endif

    printk("Hellorld!\n");

    init_block.kernel_start = (void *) 0;
    init_block.kernel_end = init_block.memory_start = (void *) &_end;
    init_block.memory_end = (void *) MEM_TOP;

    heap_init(&the_heap, &init_block);

    heap_list_blocks(&the_heap);
    void *ptr1 = heap_alloc(&the_heap, 1024);
    if (ptr1 == NULL) {
        printk("allocation failed\n");
        while (1);
    }
    heap_list_blocks(&the_heap);
    void *ptr2 = heap_alloc(&the_heap, 1024);
    if (ptr2 == NULL) {
        printk("allocation failed\n");
        while (1);
    }
    heap_list_blocks(&the_heap);
    void *ptr3 = heap_alloc(&the_heap, 1024);
    if (ptr3 == NULL) {
        printk("allocation failed\n");
        while (1);
    }
    heap_list_blocks(&the_heap);
    heap_free(&the_heap, ptr2);
    printk("freed memory\n");
    heap_list_blocks(&the_heap);
    void *ptr4 = heap_alloc(&the_heap, 1016);
    heap_list_blocks(&the_heap);
    heap_free(&the_heap, ptr1);
    printk("freed memory\n");
    heap_list_blocks(&the_heap);
    heap_free(&the_heap, ptr4);
    printk("freed memory\n");
    heap_list_blocks(&the_heap);
    heap_free(&the_heap, ptr3);
    printk("freed memory\n");
    heap_list_blocks(&the_heap);

    while (1);
}

void _putchar(char c) {
#ifdef DEBUG
    int index_byte = SCRN_BASE + (console_x >> 3) + (console_y << 6);
    short index_sub = 7 - (console_x & 0x7);
    char col, row;
    short font_index;

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
#endif
}
