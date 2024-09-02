#pragma once

/// the header of a BFLT format binary
struct bflt_header {
    /// \brief the magic number
    ///
    /// this should equal "bFLT" in ASCII, or 0x62, 0x46, 0x4c, 0x54
    char magic[4];
    /// the version number of this file
    unsigned long version;
    /// the offset of the first executable instruction from the start of the file
    unsigned long entry_point;
    /// the offset of the start of the data segment from the beginning of the file
    unsigned long data_start;
    /// \brief the offset of the end of the data segment from the beginning of the file
    ///
    /// this offset also serves as the start of the bss segment
    unsigned long data_end;
    /// \brief the offset of the end of the bss segment from the beginning of the file
    ///
    /// the bss section isn't actually stored in the file,
    /// and as such this doesn't point to an actual location in it
    unsigned long bss_end;
    /// the size of the stack in bytes
    unsigned long stack_size;
    /// the offset of the start of the relocation entries from the beginning of the file
    unsigned long relocations_start;
    /// the number of relocation entries in this file
    unsigned long num_relocations;
    unsigned long flags;
    /// reserved for future (read: never) use
    unsigned long reserved[6];
};

/// indicates to the binary loader that this binary should be loaded entirely into RAM,
/// and therefore can't be executed in place
#define BFLT_FLAG_RAM 1

/// indicates to the binary loader that this binary is position-independent with a global offset table
#define BFLT_FLAG_GOTPIC 2

/// indicates to the binary loader that the rest of the file following the header is compressed in the Gzip format
#define BFLT_FLAG_GZIP 4

#include <stdbool.h>
#include "sys/kernel.h"

bool bflt_verify(const struct bflt_header *header);
size_t bflt_allocation_size(const struct bflt_header *header);
void bflt_load(struct bflt_header *header, void *allocation, struct thread_registers *registers);
