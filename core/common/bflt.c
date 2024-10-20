#include "bflt.h"
#include <stdbool.h>
#include "string.h"
#include "sys/kernel.h"

#ifdef DEBUG
#include "printf.h"
#else
#define printf(...)
#endif

bool bflt_verify(const struct bflt_header *header) {
    if (header->magic[0] != 'b' || header->magic[1] != 'F' || header->magic[2] != 'L' || header->magic[3] != 'T') {
        printf("bflt_verify: magic number is invalid!\n");
        return false;
    }

    if (header->version != 4) {
        printf("bflt_verify: binary has an unsupported version of %d\n", header->version);
        return false;
    }

    if ((header->flags & BFLT_FLAG_GZIP) != 0) {
        printf("bflt_verify: gzip compressed bFLT binaries are not supported!\n");
        return false;
    }

    return true;
}

size_t bflt_allocation_size(const struct bflt_header *header) {
    if ((header->flags & BFLT_FLAG_GOTPIC) != 0) {
        // if this binary has the GOTPIC flag set, its text segment doesn't have to be copied
        return header->bss_end - header->data_start + header->stack_size;
    } else {
        return header->bss_end - sizeof(struct bflt_header) + header->stack_size;
    }
}

void bflt_load(struct bflt_header *header, void *allocation, struct thread_registers *registers) {
    void *text_start;
    void *data_start;
    void *data_end;
    bool text_writable;

    if ((header->flags & BFLT_FLAG_GOTPIC) != 0) {
        text_start = (uint8_t *) header + sizeof(struct bflt_header);
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
        memcpy(data_start, (uint8_t *) header + header->data_start, header->data_end - header->data_start);
    } else {
        memcpy(text_start, (uint8_t *) header + sizeof(struct bflt_header), header->data_end - sizeof(struct bflt_header));
    }

    // zero out bss
    memset(data_end, 0, header->bss_end - header->data_end);

    printf("bflt_load: %d relocation(s) at offset %d\n", header->num_relocations, header->relocations_start);

    // apply all the relocations
    const uint32_t *relocations_start = (uint32_t *) ((uint8_t *) header + header->relocations_start);

    for (unsigned long i = 0; i < header->num_relocations; i ++) {
        uint32_t offset = *(relocations_start ++);

        if (offset < header->data_start - sizeof(struct bflt_header)) {
            // this offset is in the text segment
            if (text_writable) {
                uint32_t *address = (uint32_t *) ((uint8_t *) text_start + offset);
                *address = (uint32_t) address;
            } else {
                printf("bflt_load: skipping invalid relocation for offset 0x%x in read-only text segment\n", offset);
            }
        } else {
            // this offset is in either the data or bss segment, either way it's valid
            uint32_t *address = (uint32_t *) ((uint8_t *) data_start + offset - (header->data_start - sizeof(struct bflt_header)));
            *address = (uint32_t) address;
        }
    }

    if ((header->flags & BFLT_FLAG_GOTPIC) != 0) {
        uint32_t *got = (uint32_t *) data_start;
        printf("bflt_load: GOT is at address 0x%x\n", got);

        int i = 0;
        for (; *got != -1; got ++, i ++) {
            //uint32_t old = *got;
            if (*got < header->data_start - sizeof(struct bflt_header)) {
                // this offset is in the text segment
                *got += (uint32_t) text_start;
            } else {
                // this offset is in either the data or bss segment
                *got += (uint32_t) data_start - header->data_start - sizeof(struct bflt_header);
            }
            //printf("entry %d is 0x%08x from 0x%08x\n", i, *got, old);
        }

        printf("bflt_load: fixed up %d GOT entries\n", i);
    }

    size_t entry_point;

    if ((header->flags & BFLT_FLAG_GOTPIC) != 0) {
        entry_point = (size_t) header + header->entry_point;
    } else {
        entry_point = (size_t) allocation + header->entry_point - sizeof(struct bflt_header);
    }

    printf("bflt_load: entry point is 0x%x (offset %d)\n", entry_point, header->entry_point);
    set_program_counter(registers, entry_point);

    // TODO: is this ok on all platforms? will ones be supported without a full descending stack?
    set_stack_pointer(registers, (size_t) allocation + bflt_allocation_size(header));

    if ((header->flags & BFLT_FLAG_GOTPIC) != 0) {
        set_got_pointer(registers, (size_t) data_start);
    }
}
