#include "paths.h"
#include "errno.h"
#include <stddef.h>
#include "string.h"
#include "sys/kernel.h"

size_t traverse_path(size_t path_address, void *data, size_t (*fn)(void *, size_t, const char *)) {
    const char *path = (const char *) syscall_invoke(path_address, -1, UNTYPED_LOCK, 0);

    if (path == NULL) {
        return ENOMEM;
    }

    size_t length = strlen(path);
    size_t substring_start = 0;

    for (size_t i = 0; i < length; i ++) {
        if (path[i] != '/') {
            continue;
        }

        size_t length = i - substring_start;
        const char *substring_ptr = path + substring_start;

        if (length > 0 && !(length == 1 && *substring_ptr == '.')) {
            size_t result = fn(data, length, substring_ptr);

            if (result != 0) {
                syscall_invoke(path_address, -1, UNTYPED_UNLOCK, 0);
                return 0;
            }
        }

        // skip any excess slashes
        for (; i < length && path[i] == '/'; i ++);

        substring_start = i;
    }

    syscall_invoke(path_address, -1, UNTYPED_UNLOCK, 0);
    return 0;
}
