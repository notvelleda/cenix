#include "arch.h"
#include "arch/68000/interrupts.h"
#include "arch/68000/user_mode_entry.h"
#include "capabilities.h"
#include "debug.h"
#include "heap.h"
#include "main.h"
#include "platform/mac-68000/hw.h"
#include "printf.h"
#include "scheduler.h"
#include "string.h"
#include "sys/kernel.h"
#include "threads.h"

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

#define TOOL_DISP_TABLE ((uint32_t *) 0x0c00)

#ifdef DEBUG
static int console_x = 0;
static int console_y = 0;
#endif

struct heap the_heap = {NULL, 0, 0};

extern char _end;

static uint8_t read_addr(uint8_t value) {
    return value;
}

void *new_stack_pointer;

void _start(void) {
    struct init_block init_block;

#ifdef DEBUG
    memset(SCRN_BASE, 0xff, SCRN_LEN);
#endif

    init_vector_table();

    MAC_VIA_IER = 0x7f; // disable all VIA interrupts
    MAC_VIA_IFR = 0xff; // clear VIA interrupt flag to dismiss any pending interrupts

    // reset the SCC. ideally it would be enough to just disable interrupts, however if any interrupts are triggered before this
    // we can't clear them, and since the SCC is a black box with horrible documentation it's easiest to just reset it, which will
    // disable interrupts and clear any pending interrupts
    read_addr(MAC_SCC_B_CTL_RD); // reset to register 0
    asm volatile ("nop");
    MAC_SCC_B_CTL_WR = 9; // select register 9
    asm volatile ("nop");
    MAC_SCC_B_CTL_WR = 0xc0; // set d7 and d6, causing the scc to reset

    // hardware should be sane enough to enable interrupts now!
    asm volatile ("andiw #0xf8ff, %sr");

    init_block.kernel_start = init_block.memory_start = 0;
    init_block.kernel_end = &_end;
    init_block.memory_end = MEM_TOP;

    heap_init(&the_heap, &init_block);

    // now that the heap is set up, allocate a region of memory for the stack so that nothing gets trampled when existing regions are locked
    void *stack_region = heap_alloc(&the_heap, STACK_SIZE);
    new_stack_pointer = (uint8_t *) stack_region + STACK_SIZE;
    printk("moved stack to 0x%x (base 0x%x)\n", new_stack_pointer, stack_region);

    __asm__ __volatile__ (
        "movel %0, %%sp\n\t"
        "jmp after_sp_set"
        :: "r" (new_stack_pointer)
    );
}

void after_sp_set(void) {
    heap_lock_existing_region(&the_heap, SCRN_BASE, (uint8_t *) SCRN_BASE + SCRN_LEN);
    heap_lock_existing_region(&the_heap, SOUND_BASE, (uint8_t *) SOUND_BASE + SOUND_LEN);

    size_t offset = 0xfd00 - 0xa100;

    switch ((size_t) SOUND_BASE & 0xffff) {
    case 0xfd00:
        heap_lock_existing_region(&the_heap, (uint8_t *) SOUND_BASE - offset, (uint8_t *) SOUND_BASE - offset + SOUND_LEN);
        break;
    case 0xa100:
        heap_lock_existing_region(&the_heap, (uint8_t *) SOUND_BASE + offset, (uint8_t *) SOUND_BASE + offset + SOUND_LEN);
        break;
    default:
        printk("unexpected SoundBase of 0x%08x, alternate sound buffer will not be available\n", SOUND_BASE);
        break;
    }

    /*uint16_t trap = 0xa893;
    uint32_t table_entry = *(TOOL_DISP_TABLE + (trap & 0x1ff));
    __asm__ __volatile__ ("jsr (%0)" :: "a" (table_entry));*/

    main_init(&the_heap);
    enter_user_mode(new_stack_pointer);
}

void _putchar(char c) {
#ifdef DEBUG
    if (c == '\n') {
        console_x = 0;
        console_y += 8;

        if (console_y >= CONSOLE_HEIGHT) {
            memmove((uint8_t *) SCRN_BASE, (uint8_t *) SCRN_BASE + 512, SCRN_LEN_CONSOLE - 512);
            memset((uint8_t *) SCRN_BASE + SCRN_LEN_CONSOLE - 512, 0xff, 512);
            console_y = CONSOLE_HEIGHT - 8;
        }

        return;
    }

    char *index_byte = (char *) SCRN_BASE + (console_x >> 3) + (console_y << 6);
    short index_sub = 7 - (console_x & 0x7);

    if (c != ' ') {
        short font_index = (c - 0x20) * 5;

        for (char col = 0; col < 5; col ++) {
            for (char row = 0; row < 7; row ++) {
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
    }

    console_x += 6;

    if (console_x >= SCREEN_WIDTH - 6) {
        console_x = 0;
        console_y += 8;

        if (console_y >= CONSOLE_HEIGHT) {
            memmove((uint8_t *) SCRN_BASE, (uint8_t *) SCRN_BASE + 512, SCRN_LEN_CONSOLE - 512);
            memset((uint8_t *) SCRN_BASE + SCRN_LEN_CONSOLE - 512, 0xff, 512);
            console_y = CONSOLE_HEIGHT - 8;
        }
    }
#endif
}
