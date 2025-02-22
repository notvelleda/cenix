#pragma once

#include <stdio.h>
#include <stdint.h>

int read_u16(FILE *file, uint16_t *result);
int read_i64(FILE *file, int64_t *result);
uint16_t u16_to_ne(uint16_t number);
int64_t i64_to_ne(int64_t number);
uint16_t u16_to_le(uint16_t number);
int64_t i64_to_le(int64_t number);
