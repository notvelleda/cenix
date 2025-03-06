#include "jax.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "string.h"

bool open_jax(struct jax_iterator *iter, const uint8_t *start, const uint8_t *end) {
    if (start + 4 > end || start[0] != '^' || start[1] != 'j' || start[2] != 'a' || start[3] != 'x') {
        return false;
    }

    iter->start = start + 4;
    iter->end = end;

    return true;
}

uint16_t u16_to_ne(const uint8_t *bytes) {
    return bytes[0] | (bytes[1] << 8);
}

int64_t i64_to_ne(const uint8_t *bytes) {
    int64_t result = 0;
    for (int i = 0; i < 8; i ++) {
        result |= bytes[i] << i * 8;
    }

    return result;
}

bool jax_next_file(struct jax_iterator *iter, struct jax_file *file) {
    if (iter->start >= iter->end) {
        return false;
    }

    file->type = iter->start[0];
    file->name_length = u16_to_ne(iter->start + 1);
    file->name = (const char *) iter->start + 3;
    iter->start += file->name_length + 3;

    if (iter->start >= iter->end) {
        return false;
    }

    file->description_length = u16_to_ne(iter->start);
    file->description = (const char *) iter->start + 2;
    iter->start += file->description_length + 2;

    if (iter->start >= iter->end) {
        return false;
    }

    file->timestamp = i64_to_ne(iter->start);
    file->mode = u16_to_ne(iter->start + 8);
    file->owner = u16_to_ne(iter->start + 10);
    file->group = u16_to_ne(iter->start + 12);
    file->size = i64_to_ne(iter->start + 14);
    file->data = (const char *) iter->start + 22;
    iter->start += file->size + 22;

    return iter->start <= iter->end;
}

bool jax_find(struct jax_iterator *iter, const char *to_find, char type, const char **data, size_t *size) {
    struct jax_file file;

    if (*to_find == '/') {
        to_find ++;
    }

    while (jax_next_file(iter, &file)) {
        if (type != 0 && file.type != type) {
            continue;
        }

        int i = 0;
        for (; i < file.name_length && file.name[i] == to_find[i] && to_find[i] != 0; i ++);

        if (to_find[i] != 0) {
            continue;
        }

        *data = file.data;
        *size = (size_t) file.size;

        return true;
    }

    return false;
}
