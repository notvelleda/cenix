#ifndef OUTPUT_H
#define OUTPUT_H

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include "ir.h"
#include "type.h"

extern FILE *output_file;

void fwrite_checked(const void *ptr, size_t size, FILE *stream);

void fread_checked(void *ptr, size_t size, FILE *stream);

/* writes a node to disk and saves information about it */
void write_node(struct node *node, struct written_node *written, FILE *stream);

/* reads a node from disk at the given offset */
void read_node(off_t offset, struct node *node, FILE *stream);

/* modifies the reference count of a node written to disk */
void mod_references(struct written_node *written, int16_t mod, FILE *stream);

/* writes the given type to disk, returning the offset of the written type */
off_t write_type(struct type *type, FILE *stream);

/* reads a type from disk without reading any function arguments or struct/union
 * fields. after the type is read any extra data associated with it is skipped
 */
void read_type_no_data(struct type *type, FILE *stream);

#endif
