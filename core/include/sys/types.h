#pragma once

#ifdef UNDER_TEST
#include "/usr/include/sys/types.h"
#else

#include <stdint.h>
#include <stddef.h>

// TODO: clock_t, clockid_t, fsblkcnt_t, fsfilcnt_t, id_t, key_t, suseconds_t, timer_t, useconds_t

typedef uint32_t blkcnt_t;
typedef uint16_t blksize_t; // if anything wants a block size of over 64k it's Stupid and Dumb and i Don't Like It
typedef size_t dev_t; // TODO: figure out how to properly handle this
typedef uint16_t gid_t;
typedef uint32_t ino_t;
typedef uint16_t mode_t;
typedef uint16_t nlink_t;
typedef uint32_t off_t;
typedef uint16_t pid_t;
typedef uint16_t uid_t;
typedef int64_t time_t;

#endif
