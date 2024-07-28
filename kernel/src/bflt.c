#include "bflt.h"
#include "debug.h"
#include "heap.h"
#include <stdbool.h>
#include "string.h"
#include "sys/kernel.h"

bool bflt_load(struct heap *heap, void *binary_start, void *binary_end, struct thread_registers *registers) {
    size_t init_size = (size_t) binary_end - (size_t) binary_start;

    printk("bflt_load: init binary is at 0x%x to 0x%x, size %d\n", binary_start, binary_end, init_size);

    struct bflt_header *header = (struct bflt_header *) binary_start;

    if (header->magic[0] != 'b' || header->magic[1] != 'F' || header->magic[2] != 'L' || header->magic[3] != 'T') {
        printk("bflt_load: init's magic number is invalid!\n");
        return false;
    }

    if (header->version != 4) {
        printk("bflt_load: init binary has an unsupported version of %d\n", header->version);
        return false;
    }

    if ((header->flags & BFLT_FLAG_GZIP) != 0) {
        printk("bflt_load: gzip compressed bFLT binaries are not supported!\n");
        return false;
    }

    size_t allocation_size;

    if ((header->flags & BFLT_FLAG_GOTPIC) != 0) {
        // if this binary has the GOTPIC flag set, its text segment doesn't have to be copied
        allocation_size = header->bss_end - header->data_start + header->stack_size;
    } else {
        allocation_size = header->bss_end - sizeof(struct bflt_header) + header->stack_size;
    }

    printk("bflt_load: allocation size for init is %d\n", allocation_size);

    void *allocation = heap_alloc(heap, allocation_size);

    if (allocation == NULL) {
        printk("bflt_load: couldn't allocate memory for init's data and/or code\n");
        return false;
    }

    printk("bflt_load: init's allocated data is at 0x%x to 0x%x\n", allocation, (size_t) allocation + allocation_size);

    void *text_start;
    void *data_start;
    void *data_end;
    bool text_writable;

    if ((header->flags & BFLT_FLAG_GOTPIC) != 0) {
        text_start = (uint8_t *) binary_start + sizeof(struct bflt_header);
        data_start = allocation;
        data_end = (uint8_t *) allocation + header->data_end - header->data_start;
        text_writable = false;
    } else {
        text_start = allocation;
        data_start = (uint8_t *) allocation + header->data_start - sizeof(struct bflt_header);
        data_end = (uint8_t *) allocation + header->data_end - sizeof(struct bflt_header);
        text_writable = true;
    }

    // copy text and data
    if ((header->flags & BFLT_FLAG_GOTPIC) != 0) {
        memcpy(data_start, (uint8_t *) binary_start + header->data_start, header->data_end - header->data_start);
    } else {
        memcpy(text_start, (uint8_t *) binary_start + sizeof(struct bflt_header), header->data_end - sizeof(struct bflt_header));
    }

    // zero out bss
    memset(data_end, 0, header->bss_end - header->data_end);

    printk("bflt_load: init has %d relocation(s) at offset %d\n", header->num_relocations, header->relocations_start);

    // apply all the relocations
    const uint32_t *relocations_start = (uint32_t *) ((uint8_t *) binary_start + header->relocations_start);

    for (unsigned long i = 0; i < header->num_relocations; i ++) {
        uint32_t offset = *(relocations_start ++);

        if (offset < header->data_start - sizeof(struct bflt_header)) {
            // this offset is in the text segment
            if (text_writable) {
                uint32_t *address = (uint32_t *) ((uint8_t *) text_start + offset);
                *address = (uint32_t) address;
            } else {
                printk("bflt_load: skipping invalid relocation for offset 0x%x in read-only text segment\n", offset);
            }
        } else {
            // this offset is in either the data or bss segment, either way it's valid
            uint32_t *address = (uint32_t *) ((uint8_t *) data_start + offset - (header->data_start - sizeof(struct bflt_header)));
            *address = (uint32_t) address;
        }
    }

    if ((header->flags & BFLT_FLAG_GOTPIC) != 0) {
        uint32_t *got = (uint32_t *) data_start;
        printk("bflt_load: GOT is at address 0x%x\n", got);

        int i = 0;
        for (; *got != -1; got ++, i ++) {
            if (*got < header->data_start - sizeof(struct bflt_header)) {
                // this offset is in the text segment
                *got += (uint32_t) text_start;
            } else {
                // this offset is in either the data or bss segment
                *got += (uint32_t) data_start - header->data_start - sizeof(struct bflt_header);
            }
        }

        printk("bflt_load: fixed up %d GOT entries\n", i);
    }

    if ((header->flags & BFLT_FLAG_GOTPIC) != 0) {
        set_program_counter(registers, (size_t) binary_start + header->entry_point);
    } else {
        set_program_counter(registers, (size_t) allocation + header->entry_point - sizeof(struct bflt_header));
    }

    // TODO: is this ok on all platforms? will ones be supported without a full descending stack?
    set_stack_pointer(registers, (size_t) allocation + allocation_size);

    if ((header->flags & BFLT_FLAG_GOTPIC) != 0) {
        set_got_pointer(registers, (size_t) data_start);
    }

    return true;
}
