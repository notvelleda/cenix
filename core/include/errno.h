#pragma once

/// unknown error
#define EUNKNOWN 1

// these 3 error codes are required by ISO C
/// argument out of domain of function
#define EDOM 2
/// illegal byte sequence
#define EILSEQ 3
/// result too large
#define ERANGE 4

/// no such capability
#define ENOCAPABILITY 5
/// operation not supported
#define ENOTSUP 6
#define EOPNOTSUPP ENOTSUP
/// invalid capability for operation
#define ECAPINVAL 7
/// too many layers of nesting
#define ETOOMUCHNESTING 8
/// invalid argument
#define EINVAL 9
/// permission denied
#define EACCES 10
/// not enough space/cannot allocate memory
#define ENOMEM 11
/// capability exists
#define ECAPEXISTS 12
/// file exists
#define EEXIST 13
/// bad message
#define EBADMSG 14
/// no such file or directory
#define ENOENT 15
/// executable file format error
#define ENOEXEC 16
/// functionality not supported
#define ENOSYS 17
/// operation not permitted
#define EPERM 18

// TODO: should more posix errno values be supported?

// TODO: errno variable(s)
