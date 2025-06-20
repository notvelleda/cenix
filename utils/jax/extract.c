#include "endianness.h"
#include <inttypes.h>
#include "jax.h"
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>

int extract(const char *program_name, FILE *archive, bool is_verbose, int argc, char **argv, int files_start) {
    char magic_buffer[5] = {};

    if (fread(magic_buffer, 4, 1, archive) != 1 || strcmp(magic_buffer, "^jax") != 0) {
        fprintf(stderr, "%s: incorrect magic number, cannot continue\n", program_name);
        return 1;
    }

    char *buffer = malloc(BUFSIZ);

    if (buffer == NULL) {
malloc_failed:
        fprintf(stderr, "%s: memory allocation failed, cannot continue\n", program_name);
        return 1;
    }

    while (1) {
        char record_type;

        if (fread(&record_type, 1, 1, archive) != 1) {
            break;
        }

        uint16_t name_length;

        if (read_u16(archive, &name_length)) {
read_error:
            fprintf(stderr, "%s: malformed record, cannot continue\n", program_name);
            return 1;
        }

        char *name = malloc(name_length + 1);

        if (name == NULL) {
            goto malloc_failed;
        }

        if (fread(name, name_length, 1, archive) != 1) {
            goto read_error;
        }

        name[name_length] = 0;

        uint16_t description_length;

        if (read_u16(archive, &description_length)) {
            goto read_error;
        }

        fseek(archive, description_length, SEEK_CUR);

        if (files_start != argc) {
            // specific files to extract have been specified, check if this file is one of those
            bool can_extract_file = false;

            for (int i = files_start; i < argc; i ++) {
                if (strcmp(name, argv[i]) == 0) {
                    can_extract_file = true;
                    break;
                }
            }

            // TODO: check if this file belongs to a directory that's been specified for extraction

            if (!can_extract_file) {
                fseek(archive, 14, SEEK_CUR);

                int64_t file_size;

                if (read_i64(archive, &file_size) != 0) {
                    goto read_error;
                }

                for (; file_size >= 0; file_size -= LONG_MAX) {
                    fseek(archive, file_size > LONG_MAX ? LONG_MAX : file_size, SEEK_CUR);
                }

                continue;
            }
        }

        if (is_verbose) {
            printf("%s\n", name);
        }

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

        if (other_fields.file_size < 0) {
            // since for some reason file size is allowed to be negative (what purpose does that serve?) this serves as a sanity check for that.
            // this allows all
            fprintf(stderr, "%s: ignoring contents of file \"%s\" with invalid size %" PRId64 ", things may break", program_name, name, other_fields.file_size);
            other_fields.file_size = 0;
        }

        if (record_type == TYPE_REGULAR) {
            // create a regular file
            FILE *output_file = fopen(name, "w");

            if (output_file == NULL) {
                perror("failed to open file");
                return 1;
            }

            for (; other_fields.file_size >= 0; other_fields.file_size -= BUFSIZ) {
                size_t rw_size = other_fields.file_size > BUFSIZ ? BUFSIZ : (size_t) other_fields.file_size; // conversion to size_t is safe here because the minimum bound was set above

                if (fread(buffer, rw_size, 1, archive) != 1) {
                    goto read_error;
                }

                if (fwrite(buffer, rw_size, 1, output_file) != 1) {
                    fprintf(stderr, "%s: failed to write to output file\n", program_name);
                    return 1;
                }
            }

            fclose(output_file);
        } else if (record_type == TYPE_LINK) {
            // create a symlink

#if __SIZEOF_POINTER__ < 8
            // sanity check to prevent overflow on non 64 bit platforms
            if (other_fields.file_size >= (int64_t) SIZE_MAX) {
                fprintf(stderr, "%s: symlink target path too big, cannot continue\n", program_name);
                return 1;
            }
#endif

            char *link_target = malloc((size_t) other_fields.file_size + 1);

            if (link_target == NULL) {
                goto malloc_failed;
            }

            if (fread(link_target, (size_t) other_fields.file_size, 1, archive) != 1) {
                goto read_error;
            }

            link_target[other_fields.file_size] = 0;

            if (symlink(link_target, name) != 0) {
                free(link_target);
                perror("failed to create symbolic link");
                return 1;
            }

            free(link_target);
        } else if (record_type == TYPE_DIRECTORY) {
            // create a directory
            if (mkdir(name, other_fields.mode) != 0) {
                perror("failed to create directory");
                return 1;
            }

            // this shouldn't be necessary but you never know
            for (; other_fields.file_size >= 0; other_fields.file_size -= LONG_MAX) {
                fseek(archive, other_fields.file_size > LONG_MAX ? LONG_MAX : other_fields.file_size, SEEK_CUR);
            }
        } else {
            printf("%s: invalid record type '%c'\n", program_name, record_type);
            return 1;
        }

        // TODO: set access time if it's specified in the description
        const struct utimbuf timestamp = {other_fields.timestamp, other_fields.timestamp};
        if (utime(name, &timestamp) != 0) {
            perror("failed to change timestamp of output file/link/directory");
        }

        if (record_type != TYPE_DIRECTORY && chmod(name, other_fields.mode) != 0) {
            perror("failed to change mode of output file/link");
        }

        if (chown(name, other_fields.owner, other_fields.group) != 0) {
            perror("failed to change owner/group of output file/link/directory");
        }

        free(name);
    }

    free(buffer);

    return 0;
}
