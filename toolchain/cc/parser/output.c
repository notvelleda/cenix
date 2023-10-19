#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "output.h"
#include "ir.h"

FILE *output_file = NULL;

void set_output_file(FILE *file) {
    if (output_file != NULL) {
        fprintf(stderr, "can't set output_file multiple times\n");
        exit(1);
    }

    output_file = file;
}

void fwrite_checked(const void *ptr, size_t size, FILE *stream) {
    if (!fwrite(ptr, size, 1, stream)) {
        perror("failed to write to file");
        exit(1);
    }
}

void fread_checked(void *ptr, size_t size, FILE *stream) {
    if (!fread(ptr, size, 1, stream)) {
        perror("failed to read from file");
        exit(1);
    }
}

void write_node(struct node *node, struct written_node *written, FILE *stream) {
    written->offset = ftell(stream);
    written->kind = node->kind;
    written->flags = node->flags;
    written->type = node->type;

    fwrite_checked(node, sizeof(struct node), stream);
}

void read_node(off_t offset, struct node *node, FILE *stream) {
    off_t current = ftell(stream);
    fseek(stream, offset, SEEK_SET);
    fread_checked(node, sizeof(struct node), stream);
    fseek(stream, current, SEEK_SET);
}

void mod_references(struct written_node *written, int16_t mod, FILE *stream) {
    off_t offset = ftell(stream);
    uint16_t references;

    fseek(
        stream,
        written->offset + offsetof(struct node, references),
        SEEK_SET
    );

    fread_checked(&references, 2, stream);

    fseek(stream, -2, SEEK_CUR);

    references += mod;
    if (references < 0)
        references = 0;

    fwrite_checked(&references, 2, stream);

    fseek(stream, offset, SEEK_SET);
}

void write_string(const char *str, FILE *stream) {
    size_t len = 0;

    if (str == NULL) {
        fwrite_checked(&len, sizeof(size_t), stream);
        return;
    }

    len = strlen(str);
    fwrite_checked(&len, sizeof(size_t), stream);
    fwrite_checked(str, len, stream);
}

off_t write_type(struct type *type, FILE *stream) {
    off_t offset = ftell(stream);

    while (type != NULL) {
        size_t len = 0;
        off_t type_offset = ftell(stream);
        off_t type_end;
        fwrite_checked(&len, sizeof(size_t), stream);
        fwrite_checked(type, sizeof(struct type), stream);

        if (type->top == TOP_BASIC) {
            if (type->type.basic.has_fields) {
                /* write struct/union fields */
                struct struct_union_field *field =
                    (struct struct_union_field *)
                        type->type.basic.name_field_ptr;

                for (; field != NULL; field = field->next) {
                    fwrite_checked(
                        field,
                        sizeof(struct struct_union_field),
                        stream
                    );
                    write_type(field->type, stream);
                    write_string(field->name, stream);
                }
            } else
                /* write struct/union name */
                write_string(
                    (const char *) type->type.basic.name_field_ptr,
                    stream
                );
        } else if (type->top == TOP_FUNCTION) {
            /* write function arguments */
            struct function_argument *arg = type->type.derived.type.function;

            for (; arg != NULL; arg = arg->next) {
                write_type(arg->type, stream);
                write_string(arg->name, stream);
                fwrite_checked(&arg->next, sizeof(arg->next), stream);
            }
        }

        /* write type size */
        type_end = ftell(stream);
        fseek(stream, type_offset, SEEK_SET);
        len = type_end - type_offset;
        fwrite_checked(&len, sizeof(size_t), stream);
        fseek(stream, type_end, SEEK_SET);

        if (type->top == TOP_BASIC)
            break;
        else
            type = type->type.derived.derivation;
    }

    return offset;
}

void read_type_no_data(struct type *type, FILE *stream) {
    size_t len;
    fread_checked(&len, sizeof(size_t), stream);
    fread_checked(type, sizeof(struct type), stream);
    fseek(stream, len - sizeof(struct type), SEEK_CUR);
}
