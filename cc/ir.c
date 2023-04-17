#include <stddef.h>
#include "ir.h"

#if 0
unsigned int get_type_size(struct type *type) {
    struct struct_union_field *field;

    switch (type->top_type) {
        case TOP_NORMAL:
            switch (type->type.standard_type.type_specifier) {
                case TY_VOID:
                    return 0;
                case TY_CHAR:
                    return 1;
                case TY_SHORT:
                case TY_INT:
                    return 2;
                case TY_LONG:
                    return 4;
                case TY_LONG_LONG:
                    return 8;
                case TY_STRUCT:
                    if (type->type.standard_type.has_fields) {
                        field = type->type.standard_type.name_field_ptr.field;

                        if (field == NULL)
                            return 0;

                        while (1)
                            if (field->next == NULL)
                                return field->offset +
                                    get_type_size(&field->type);
                            else
                                field = field->next;
                    } else
                        return 0;
                case TY_UNION:
                    if (type->type.standard_type.has_fields) {
                        unsigned int largest = 0;

                        field = type->type.standard_type.name_field_ptr.field;
                        while (field != NULL) {
                            unsigned int size = get_type_size(&field->type);
                            if (size > largest)
                                largest = size;

                            field = field->next;
                        }

                        return largest;
                    } else
                        return 0;
                case TY_ENUM:
                    return 0;
            }
        case TOP_ARRAY:
            return type->type.array_type.length *
                get_type_size(type->type.array_type.derivation);
        case TOP_POINTER:
            return 2;
        case TOP_FUNCTION:
            return 0; /* size of function is unknown */
    }
}
#endif
