#include "tar.h"
#include "printf.h"
#include "string.h"
#include "sys/kernel.h"

void open_tar(struct tar_iterator *iter, const char *start, const char *end) {
    iter->start = start;
    iter->end = end;
}

/* https://wiki.osdev.org/USTAR */
int oct2bin(unsigned char *str, int size) {
    int n = 0;
    unsigned char *c = str;
    while (size-- > 0) {
        n *= 8;
        n += *c - '0';
        c++;
    }
    return n;
}

bool tar_next_file(struct tar_iterator *iter, struct tar_header **header, const char **data, size_t *size) {
    if (iter->start >= iter->end)
        return false;

    struct tar_header *actual_header = (struct tar_header *) iter->start;
    actual_header->ustar_indicator[5] = 0;

    if (strcmp(actual_header->ustar_indicator, "ustar")) {
        return false;
    }

    *header = actual_header;
    *data = iter->start + 512;
    *size = oct2bin((unsigned char *) &actual_header->file_size, 11);

    iter->start += 512 + ((*size + 511) & ~511);

    return true;
}

void tar_get_name(struct tar_header *header, char *name_ptr) {
    char *c = header->name;
    for (int i = 0; i < 100 && *c; i ++) {
        *(name_ptr ++) = *(c ++);
    }

    c = header->filename_prefix;
    for (int i = 0; i < 155 && *c; i ++) {
        *(name_ptr ++) = *(c ++);
    }

    *name_ptr = 0;
}

bool tar_find(struct tar_iterator *iter, const char *to_find, char kind, const char **data, size_t *size) {
    struct tar_header *header;

    if (*to_find == '/') {
        to_find ++;
    }

    while (1) {
        if (!tar_next_file(iter, &header, data, size)) {
            break;
        }

        if (kind != 0 && header->kind != kind) {
            continue;
        }

        char name[257]; // for some reason this didn't work as static, maybe there's a bug in bflt loading?

        tar_get_name(header, name);
        const char *name_ptr = name;

        if (*name_ptr == '/') {
            name_ptr ++;
        }

        while (*name_ptr == '.' && *(name_ptr + 1) == '/')
            name_ptr += 2;

        const char *s = to_find;
        for (; *name_ptr == *s && *name_ptr != 0; name_ptr ++, s ++);

        if (*s != 0) {
            continue;
        }

        if (*name_ptr == 0) {
            return true;
        }

        // match directory names with trailing /s
        if ((kind == 0 || kind == TAR_DIRECTORY) && *name_ptr == '/') {
            name_ptr ++;

            if (*name_ptr == 0) {
                return true;
            }
        }

        /*if (!strcmp(name_ptr, to_find))
            return true;

        size_t len = strlen(name_ptr);
        if (len == strlen(to_find) + 1 && name_ptr[len - 1] == '/')
            return true;*/
    }

    return false;
}
