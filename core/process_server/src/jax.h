#pragma once

// jax parser implementing the spec described in https://wiki.restless.systems/wiki/Jax_Archive

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct jax_iterator {
    const uint8_t *start;
    const uint8_t *end;
};

bool open_jax(struct jax_iterator *iter, const uint8_t *start, const uint8_t *end);

#define TYPE_DIRECTORY 'd'
#define TYPE_LINK 's'
#define TYPE_REGULAR 'r'

struct jax_file {
    /// the type of this file (regular, directory, link)
    char type;
    /// the length of the name of this file
    uint16_t name_length;
    /// the name of this file. NOT NULL TERMINATED
    const char *name;
    /// the length of the description of this file
    uint16_t description_length;
    /// the description of this file. NOT NULL TERMINATED
    const char *description;
    /// the timestamp of this file
    int64_t timestamp;
    /// the mode (permissions bit set) of this file
    uint16_t mode;
    /// the user ID of the owner of this file
    uint16_t owner;
    /// the group ID of this file
    uint16_t group;
    /// the size of this file
    int64_t size;
    /// the data of this file
    const char *data;
};

bool jax_next_file(struct jax_iterator *iter, struct jax_file *file);
bool jax_find(struct jax_iterator *iter, const char *to_find, char type, const char **data, size_t *size);
