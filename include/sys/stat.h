#pragma once

#include "sys/types.h"

struct stat {
    /// \brief the device ID of the device containing this file.
    ///
    /// this field is essentially meaningless and its contents are undefined, however it's included for compatibility reasons
    dev_t st_dev;
    /// the inode number of this file
    ino_t st_ino;
    /// the file's mode/permissions set
    mode_t st_mode;
    /// the number of hard links to this file (i.e. the number of other files sharing inodes with this one)
    nlink_t st_nlink;
    /// the user ID of this file
    uid_t st_uid;
    /// the group ID of this file
    gid_t st_gid;
    /// \brief the device ID of this file (if it's a character or block device).
    ///
    /// this field is completely meaningless and its contents are undefined as character/block devices are simply not a thing in cenix
    dev_t st_rdev;
    /// the size of this file in bytes
    off_t st_size;
    /// the time this file was last accessed
    time_t st_atime;
    /// the time this file was last modified
    time_t st_mtime;
    /// the time this file was created
    time_t st_ctime;
    /// the preferred I/O block size for this file, as defined by its filesystem
    blksize_t st_blksize;
    /// the number of blocks allocated for this file
    blkcnt_t st_blocks;
};

/// bitmask that includes all the bits in a file's st_mode field that determine its type
#define S_IFMT (15 << 9)

// TODO: document all these defines holy shit why are there so many

#define S_IFBLK (4 << 9)
#define S_IFCHR (5 << 9)
#define S_IFIFO (6 << 9)
#define S_IFREG (1 << 9)
#define S_IFDIR (2 << 9)
#define S_IFLNK (3 << 9)
#define S_IFSOCK (7 << 9)
// these aren't defined in POSIX, but are included here since filesystem servers will need them
#define S_IFMQ (8 << 9)
#define S_IFSEM (9 << 9)
#define S_IFSHM (10 << 9)

#define S_ISBLK(mode) (((mode) & S_IFMT) == S_IFBLK)
#define S_ISCHR(mode) (((mode) & S_IFMT) == S_IFCHR)
#define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#define S_ISFIFO(mode) (((mode) & S_IFMT) == S_IFIFO)
#define S_ISREG(mode) (((mode) & S_IFMT) == S_IFREG)
#define S_ISLNK(mode) (((mode) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(mode) (((mode) & S_IFMT) == S_IFSOCK)

#define S_TYPEISMQ(stat_ptr) (((stat_ptr)->st_mode & S_IFMT) == S_IFMQ)
#define S_TYPEISSEM(stat_ptr) (((stat_ptr)->st_mode & S_IFMT) == S_IFSEM)
#define S_TYPEISSHM(stat_ptr) (((stat_ptr)->st_mode & S_IFMT) == S_IFSHM)

#define S_IRWXU 0b111000000
#define S_IRUSR 0b100000000
#define S_IWUSR 0b010000000
#define S_IXUSR 0b001000000

#define S_IRWXG 0b000111000
#define S_IRGRP 0b000100000
#define S_IWGRP 0b000010000
#define S_IXGRP 0b000001000

#define S_IRWXO 0b000000111
#define S_IROTH 0b000000100
#define S_IWOTH 0b000000010
#define S_IXOTH 0b000000001

// i don't like these so they aren't supported :3
#define S_ISUID 0
#define S_ISGID 0
#define S_ISVTX 0

// TODO: libc function declarations
