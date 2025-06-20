// need this for nftw
#define _XOPEN_SOURCE 700
#define _XOPEN_SOURCE_EXTENDED

#include "endianness.h"
#include "jax.h"
#include <ftw.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char *global_buffer;
const char *global_program_name;
FILE *global_archive;
bool global_is_verbose;

static int nftw_visit(const char *name, const struct stat *file_stat, int type, struct FTW *ftw_struct) {
    (void) ftw_struct;

    if (type == FTW_DNR) {
        fprintf(stderr, "%s: failed to read directory \"%s\"\n", global_program_name, name);
        return 0; // TODO: should this fail?
    } else if (type == FTW_NS) {
        fprintf(stderr, "%s: failed to stat file \"%s\"\n", global_program_name, name);
        return 0; // TODO: should this fail?
    }

    if (global_is_verbose) {
        printf("%s\n", name);
    }

    char record_type;

    switch (type) {
    case FTW_F:
        record_type = TYPE_REGULAR;
        break;
    case FTW_D:
        record_type = TYPE_DIRECTORY;
        break;
    case FTW_SL:
        record_type = TYPE_LINK;
        break;
    }

    if (fwrite(&record_type, 1, 1, global_archive) != 1) {
write_error:
        fprintf(stderr, "%s: failed to write to archive\n", global_program_name);
        return 1;
    }

    uint16_t name_length = strlen(name);
    uint16_t name_length_le = u16_to_le(name_length);

    if (fwrite(&name_length_le, 2, 1, global_archive) != 1 || fwrite(name, name_length, 1, global_archive) != 1) {
        goto write_error;
    }

    uint16_t description_length = 0;

    if (fwrite(&description_length, 2, 1, global_archive) != 1) {
        goto write_error;
    }

    struct {
        int64_t timestamp;
        uint16_t mode;
        uint16_t owner;
        uint16_t group;
        int64_t file_size;
    } __attribute__((__packed__)) other_fields = {
        i64_to_le(file_stat->st_mtime),
        u16_to_le(file_stat->st_mode),
        u16_to_le(file_stat->st_uid),
        u16_to_le(file_stat->st_gid),
        type == FTW_D ? 0 : i64_to_le(file_stat->st_size)
    };

    if (fwrite(&other_fields, sizeof(other_fields), 1, global_archive) != 1) {
        goto write_error;
    }

    if (type == FTW_F) {
        FILE *file = fopen(name, "r");

        if (file == NULL) {
            perror("failed to open file");
            return 1;
        }

        for (int64_t file_size = file_stat->st_size; file_size >= 0; file_size -= BUFSIZ) {
            size_t rw_size = file_size > BUFSIZ ? BUFSIZ : (size_t) file_size; // file_size is guaranteed to be positive due to the loop condition

            if (fread(global_buffer, rw_size, 1, file) != 1) {
                fprintf(stderr, "%s: failed to read from file\n", global_program_name);
                return 1;
            }

            if (fwrite(global_buffer, rw_size, 1, global_archive) != 1) {
                goto write_error;
            }
        }

        fclose(file);
    } else if (type == FTW_SL) {
        if (file_stat->st_size < 0) {
            // TODO: sanity check the upper bound of this on platforms where that's an issue
            fprintf(stderr, "%s: invalid symlink size %ld for file \"%s\"\n", global_program_name, file_stat->st_size, name);
            return 1;
        }

        char *link_target = malloc((size_t) file_stat->st_size);

        if (link_target == NULL) {
            fprintf(stderr, "%s: failed to allocate memory\n", global_program_name);
            return 1;
        }

        ssize_t bytes_read = readlink(name, link_target, (size_t) file_stat->st_size);

        if (bytes_read < 0) { // this could be a check against -1 but realistically this value shouldn't be negative at all
            free(link_target);
            perror("failed to read link");
            return 1;
        } else if (bytes_read != file_stat->st_size) {
            // ooh, a race condition! attempt to handle it
            fseek(global_archive, -8, SEEK_CUR);

            int64_t new_file_size = bytes_read;

            if (fwrite(&new_file_size, 8, 1, global_archive) != 1) {
                goto write_error;
            }
        }

        if (fwrite(link_target, (size_t) bytes_read, 1, global_archive) != 1) { // cast to size_t is valid here because of the above check
            goto write_error;
        }

        free(link_target);
    }

    return 0;
}

static int create_append(const char *program_name, FILE *archive, bool is_verbose, int argc, char **argv, int files_start) {
    // this shouldn't be necessary if whoever designed ftw/nftw had a single fucking brain cell but apparently not!
    global_program_name = program_name;
    global_archive = archive;
    global_is_verbose = is_verbose;
    global_buffer = malloc(BUFSIZ);

    if (global_buffer == NULL) {
        fprintf(stderr, "%s: failed to allocate memory\n", program_name);
        return 1;
    }

    for (int i = files_start; i < argc; i ++) {
        int result = nftw(argv[i], nftw_visit, 16, FTW_PHYS);

        if (result == -1) {
            perror("failed to walk directory tree");
            return 1;
        } else if (result != 0) {
            return result;
        }
    }

    free(global_buffer);

    return 0;
}

int create(const char *program_name, FILE *archive, bool is_verbose, int argc, char **argv, int files_start) {
    char magic[4] = {'^', 'j', 'a', 'x'};

    if (fwrite(magic, 4, 1, archive) != 1) {
        fprintf(stderr, "%s: failed to write to archive\n", program_name);
        return 1;
    }

    return create_append(program_name, archive, is_verbose, argc, argv, files_start);
}

int append(const char *program_name, FILE *archive, bool is_verbose, int argc, char **argv, int files_start) {
    char magic_buffer[5] = {};

    if (fread(magic_buffer, 4, 1, archive) != 1 || strcmp(magic_buffer, "^jax") != 0) {
        fprintf(stderr, "%s: incorrect magic number, cannot continue\n", program_name);
        return 1;
    }

    fseek(archive, 0, SEEK_END);

    // TODO: should the integrity of all the records be verified here?

    return create_append(program_name, archive, is_verbose, argc, argv, files_start);
}
