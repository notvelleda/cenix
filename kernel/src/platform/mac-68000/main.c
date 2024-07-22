#include "string.h"
#include "printf.h"
#include "debug.h"
#include "heap.h"
#include "capabilities.h"

#ifdef DEBUG
#include "font.h"
#endif

#define SCRN_BASE (*(void **) 0x0824)
#define SCRN_LEN 0x5580
#define SCRN_LEN_CONSOLE 0x5400

#define SOUND_BASE (*(void **) 0x0266)
#define SOUND_LEN 370

#define MEM_TOP (*(void **) 0x0108)

#define STACK_SIZE 4096

#define SCREEN_WIDTH 512
#define SCREEN_HEIGHT 342
#define CONSOLE_HEIGHT 336 // console height has to be a multiple of 8!

#ifdef DEBUG
static int console_x = 0;
static int console_y = 0;
#endif

struct heap the_heap = {NULL, 0, 0};

extern char _end;

void _start(void) {
    struct init_block init_block;

#ifdef DEBUG
    memset(SCRN_BASE, 0xff, SCRN_LEN);
#endif

    printk("Hellorld!\n");

    init_block.kernel_start = init_block.memory_start = (void *) 0;
    init_block.kernel_end = (void *) &_end;
    init_block.memory_end = MEM_TOP;

    heap_init(&the_heap, &init_block);

    // now that the heap is set up, allocate a region of memory for the stack so that nothing gets trampled when existing regions are locked
    void *stack_region = heap_alloc(&the_heap, STACK_SIZE);
    void *new_stack_pointer = stack_region + STACK_SIZE - 1;

    __asm__ __volatile__ (
        "movl %0, %%sp\n\t"
        "jsr after_sp_set"
        :: "r" (new_stack_pointer)
    );
}

void after_sp_set(void) {
    heap_lock_existing_region(&the_heap, SCRN_BASE, SCRN_BASE + SCRN_LEN);
    heap_lock_existing_region(&the_heap, SOUND_BASE, SOUND_BASE + SOUND_LEN);

    size_t offset = 0xfd00 - 0xa100;

    switch ((size_t) SOUND_BASE & 0xffff) {
    case 0xfd00:
        heap_lock_existing_region(&the_heap, SOUND_BASE - offset, SOUND_BASE - offset + SOUND_LEN);
        break;
    case 0xa100:
        heap_lock_existing_region(&the_heap, SOUND_BASE + offset, SOUND_BASE + offset + SOUND_LEN);
        break;
    default:
        printk("unexpected SoundBase of 0x%08x, alternate sound buffer will not be available\n", SOUND_BASE);
        break;
    }

    heap_list_blocks(&the_heap);

    printk(
        "total memory: %d KiB, used memory: %d KiB, free memory: %d KiB\n",
        the_heap.total_memory / 1024,
        the_heap.used_memory / 1024,
        (the_heap.total_memory - the_heap.used_memory) / 1024
    );

    /*void *ptr1 = heap_alloc(&the_heap, 1024);
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
    heap_list_blocks(&the_heap);*/

    init_root_capability(&the_heap);

    heap_list_blocks(&the_heap);

    while (1);
}

void _putchar(char c) {
#ifdef DEBUG
    char *index_byte = (char *) SCRN_BASE + (console_x >> 3) + (console_y << 6);
    short index_sub = 7 - (console_x & 0x7);
    char col, row;
    short font_index;

    font_index = (c - 0x20) * 5;
    if (c == '\n') {
        console_x = 0;
        console_y += 8;

        if (console_y >= CONSOLE_HEIGHT) {
            memmove(SCRN_BASE, SCRN_BASE + 512, SCRN_LEN_CONSOLE - 512);
            memset(SCRN_BASE + SCRN_LEN_CONSOLE - 512, 0xff, 512);
            console_y = CONSOLE_HEIGHT - 8;
        }

        index_byte = (char *) SCRN_BASE + (console_x >> 3) + (console_y << 6);
        index_sub = 7 - (console_x & 0x7);
    } else {
        if (c == ' ') {
            console_x += 6;
            index_byte = (char *) SCRN_BASE + (console_x >> 3) + (console_y << 6);
            index_sub = 7 - (console_x & 0x7);
            console_x -= 6;
        } else {
            for (col = 0; col < 5; col ++) {
                for (row = 0; row < 7; row ++) {
                    char pixel = (font[font_index] >> row) & 0x1;
                    if (pixel) {
                        *(index_byte + (row << 6)) &= ~(1 << index_sub);
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
                memmove(SCRN_BASE, SCRN_BASE + 512, SCRN_LEN_CONSOLE - 512);
                memset(SCRN_BASE + SCRN_LEN_CONSOLE - 512, 0xff, 512);
                console_y = CONSOLE_HEIGHT - 8;
            }

            index_byte = (char *) SCRN_BASE + (console_x >> 3) + (console_y << 6);
            index_sub = 7 - (console_x & 0x7);
        }
    }
#endif
}
