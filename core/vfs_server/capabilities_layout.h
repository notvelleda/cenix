#pragma once

#include <stddef.h>
#include "sys/kernel.h"

#define PROCESS_DATA_NODE_SLOT 4

#define MOUNT_POINTS_NODE_SLOT 5
#define USED_MOUNT_POINT_IDS_SLOT 6

#define MOUNTED_LIST_INFO_SLOT 7
#define MOUNTED_LIST_NODE_SLOT 8
#define USED_MOUNTED_LISTS_SLOT 9

#define NAMESPACE_NODE_SLOT 10
#define USED_NAMESPACES_SLOT 11

#define DIRECTORY_NODE_SLOT 12
#define DIRECTORY_INFO_SLOT 13
#define USED_DIRECTORY_IDS_SLOT 14

#define THREAD_STORAGE_NODE_SLOT 15
// no more slots are available in the root node as it's only 4 bits

#define MOUNT_POINTS_BITS 8
#define MAX_MOUNT_POINTS 256

#define MOUNTED_FS_BITS 8
#define MAX_MOUNTED_FS 256

#define NAMESPACE_BITS 8
#define MAX_NAMESPACES 256

#define DIRECTORY_BITS 8
#define MAX_OPEN_DIRECTORIES 256

// should this be defined here?
#define THREAD_STORAGE_BITS 3 // number of bits required to store IPC_CAPABILITY_SLOTS slots + 1
#define MAX_WORKER_THREADS 8
#define THREAD_STORAGE_NODE_BITS 3 // number of bits required to store MAX_WORKER_THREADS slots

#define SIZE_BITS (sizeof(size_t) * 8)

#define THREAD_STORAGE_ADDRESS(thread_id) (((size_t) (thread_id) << INIT_NODE_DEPTH) | (size_t) THREAD_STORAGE_NODE_SLOT)
#define THREAD_STORAGE_DEPTH (THREAD_STORAGE_BITS + INIT_NODE_DEPTH)
#define THREAD_STORAGE_SLOT(thread_id, slot) (((size_t) (slot) << (THREAD_STORAGE_BITS + INIT_NODE_DEPTH)) | THREAD_STORAGE_ADDRESS(thread_id))
#define THREAD_STORAGE_SLOT_DEPTH (THREAD_STORAGE_NODE_BITS + THREAD_STORAGE_DEPTH)

#define REPLY_ENDPOINT_SLOT ((1 << THREAD_STORAGE_BITS) - 1) // the slot number of the reply endpoint to use when issuing ipc calls to filesystem servers

#if __SIZEOF_POINTER__ == 2
#define MOUNTED_LIST_ENTRY_SIZE 16
#define MOUNTED_LIST_ENTRY_BITS 4
#elif __SIZEOF_POINTER__ == 4
#define MOUNTED_LIST_ENTRY_SIZE 32
#define MOUNTED_LIST_ENTRY_BITS 5
#elif __SIZEOF_POINTER__ == 8
#define MOUNTED_LIST_ENTRY_SIZE 64
#define MOUNTED_LIST_ENTRY_BITS 6
#else
#error unsupported pointer size
#endif

#define MOUNTED_LIST_INFO_ADDRESS(index) (((size_t) (index) << INIT_NODE_DEPTH) | (size_t) MOUNTED_LIST_INFO_SLOT)
#define MOUNTED_LIST_NODE_ADDRESS(index) (((size_t) (index) << INIT_NODE_DEPTH) | (size_t) MOUNTED_LIST_NODE_SLOT)
#define MOUNTED_LIST_NODE_DEPTH (INIT_NODE_DEPTH + MOUNTED_FS_BITS)
#define MOUNTED_LIST_SLOT(index, slot) (((size_t) (slot) << MOUNTED_FS_BITS) | MOUNTED_LIST_NODE_ADDRESS(index))

#define DIRECTORY_ADDRESS(id) (((id) << INIT_NODE_DEPTH) | DIRECTORY_NODE_SLOT)
#define DIRECTORY_INFO_ADDRESS(id) (((id) << INIT_NODE_DEPTH) | DIRECTORY_INFO_SLOT)
