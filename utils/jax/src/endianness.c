#include "endianness.h"
#include <stdint.h>
#include <stdio.h>

int read_u16(FILE *file, uint16_t *result) {
    uint8_t bytes[2];

    if (fread(&bytes, 2, 1, file) != 1) {
        return -1;
    }

    *result = bytes[0] | (bytes[1] << 8);

    return 0;
}

int read_i64(FILE *file, int64_t *result) {
    uint8_t bytes[8];

    if (fread(&bytes, 8, 1, file) != 1) {
        return -1;
    }

    *result = 0;
    for (int i = 0; i < 8; i ++) {
        *result |= bytes[i] << (i * 8);
    }

    return 0;
}

uint16_t u16_to_ne(uint16_t number) {
    uint8_t *bytes = (uint8_t *) &number;

    return bytes[0] | (bytes[1] << 8);
}

int64_t i64_to_ne(int64_t number) {
    uint8_t *bytes = (uint8_t *) &number;

    int64_t result = 0;
    for (int i = 0; i < 8; i ++) {
        result |= bytes[i] << i * 8;
    }

    return result;
}

uint16_t u16_to_le(uint16_t number) {
    uint8_t bytes[2];

    bytes[0] = number & 0xff;
    bytes[1] = number >> 8;

    return *(uint16_t *) &bytes;
}

int64_t i64_to_le(int64_t number) {
    uint8_t bytes[8];

    for (int i = 0; i < 8; i ++) {
        bytes[i] = (number >> (i * 8)) & 0xff;
    }

    return *(int64_t *) &bytes;
}
