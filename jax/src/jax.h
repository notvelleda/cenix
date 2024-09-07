#pragma once

#include <stdbool.h>
#include <stdio.h>

#define TYPE_DIRECTORY 'd'
#define TYPE_LINK 's'
#define TYPE_REGULAR 'r'

/// lists names of files in the archive
int list(const char *program_name, FILE *archive);

/// verbosely lists files in the archive, printing out modes, creator uid/gid, size, and timestamp in addition to filenames
int list_verbose(const char *program_name, FILE *archive);

/// extracts files from an archive. if no files are provided, all files will be extracted
int extract(const char *program_name, FILE *archive, bool is_verbose, int argc, char **argv, int files_start);

/// creates a new archive, adding the files specified in arguments
int create(const char *program_name, FILE *archive, bool is_verbose, int argc, char **argv, int files_start);

/// appends the files specified in arguments to an existing archive
int append(const char *program_name, FILE *archive, bool is_verbose, int argc, char **argv, int files_start);
