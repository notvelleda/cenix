#include "jax.h"
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#define MODE_CREATE 1
#define MODE_APPEND 2
#define MODE_LIST 3
#define MODE_EXTRACT 4

int main(int argc, char **argv) {
    int c;
    char *archive_file = NULL;
    bool has_error = false;
    bool multiple_exclusive_arguments = false;
    bool verbose = false;
    int mode = 0;

    while ((c = getopt(argc, argv, "cf:rtvx")) != -1) {
        switch (c) {
        case 'f':
            if (archive_file == NULL) {
                archive_file = optarg;
            } else {
                fprintf(stderr, "%s: archive filename cannot be specified multiple times\n", argv[0]);
                has_error = true;
            }
            break;
        case 'c':
            if (mode == 0) {
                mode = MODE_CREATE;
            } else {
                has_error = true;
                multiple_exclusive_arguments = true;
            }
            break;
        case 'r':
            if (mode == 0) {
                mode = MODE_APPEND;
            } else {
                has_error = true;
                multiple_exclusive_arguments = true;
            }
            break;
        case 't':
            if (mode == 0) {
                mode = MODE_LIST;
            } else {
                has_error = true;
                multiple_exclusive_arguments = true;
            }
            break;
        case 'v':
            verbose = true;
            break;
        case 'x':
            if (mode == 0) {
                mode = MODE_EXTRACT;
            } else {
                has_error = true;
                multiple_exclusive_arguments = true;
            }
            break;
        case '?':
            fprintf(stderr, "%s: unrecognized option: -%c\n", argv[0], c);
            has_error = true;
        }
    }

    if (mode == 0) {
        has_error = true;
    }

    if (multiple_exclusive_arguments) {
        fprintf(stderr, "%s: only one of the arguments -crtx can be specified\n", argv[0]);
    }

    if (has_error) {
        fprintf(stderr, "usage: %s {-crtx} [-v] [-f archive] file1 file2...\n", argv[0]);
        return 1;
    }

    FILE *archive;

    if (archive_file == NULL) {
        switch (mode) {
        case MODE_CREATE:
        case MODE_APPEND:
            archive = stdout;
            break;
        case MODE_LIST:
        case MODE_EXTRACT:
            archive = stdin;
            // TODO: should this check isatty? will cenix even support that?
            break;
        }
    } else {
        const char *open_mode;

        switch (mode) {
        case MODE_CREATE:
        case MODE_APPEND:
            open_mode = "w";
            break;
        case MODE_LIST:
        case MODE_EXTRACT:
            open_mode = "r";
            break;
        }

        archive = fopen(archive_file, open_mode);

        if (archive == NULL) {
            perror("failed to open archive file");
            return 1;
        }
    }

    switch (mode) {
    case MODE_CREATE:
        return create(argv[0], archive, verbose, argc, argv, optind);
    case MODE_APPEND:
        return append(argv[0], archive, verbose, argc, argv, optind);
    case MODE_EXTRACT:
        return extract(argv[0], archive, verbose, argc, argv, optind);
    case MODE_LIST:
        if (verbose) {
            return list_verbose(argv[0], archive);
        } else {
            return list(argv[0], archive);
        }
    }

    return 0;
}
