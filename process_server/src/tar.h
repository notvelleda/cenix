#pragma once

#include <stdbool.h>
#include <stddef.h>

struct tar_header {
    char name[100];
    char mode[8];
    char owner_uid[8];
    char owner_gid[8];
    char file_size[12];
    char mod_time[12];
    char checksum[8];
    char kind;
    char link_name[100];
    char ustar_indicator[6];
    char ustar_version[2];
    char owner_user_name[32];
    char owner_group_name[32];
    char device_major[8];
    char device_minor[8];
    char filename_prefix[155];
};

#define TAR_NORMAL_FILE '0'
#define TAR_HARD_LINK '1'
#define TAR_SYM_LINK '2'
#define TAR_CHAR_DEVICE '3'
#define TAR_BLOCK_DEVICE '4'
#define TAR_DIRECTORY '5'
#define TAR_NAMED_PIPE '6'

struct tar_iterator {
    const char *start;
    const char *end;
};

int oct2bin(unsigned char *str, int size);
void open_tar(struct tar_iterator *iter, const char *start, const char *end);
bool tar_next_file(struct tar_iterator *iter, struct tar_header **header, const char **data, size_t *size);
const char *tar_get_name(struct tar_header *header);
bool tar_find(struct tar_iterator *iter, const char *to_find, char kind, const char **data, size_t *size);
