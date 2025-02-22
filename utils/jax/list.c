#include "endianness.h"
#include "jax.h"
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

int list(const char *program_name, FILE *archive) {
    char buffer[5] = {};

    if (fread(buffer, 4, 1, archive) != 1 || strcmp(buffer, "^jax") != 0) {
        fprintf(stderr, "%s: incorrect magic number, cannot continue\n", program_name);
        return 1;
    }

    while (1) {
        char record_type;

        if (fread(&record_type, 1, 1, archive) != 1) {
            break;
        }

        uint16_t name_length;

        if (read_u16(archive, &name_length) != 0) {
read_error:
            fprintf(stderr, "%s: malformed record, cannot continue\n", program_name);
            return 1;
        }

        // TODO: make this faster with block sized reads/writes
        char c;

        for (int i = 0; i < name_length; i ++) {
            if (fread(&c, 1, 1, archive) != 1) {
                goto read_error;
            }

            if (fwrite(&c, 1, 1, stdout) != 1) {
                return 1;
            }
        }

        if (record_type == TYPE_DIRECTORY) {
            c = '/';
            if (fwrite(&c, 1, 1, stdout) != 1) {
                return 1;
            }
        }

        c = '\n';
        if (fwrite(&c, 1, 1, stdout) != 1 || fflush(stdout) == EOF) {
            return 1;
        }

        uint16_t description_length;

        if (read_u16(archive, &description_length)) {
            goto read_error;
        }

        // seek past remaining fields
        fseek(archive, description_length + 14, SEEK_CUR);

        int64_t file_size;

        if (read_i64(archive, &file_size) != 0) {
            goto read_error;
        }

        for (; file_size >= 0; file_size -= LONG_MAX) {
            fseek(archive, file_size > LONG_MAX ? LONG_MAX : file_size, SEEK_CUR);
        }
    }

    return 0;
}

static int num_digits(int number) {
    int offset = number < 0 ? 1 : 0;
    if (number < 0) number = -number;

    if (number < 10) return offset + 1;
    else if (number < 100) return offset + 2;
    else if (number < 1000) return offset + 3;
    else if (number < 10000) return offset + 4;
    else if (number < 100000) return offset + 5;
    else if (number < 1000000) return offset + 6;
    else if (number < 10000000) return offset + 7;
    else return offset + 8;
}

int list_verbose(const char *program_name, FILE *archive) {
    char buffer[5] = {};

    if (fread(buffer, 4, 1, archive) != 1 || strcmp(buffer, "^jax") != 0) {
        fprintf(stderr, "%s: incorrect magic number, cannot continue\n", program_name);
        return 1;
    }

    while (1) {
        char record_type;

        if (fread(&record_type, 1, 1, archive) != 1) {
            break;
        }

        uint16_t name_length;

        if (read_u16(archive, &name_length) != 0) {
read_error:
            fprintf(stderr, "%s: malformed record, cannot continue\n", program_name);
            return 1;
        }

        // save name position so that it can be returned to later
        long name_position = ftell(archive);
        fseek(archive, name_length, SEEK_CUR);

        uint16_t description_length;

        if (read_u16(archive, &description_length)) {
            goto read_error;
        }

        fseek(archive, description_length, SEEK_CUR);

        struct {
            int64_t timestamp;
            uint16_t mode;
            uint16_t owner;
            uint16_t group;
            int64_t file_size;
        } __attribute__((__packed__)) other_fields;

        if (fread(&other_fields, sizeof(other_fields), 1, archive) != 1) {
            goto read_error;
        }

        other_fields.timestamp = i64_to_ne(other_fields.timestamp);
        other_fields.mode = u16_to_ne(other_fields.mode);
        other_fields.owner = u16_to_ne(other_fields.owner);
        other_fields.group = u16_to_ne(other_fields.group);
        other_fields.file_size = i64_to_ne(other_fields.file_size);

        long file_content_position = ftell(archive);

        char mode_string[11] = {};

        switch (record_type) {
        case TYPE_DIRECTORY:
            mode_string[0] = 'd';
            break;
        case TYPE_LINK:
            mode_string[0] = 'l';
            break;
        case TYPE_REGULAR:
            mode_string[0] = '-';
            break;
        }

        for (int i = 0; i < 3; i ++) {
            uint16_t triplet = (other_fields.mode >> ((2 - i) * 3)) & 7;
            mode_string[i * 3 + 1] = triplet & 0b100 ? 'r' : '-';
            mode_string[i * 3 + 2] = triplet & 0b010 ? 'w' : '-';
            mode_string[i * 3 + 3] = triplet & 0b001 ? 'x' : '-';
        }

        printf("%s %d/%d ", mode_string, other_fields.owner, other_fields.group);

        int padding_characters_required = 17 - num_digits(other_fields.owner) - num_digits(other_fields.group) - num_digits(other_fields.file_size);
        for (int i = 0; i < padding_characters_required; i ++) {
            printf(" ");
        }

        time_t timestamp_epoch = other_fields.timestamp;
        struct tm timestamp;
        char date_string[17] = {};
        strftime(date_string, 17, "%C%y-%m-%d %H:%M:%S", localtime_r(&timestamp_epoch, &timestamp));

        printf("%lld %s ", other_fields.file_size, date_string);

        fseek(archive, name_position, SEEK_SET); // TODO: do this with SEEK_CUR for best possible compatibility

        // TODO: make this faster with block sized reads/writes
        char c;

        for (int i = 0; i < name_length; i ++) {
            if (fread(&c, 1, 1, archive) != 1) {
                goto read_error;
            }

            if (fwrite(&c, 1, 1, stdout) != 1) {
                return 1;
            }
        }

        if (record_type == TYPE_DIRECTORY) {
            c = '/';
            if (fwrite(&c, 1, 1, stdout) != 1) {
                return 1;
            }
        }

        c = '\n';
        if (fwrite(&c, 1, 1, stdout) != 1 || fflush(stdout) == EOF) {
            return 1;
        }

        fseek(archive, file_content_position, SEEK_SET);
        for (; other_fields.file_size >= 0; other_fields.file_size -= LONG_MAX) {
            fseek(archive, other_fields.file_size > LONG_MAX ? LONG_MAX : other_fields.file_size, SEEK_CUR);
        }
    }

    return 0;
}
