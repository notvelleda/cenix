#include "capabilities_layout.h"
#include "directories.h"
#include "errno.h"
#include "fff.h"
#include <inttypes.h>
#include <string.h>
#include <sys/types.h>
#include "ipc.h"
#include "mount_points.h"
#include "namespaces.h"
#include "structures.h"
#include "sys/kernel.h"
#include "sys/stat.h"
#include "sys/vfs.h"
#include "unity.h"
#include "unity_internals.h"
#include "userland_low_level.h"

DEFINE_FFF_GLOBALS;

FAKE_VALUE_FUNC(size_t, endpoint_send, size_t, size_t, struct capability *, size_t);
FAKE_VALUE_FUNC(size_t, endpoint_receive, size_t, size_t, struct capability *, size_t);

void main_loop(const struct state *state);

struct state state;

size_t endpoint_success_fake(size_t address, size_t depth, struct capability *slot, size_t argument) {
    (void) address;
    (void) depth;
    (void) slot;
    (void) argument;

    return 0;
}

size_t endpoint_error_fake(size_t address, size_t depth, struct capability *slot, size_t argument) {
    (void) address;
    (void) depth;
    (void) slot;
    (void) argument;

    return EUNKNOWN;
}

size_t badge_values[2];

size_t new_process_response_fake(size_t address, size_t depth, struct capability *slot, size_t argument) {
    (void) address;
    (void) depth;
    (void) slot;

    struct ipc_message *message = (struct ipc_message *) argument;

    badge_values[0] = badge_values[1];
    TEST_ASSERT(read_badge(message->capabilities[0].address, message->capabilities[0].depth, &badge_values[1]) == 0);
    TEST_ASSERT(IPC_FLAGS(badge_values[1]) == IPC_FLAG_IS_MOUNT_POINT);

#ifdef DEBUG
    printf("new_process_response_fake: got badge 0x%" PRIxPTR ": id 0x%" PRIxPTR ", flags 0x%" PRIxPTR "\n", badge_values[1], IPC_ID(badge_values[1]), IPC_FLAGS(badge_values[1]));
#endif

    return 0;
}

static size_t response_should_fail_fake(size_t address, size_t depth, struct capability *slot, size_t argument) {
    (void) address;
    (void) depth;
    (void) slot;

    struct ipc_message *message = (struct ipc_message *) argument;
    TEST_ASSERT(FD_RETURN_VALUE(*message) != 0);

    return 0;
}

size_t fd_open_response_fake(size_t address, size_t depth, struct capability *slot, size_t argument) {
    (void) address;
    (void) depth;
    (void) slot;

    struct ipc_message *message = (struct ipc_message *) argument;

    badge_values[0] = badge_values[1];
    TEST_ASSERT(read_badge(message->capabilities[0].address, message->capabilities[0].depth, &badge_values[1]) == 0);

#ifdef DEBUG
    printf("fd_open_response_fake: got badge 0x%" PRIxPTR ": id 0x%" PRIxPTR ", flags 0x%" PRIxPTR "\n", badge_values[1], IPC_ID(badge_values[1]), IPC_FLAGS(badge_values[1]));
#endif

    return 0;
}

struct directory_info *directory_info_from_badge(size_t badge) {
    size_t directory_id = IPC_ID(badge);

    struct directory_info *info = (struct directory_info *) syscall_invoke(directory_id, SIZE_MAX, UNTYPED_LOCK, 0);
    syscall_invoke(directory_id, SIZE_MAX, UNTYPED_UNLOCK, 0); // this is ok since the heap here doesn't reallocate or anything
    TEST_ASSERT(info != NULL);

    return info;
}

void custom_setup(void) {
    RESET_FAKE(endpoint_send);
    RESET_FAKE(endpoint_receive);

    init_vfs_structures();

    const struct alloc_args endpoint_alloc_args = {
        .type = TYPE_ENDPOINT,
        .size = 0,
        .address = 3,
        .depth = SIZE_MAX
    };
    TEST_ASSERT(syscall_invoke(0, SIZE_MAX, ADDRESS_SPACE_ALLOC, (size_t) &endpoint_alloc_args) == 0);

    state = (struct state) {
        .thread_id = 0,
        .temp_slot = IPC_CAPABILITY_SLOTS + 1,
        .endpoint_address = endpoint_alloc_args.address
    };

    const struct alloc_args reply_alloc_args = {
        .type = TYPE_ENDPOINT,
        .size = 0,
        .address = THREAD_STORAGE_SLOT(state.thread_id, 0),
        .depth = SIZE_MAX
    };
    TEST_ASSERT(syscall_invoke(0, SIZE_MAX, ADDRESS_SPACE_ALLOC, (size_t) &reply_alloc_args) == 0);

    endpoint_send_fake.custom_fake = new_process_response_fake;
    TEST_ASSERT(set_up_filesystem_for_process(&state, 1, 1, 0, reply_alloc_args.address) == 0);

    syscall_invoke(THREAD_STORAGE_ADDRESS(state.thread_id), THREAD_STORAGE_DEPTH, NODE_DELETE, 0);

    RESET_FAKE(endpoint_send);
    FFF_RESET_HISTORY();
}

static void fix_up_mount_point_address(struct directory_info *info) {
    // copied from handle_mount_point_message
    if (info->mount_point_address == SIZE_MAX) {
        size_t namespace_address = (info->namespace_id << INIT_NODE_DEPTH) | NAMESPACE_NODE_SLOT;
        struct fs_namespace *namespace = (struct fs_namespace *) syscall_invoke(namespace_address, SIZE_MAX, UNTYPED_LOCK, 0);

        if (namespace != NULL) {
            if (namespace->root_address != SIZE_MAX) {
                info->mount_point_address = namespace->root_address;
            }

            syscall_invoke(namespace_address, SIZE_MAX, UNTYPED_UNLOCK, 0);
        }
    }
}

static void mount_at(size_t badge) {
    // allocate fd endpoint
    const struct alloc_args fd_alloc_args = {
        .type = TYPE_ENDPOINT,
        .size = 0,
        .address = THREAD_STORAGE_SLOT(state.thread_id, 1),
        .depth = SIZE_MAX
    };
    TEST_ASSERT(syscall_invoke(0, SIZE_MAX, ADDRESS_SPACE_ALLOC, (size_t) &fd_alloc_args) == 0);

    // setup for mount call
    size_t info_address = IPC_ID(badge);
    struct directory_info *info = (struct directory_info *) syscall_invoke(info_address, SIZE_MAX, UNTYPED_LOCK, 0);
    TEST_ASSERT(info != NULL);
    fix_up_mount_point_address(info);

    // perform mount call
    TEST_ASSERT(mount(info, fd_alloc_args.address, 0) == 0);
    TEST_ASSERT(syscall_invoke(info_address, SIZE_MAX, UNTYPED_UNLOCK, 0) == 0);

    // delete the endpoint in case it's not copied (since this bit isn't implemented yet lmao)
    syscall_invoke(THREAD_STORAGE_ADDRESS(state.thread_id), THREAD_STORAGE_DEPTH, NODE_DELETE, 1);
}

static void read_directory_entry(size_t badge, size_t position, struct vfs_directory_entry *entry) {
    (void) position;
    (void) entry;

    size_t info_address = IPC_ID(badge);
    struct directory_info *info = (struct directory_info *) syscall_invoke(info_address, SIZE_MAX, UNTYPED_LOCK, 0);
    TEST_ASSERT(info != NULL);
    fix_up_mount_point_address(info);

    TEST_FAIL_MESSAGE("TODO"); // FD_READ for mount points hasn't been implemented :(
}

static void allocate_reply_capability(struct ipc_message *message) {
    const struct alloc_args reply_alloc_args = {
        .type = TYPE_ENDPOINT,
        .size = 0,
        .address = FD_REPLY_ENDPOINT(*message).address,
        .depth = FD_REPLY_ENDPOINT(*message).depth
    };
    TEST_ASSERT(syscall_invoke(0, SIZE_MAX, ADDRESS_SPACE_ALLOC, (size_t) &reply_alloc_args) == 0);
    message->transferred_capabilities = 1;
}

static size_t fd_open_receive_fake(size_t address, size_t depth, struct capability *slot, size_t argument) {
    (void) address;
    (void) depth;
    (void) slot;

    struct ipc_message *message = (struct ipc_message *) argument;

    const struct alloc_args alloc_args = {
        .type = TYPE_ENDPOINT,
        .size = 0,
        .address = FD_OPEN_REPLY_FD(*message).address,
        .depth = FD_OPEN_REPLY_FD(*message).depth
    };
    TEST_ASSERT(syscall_invoke(0, SIZE_MAX, ADDRESS_SPACE_ALLOC, (size_t) &alloc_args) == 0);
    message->transferred_capabilities = 1;

    return 0;
}

static ino_t fd_stat_inode = 0;
static mode_t fd_stat_mode = 0;

static size_t fd_stat_receive_fake(size_t address, size_t depth, struct capability *slot, size_t argument) {
    (void) address;
    (void) depth;
    (void) slot;

    struct ipc_message *message = (struct ipc_message *) argument;

    FD_RETURN_VALUE(*message) = 0;
    FD_STAT_STRUCT(*message) = (struct stat) {
        .st_dev = 0,
        .st_ino = fd_stat_inode,
        .st_mode = fd_stat_mode,
        .st_nlink = 0,
        .st_uid = 0,
        .st_gid = 0,
        .st_rdev = 0,
        .st_size = 0,
        .st_atime = 0,
        .st_mtime = 0,
        .st_ctime = 0,
        .st_blksize = 0,
        .st_blocks = 0
    };

    return 0;
}

/// delete leftover capabilities as is done in main_loop()
static void clean_up_thread_storage(void) {
    for (size_t i = 0; i < IPC_CAPABILITY_SLOTS; i ++) {
        syscall_invoke(THREAD_STORAGE_ADDRESS(state.thread_id), THREAD_STORAGE_DEPTH, NODE_DELETE, i);
    }
}

static void open_at(size_t directory_badge, ino_t inode, mode_t mode, const char *name) {
    // open a directory in the root directory
    size_t info_address = IPC_ID(directory_badge);
    struct directory_info *info = (struct directory_info *) syscall_invoke(info_address, SIZE_MAX, UNTYPED_LOCK, 0);
    TEST_ASSERT(info != NULL);
    fix_up_mount_point_address(info);

    fd_stat_inode = inode;
    fd_stat_mode = mode;

    endpoint_send_fake.call_count = 0; // teehee
    endpoint_receive_fake.call_count = 0;

    size_t (*send_fakes[])(size_t, size_t, struct capability *, size_t) = {
        endpoint_success_fake,
        endpoint_success_fake,
        fd_open_response_fake,
        endpoint_error_fake
    };
    SET_CUSTOM_FAKE_SEQ(endpoint_send, send_fakes, sizeof(send_fakes) / sizeof(send_fakes[0]));

    size_t (*receive_fakes[])(size_t, size_t, struct capability *, size_t) = {
        fd_open_receive_fake,
        fd_stat_receive_fake,
        endpoint_error_fake
    };
    SET_CUSTOM_FAKE_SEQ(endpoint_receive, receive_fakes, sizeof(receive_fakes) / sizeof(receive_fakes[0]));

    struct ipc_message message;

    // allocate a capability to store the name
    const struct alloc_args alloc_args = {
        .type = TYPE_UNTYPED,
        .size = strlen(name) + 1,
        .address = THREAD_STORAGE_SLOT(state.thread_id, 1),
        .depth = THREAD_STORAGE_SLOT_DEPTH
    };
    TEST_ASSERT(syscall_invoke(0, SIZE_MAX, ADDRESS_SPACE_ALLOC, (size_t) &alloc_args) == 0);

    // copy the name into it
    char *buffer = (char *) syscall_invoke(alloc_args.address, alloc_args.depth, UNTYPED_LOCK, 0);
    TEST_ASSERT(buffer != NULL);

    memcpy(buffer, name, alloc_args.size);

    TEST_ASSERT(syscall_invoke(alloc_args.address, alloc_args.depth, UNTYPED_UNLOCK, 0) == 0);

    FD_OPEN_MODE(message) = MODE_READ;
    FD_OPEN_FLAGS(message) = OPEN_DIRECTORY;
    FD_OPEN_NAME_ADDRESS(message) = (struct ipc_capability) {alloc_args.address, alloc_args.depth};

    FD_REPLY_ENDPOINT(message) = (struct ipc_capability) {THREAD_STORAGE_SLOT(state.thread_id, 0), SIZE_MAX};
    allocate_reply_capability(&message);

    mount_point_open(&state, info, &message);

    TEST_ASSERT(syscall_invoke(info_address, SIZE_MAX, UNTYPED_UNLOCK, 0) == 0);

    if (S_ISDIR(mode)) {
        TEST_ASSERT(IPC_FLAGS(badge_values[1]) == IPC_FLAG_IS_DIRECTORY || IPC_FLAGS(badge_values[1]) == IPC_FLAG_IS_MOUNT_POINT);
    }

    TEST_ASSERT(endpoint_send_fake.call_count == 3);
    TEST_ASSERT(endpoint_receive_fake.call_count == 2);

    SET_CUSTOM_FAKE_SEQ(endpoint_send, NULL, 0);
    SET_CUSTOM_FAKE_SEQ(endpoint_receive, NULL, 0);
    endpoint_send_fake.return_val = EUNKNOWN;
    endpoint_receive_fake.return_val = EUNKNOWN;

    clean_up_thread_storage();
}

static void create_and_badge(size_t container_address, size_t container_depth, size_t slot, uint8_t type, size_t badge) {
    const struct alloc_args alloc_args = {
        .type = type,
        .size = 0,
        .address = THREAD_STORAGE_SLOT(state.thread_id, IPC_CAPABILITY_SLOTS + 1),
        .depth = SIZE_MAX
    };
    TEST_ASSERT(syscall_invoke(0, SIZE_MAX, ADDRESS_SPACE_ALLOC, (size_t) &alloc_args) == 0);

    const struct node_copy_args copy_args = {
        .source_address = alloc_args.address,
        .source_depth = alloc_args.depth,
        .dest_slot = slot,
        .access_rights = UINT8_MAX,
        .badge = badge,
        .should_set_badge = true,
    };
    TEST_ASSERT(syscall_invoke(container_address, container_depth, NODE_COPY, (size_t) &copy_args) == 0);

    syscall_invoke(THREAD_STORAGE_ADDRESS(state.thread_id), THREAD_STORAGE_DEPTH, NODE_DELETE, IPC_CAPABILITY_SLOTS + 1);
}

// ====================================================================================================
// VFS_NEW_PROCESS tests
// ====================================================================================================

// (VFS_NEW_PROCESS + VFS_SHARE_NAMESPACE) test that the namespace of the new process is the same as its creator
void new_process_share_namespace(void) {
    const struct alloc_args reply_alloc_args = {
        .type = TYPE_ENDPOINT,
        .size = 0,
        .address = THREAD_STORAGE_SLOT(state.thread_id, 0),
        .depth = SIZE_MAX
    };
    syscall_invoke(0, SIZE_MAX, ADDRESS_SPACE_ALLOC, (size_t) &reply_alloc_args);

    endpoint_send_fake.custom_fake = new_process_response_fake;
    TEST_ASSERT(set_up_filesystem_for_process(&state, 1, 2, VFS_SHARE_NAMESPACE, reply_alloc_args.address) == 0);

    TEST_ASSERT(directory_info_from_badge(badge_values[0])->namespace_id == directory_info_from_badge(badge_values[1])->namespace_id);
    TEST_ASSERT(directory_info_from_badge(badge_values[0])->can_modify_namespace == true);
    TEST_ASSERT(directory_info_from_badge(badge_values[1])->can_modify_namespace == true);
}

size_t new_process_pid_2_share_fake(size_t address, size_t depth, struct capability *slot, size_t argument) {
    (void) address;
    (void) depth;
    (void) slot;

    struct ipc_message *message = (struct ipc_message *) argument;

    uint16_t new_pid = 2;
    uint16_t creator_pid = 1;

    message->badge = IPC_BADGE(0, 0);

    message->buffer[0] = VFS_NEW_PROCESS;
    message->buffer[1] = VFS_SHARE_NAMESPACE;
    message->buffer[2] = new_pid >> 8;
    message->buffer[3] = new_pid & 0xff;
    message->buffer[4] = creator_pid >> 8;
    message->buffer[5] = creator_pid & 0xff;

    allocate_reply_capability(message);

    return 0;
}

void new_process_share_namespace_ipc(void) {
    size_t (*send_fakes[])(size_t, size_t, struct capability *, size_t) = {
        new_process_response_fake,
        endpoint_error_fake
    };
    SET_CUSTOM_FAKE_SEQ(endpoint_send, send_fakes, sizeof(send_fakes) / sizeof(send_fakes[0]));

    size_t (*receive_fakes[])(size_t, size_t, struct capability *, size_t) = {
        new_process_pid_2_share_fake,
        endpoint_error_fake
    };
    SET_CUSTOM_FAKE_SEQ(endpoint_receive, receive_fakes, sizeof(receive_fakes) / sizeof(receive_fakes[0]));

    main_loop(&state);

    TEST_ASSERT(endpoint_send_fake.call_count == 1); // called once per call, 1 call in this test
    TEST_ASSERT(endpoint_receive_fake.call_count == 2); // called once per call plus an extra at the end to break

    TEST_ASSERT(directory_info_from_badge(badge_values[0])->namespace_id == directory_info_from_badge(badge_values[1])->namespace_id);
    TEST_ASSERT(directory_info_from_badge(badge_values[0])->can_modify_namespace == true);
    TEST_ASSERT(directory_info_from_badge(badge_values[1])->can_modify_namespace == true);
}

// (VFS_NEW_PROCESS + VFS_SHARE_NAMESPACE) test that read-onlyness is preserved
void new_process_share_read_only_namespace(void) {
    const struct alloc_args reply_alloc_args = {
        .type = TYPE_ENDPOINT,
        .size = 0,
        .address = THREAD_STORAGE_SLOT(state.thread_id, 0),
        .depth = SIZE_MAX
    };
    syscall_invoke(0, SIZE_MAX, ADDRESS_SPACE_ALLOC, (size_t) &reply_alloc_args);

    endpoint_send_fake.custom_fake = new_process_response_fake;
    TEST_ASSERT(set_up_filesystem_for_process(&state, 1, 2, VFS_SHARE_NAMESPACE | VFS_READ_ONLY_NAMESPACE, reply_alloc_args.address) == 0);

    endpoint_send_fake.custom_fake = new_process_response_fake;
    TEST_ASSERT(set_up_filesystem_for_process(&state, 2, 3, VFS_SHARE_NAMESPACE, reply_alloc_args.address) == 0);

    TEST_ASSERT(directory_info_from_badge(badge_values[0])->namespace_id == directory_info_from_badge(badge_values[1])->namespace_id);
    TEST_ASSERT(directory_info_from_badge(badge_values[0])->can_modify_namespace == false);
    TEST_ASSERT(directory_info_from_badge(badge_values[1])->can_modify_namespace == false);
}

size_t new_process_pid_2_read_only_fake(size_t address, size_t depth, struct capability *slot, size_t argument) {
    (void) address;
    (void) depth;
    (void) slot;

    struct ipc_message *message = (struct ipc_message *) argument;

    uint16_t new_pid = 2;
    uint16_t creator_pid = 1;

    message->badge = IPC_BADGE(0, 0);

    message->buffer[0] = VFS_NEW_PROCESS;
    message->buffer[1] = VFS_SHARE_NAMESPACE | VFS_READ_ONLY_NAMESPACE;
    message->buffer[2] = new_pid >> 8;
    message->buffer[3] = new_pid & 0xff;
    message->buffer[4] = creator_pid >> 8;
    message->buffer[5] = creator_pid & 0xff;

    allocate_reply_capability(message);

    return 0;
}

size_t new_process_pid_3_share_fake(size_t address, size_t depth, struct capability *slot, size_t argument) {
    (void) address;
    (void) depth;
    (void) slot;

    struct ipc_message *message = (struct ipc_message *) argument;

    uint16_t new_pid = 3;
    uint16_t creator_pid = 2;

    message->badge = IPC_BADGE(0, 0);

    message->buffer[0] = VFS_NEW_PROCESS;
    message->buffer[1] = VFS_SHARE_NAMESPACE;
    message->buffer[2] = new_pid >> 8;
    message->buffer[3] = new_pid & 0xff;
    message->buffer[4] = creator_pid >> 8;
    message->buffer[5] = creator_pid & 0xff;

    allocate_reply_capability(message);

    return 0;
}

void new_process_share_read_only_namespace_ipc(void) {
    size_t (*send_fakes[])(size_t, size_t, struct capability *, size_t) = {
        new_process_response_fake,
        new_process_response_fake,
        endpoint_error_fake
    };
    SET_CUSTOM_FAKE_SEQ(endpoint_send, send_fakes, sizeof(send_fakes) / sizeof(send_fakes[0]));

    size_t (*receive_fakes[])(size_t, size_t, struct capability *, size_t) = {
        new_process_pid_2_read_only_fake,
        new_process_pid_3_share_fake,
        endpoint_error_fake
    };
    SET_CUSTOM_FAKE_SEQ(endpoint_receive, receive_fakes, sizeof(receive_fakes) / sizeof(receive_fakes[0]));

    main_loop(&state);

    TEST_ASSERT(endpoint_send_fake.call_count == 2); // called once per call, 2 calls in this test
    TEST_ASSERT(endpoint_receive_fake.call_count == 3); // called once per call plus an extra at the end to break

    TEST_ASSERT(directory_info_from_badge(badge_values[0])->namespace_id == directory_info_from_badge(badge_values[1])->namespace_id);
    TEST_ASSERT(directory_info_from_badge(badge_values[0])->can_modify_namespace == false);
    TEST_ASSERT(directory_info_from_badge(badge_values[1])->can_modify_namespace == false);
}

// (VFS_NEW_PROCESS + VFS_READ_ONLY_NAMESPACE) test that the namespace of the new process cannot be modified with mount/unmount calls
void new_process_read_only_namespace(void) {
    const struct alloc_args reply_alloc_args = {
        .type = TYPE_ENDPOINT,
        .size = 0,
        .address = THREAD_STORAGE_SLOT(state.thread_id, 0),
        .depth = SIZE_MAX
    };
    syscall_invoke(0, SIZE_MAX, ADDRESS_SPACE_ALLOC, (size_t) &reply_alloc_args);

    endpoint_send_fake.custom_fake = new_process_response_fake;
    TEST_ASSERT(set_up_filesystem_for_process(&state, 1, 2, VFS_SHARE_NAMESPACE | VFS_READ_ONLY_NAMESPACE, reply_alloc_args.address) == 0);

    // allocate fd endpoint
    const struct alloc_args fd_alloc_args = {
        .type = TYPE_ENDPOINT,
        .size = 0,
        .address = THREAD_STORAGE_SLOT(state.thread_id, 1),
        .depth = SIZE_MAX
    };
    TEST_ASSERT(syscall_invoke(0, SIZE_MAX, ADDRESS_SPACE_ALLOC, (size_t) &fd_alloc_args) == 0);

    // setup for mount call
    size_t info_address = IPC_ID(badge_values[1]);
    struct directory_info *info = (struct directory_info *) syscall_invoke(info_address, SIZE_MAX, UNTYPED_LOCK, 0);
    TEST_ASSERT(info != NULL);
    fix_up_mount_point_address(info);

    // perform mount call
    TEST_ASSERT(mount(info, fd_alloc_args.address, 0) == EPERM);
    TEST_ASSERT(syscall_invoke(info_address, SIZE_MAX, UNTYPED_UNLOCK, 0) == 0);

    // TODO: test that mounting in subdirectories still fails
    // TODO: test fd_unmount here when it's implemented
}

static size_t mount_root_directory_fake(size_t address, size_t depth, struct capability *slot, size_t argument) {
    (void) address;
    (void) depth;
    (void) slot;

    struct ipc_message *message = (struct ipc_message *) argument;

    FD_CALL_NUMBER(*message) = FD_MOUNT;
    FD_MOUNT_FLAGS(*message) = 0;

    const struct alloc_args fd_alloc_args = {
        .type = TYPE_ENDPOINT,
        .size = 0,
        .address = FD_MOUNT_FILE_DESCRIPTOR(*message).address,
        .depth = FD_MOUNT_FILE_DESCRIPTOR(*message).depth
    };
    TEST_ASSERT(syscall_invoke(0, SIZE_MAX, ADDRESS_SPACE_ALLOC, (size_t) &fd_alloc_args) == 0);

    allocate_reply_capability(message);

    return 0;
}

void new_process_read_only_namespace_ipc(void) {
    size_t (*send_fakes[])(size_t, size_t, struct capability *, size_t) = {
        new_process_response_fake,
        response_should_fail_fake,
        endpoint_error_fake
    };
    SET_CUSTOM_FAKE_SEQ(endpoint_send, send_fakes, sizeof(send_fakes) / sizeof(send_fakes[0]));

    size_t (*receive_fakes[])(size_t, size_t, struct capability *, size_t) = {
        new_process_pid_2_read_only_fake,
        mount_root_directory_fake,
        endpoint_error_fake
    };
    SET_CUSTOM_FAKE_SEQ(endpoint_receive, receive_fakes, sizeof(receive_fakes) / sizeof(receive_fakes[0]));

    main_loop(&state);

    TEST_ASSERT(endpoint_send_fake.call_count == 2);
    TEST_ASSERT(endpoint_receive_fake.call_count == 3);

    // TODO: have this test achieve parity with the previous one
}

// (VFS_NEW_PROCESS + no flags) test that a new namespace is created for this process
void new_process_new_namespace(void) {
    const struct alloc_args reply_alloc_args = {
        .type = TYPE_ENDPOINT,
        .size = 0,
        .address = THREAD_STORAGE_SLOT(state.thread_id, 0),
        .depth = SIZE_MAX
    };
    syscall_invoke(0, SIZE_MAX, ADDRESS_SPACE_ALLOC, (size_t) &reply_alloc_args);

    endpoint_send_fake.custom_fake = new_process_response_fake;
    TEST_ASSERT(set_up_filesystem_for_process(&state, 1, 2, 0, reply_alloc_args.address) == 0);

    TEST_ASSERT(directory_info_from_badge(badge_values[0])->namespace_id == 0);
    TEST_ASSERT(directory_info_from_badge(badge_values[1])->namespace_id == 1);
    TEST_ASSERT(directory_info_from_badge(badge_values[0])->can_modify_namespace == true);
    TEST_ASSERT(directory_info_from_badge(badge_values[1])->can_modify_namespace == true);
}

size_t new_process_pid_2_new_namespace_fake(size_t address, size_t depth, struct capability *slot, size_t argument) {
    (void) address;
    (void) depth;
    (void) slot;

    struct ipc_message *message = (struct ipc_message *) argument;

    uint16_t new_pid = 2;
    uint16_t creator_pid = 1;

    message->badge = IPC_BADGE(0, 0);

    message->buffer[0] = VFS_NEW_PROCESS;
    message->buffer[1] = 0;
    message->buffer[2] = new_pid >> 8;
    message->buffer[3] = new_pid & 0xff;
    message->buffer[4] = creator_pid >> 8;
    message->buffer[5] = creator_pid & 0xff;

    allocate_reply_capability(message);

    return 0;
}

void new_process_new_namespace_ipc(void) {
    size_t (*send_fakes[])(size_t, size_t, struct capability *, size_t) = {
        new_process_response_fake,
        endpoint_error_fake
    };
    SET_CUSTOM_FAKE_SEQ(endpoint_send, send_fakes, sizeof(send_fakes) / sizeof(send_fakes[0]));

    size_t (*receive_fakes[])(size_t, size_t, struct capability *, size_t) = {
        new_process_pid_2_new_namespace_fake,
        endpoint_error_fake
    };
    SET_CUSTOM_FAKE_SEQ(endpoint_receive, receive_fakes, sizeof(receive_fakes) / sizeof(receive_fakes[0]));

    main_loop(&state);

    TEST_ASSERT(endpoint_send_fake.call_count == 1); // called once per call, 1 call in this test
    TEST_ASSERT(endpoint_receive_fake.call_count == 2); // called once per call plus an extra at the end to break

    TEST_ASSERT(directory_info_from_badge(badge_values[0])->namespace_id == 0);
    TEST_ASSERT(directory_info_from_badge(badge_values[1])->namespace_id == 1);
    TEST_ASSERT(directory_info_from_badge(badge_values[0])->can_modify_namespace == true);
    TEST_ASSERT(directory_info_from_badge(badge_values[1])->can_modify_namespace == true);
}

// ====================================================================================================
// FD_READ tests
// ====================================================================================================

// (FD_READ) mount points should emit one single instance of . and .. each
void fd_read_dot_entries(void) {
    // mount two filesystems on the root directory
    mount_at(badge_values[1]);
    mount_at(badge_values[1]);

    uint8_t data[256];
    struct vfs_directory_entry *entry = (struct vfs_directory_entry *) &data;

    endpoint_receive_fake.return_val = EUNKNOWN; // so that unimplemented stuff won't trip me up later

    read_directory_entry(badge_values[1], 0, entry);
    TEST_ASSERT(entry->name_length == 1);
    TEST_ASSERT(entry->name[0] == '.');

    read_directory_entry(badge_values[1], entry->next_entry_position, entry);
    TEST_ASSERT(entry->name_length == 2);
    TEST_ASSERT(entry->name[0] == '.');
    TEST_ASSERT(entry->name[1] == '.');

    while (entry->next_entry_position != 0) {
        read_directory_entry(badge_values[1], entry->next_entry_position, entry);
        TEST_ASSERT(!((entry->name_length == 1 && entry->name[0] == '.') || (entry->name_length == 2 && entry->name[0] == '.' && entry->name[1] == '.')));
    }
}

void fd_read_dot_entries_ipc(void) {
    TEST_FAIL_MESSAGE("TODO");
}

// (FD_READ) mount points should emit all directory entries for all mounted directories in the order of which they were mounted
void fd_read_mount_ordering(void) {
    TEST_FAIL_MESSAGE("TODO");
}

void fd_read_mount_ordering_ipc(void) {
    TEST_FAIL_MESSAGE("TODO");
}

static size_t fd_read_passthru_fake(size_t address, size_t depth, struct capability *slot, size_t argument) {
    (void) address;
    (void) depth;
    (void) slot;

    struct ipc_message *message = (struct ipc_message *) argument;

    TEST_ASSERT(FD_CALL_NUMBER(*message) == FD_READ);
    TEST_ASSERT(FD_READ_SIZE(*message) == 32);
    TEST_ASSERT(FD_READ_POSITION(*message) == 1234);

    size_t badge;
    TEST_ASSERT(read_badge(FD_READ_BUFFER(*message).address, FD_READ_BUFFER(*message).depth, &badge) == 0);
    TEST_ASSERT(badge == 0xdeadbeef);

    return 0;
}

static size_t fd_read_fast_passthru_fake(size_t address, size_t depth, struct capability *slot, size_t argument) {
    (void) address;
    (void) depth;
    (void) slot;

    struct ipc_message *message = (struct ipc_message *) argument;

    TEST_ASSERT(FD_CALL_NUMBER(*message) == FD_READ_FAST);
    TEST_ASSERT(FD_READ_FAST_SIZE(*message) == 32);
    TEST_ASSERT(FD_READ_FAST_POSITION(*message) == 1234);

    return 0;
}

static size_t fd_stat_passthru_fake(size_t address, size_t depth, struct capability *slot, size_t argument) {
    (void) address;
    (void) depth;
    (void) slot;

    struct ipc_message *message = (struct ipc_message *) argument;
    TEST_ASSERT(FD_CALL_NUMBER(*message) == FD_STAT);

    return 0;
}

static size_t fd_link_passthru_fake(size_t address, size_t depth, struct capability *slot, size_t argument) {
    (void) address;
    (void) depth;
    (void) slot;

    struct ipc_message *message = (struct ipc_message *) argument;

    TEST_ASSERT(FD_CALL_NUMBER(*message) == FD_LINK);

    size_t badge;
    TEST_ASSERT(read_badge(FD_LINK_FD(*message).address, FD_LINK_FD(*message).depth, &badge) == 0);
    TEST_ASSERT(badge == 0xdeadbeef);
    TEST_ASSERT(read_badge(FD_LINK_NAME_ADDRESS(*message).address, FD_LINK_NAME_ADDRESS(*message).depth, &badge) == 0);
    TEST_ASSERT(badge == 0xabababab);

    return 0;
}

static size_t fd_unlink_passthru_fake(size_t address, size_t depth, struct capability *slot, size_t argument) {
    (void) address;
    (void) depth;
    (void) slot;

    struct ipc_message *message = (struct ipc_message *) argument;

    TEST_ASSERT(FD_CALL_NUMBER(*message) == FD_UNLINK);

    size_t badge;
    TEST_ASSERT(read_badge(FD_UNLINK_NAME_ADDRESS(*message).address, FD_UNLINK_NAME_ADDRESS(*message).depth, &badge) == 0);
    TEST_ASSERT(badge == 0xdeadbeef);

    return 0;
}

// (FD_READ) directories should pass thru
void fd_read_directory_passthru(void) {
    mount_at(badge_values[1]);
    open_at(badge_values[1], 1234, S_IFDIR | 0777, "random_name");

    // test FD_READ
    struct ipc_message message = {
        .badge = badge_values[1]
    };

    FD_CALL_NUMBER(message) = FD_READ;
    FD_READ_SIZE(message) = 32;
    FD_READ_POSITION(message) = 1234;
    create_and_badge(THREAD_STORAGE_ADDRESS(state.thread_id), THREAD_STORAGE_DEPTH, 1, TYPE_ENDPOINT, 0xdeadbeef);
    FD_READ_BUFFER(message) = (struct ipc_capability) {THREAD_STORAGE_SLOT(state.thread_id, 1), SIZE_MAX};

    endpoint_send_fake.call_count = 0;
    endpoint_send_fake.custom_fake = fd_read_passthru_fake;

    handle_directory_message(&state, &message);

    TEST_ASSERT(endpoint_send_fake.call_count == 1);
    clean_up_thread_storage();

    // test FD_READ_FAST
    memset(message.capabilities, 0, sizeof(message.capabilities));
    FD_CALL_NUMBER(message) = FD_READ_FAST;
    FD_READ_FAST_SIZE(message) = 32;
    FD_READ_FAST_POSITION(message) = 1234;

    endpoint_send_fake.call_count = 0;
    endpoint_send_fake.custom_fake = fd_read_fast_passthru_fake;

    handle_directory_message(&state, &message);

    TEST_ASSERT(endpoint_send_fake.call_count == 1);
    clean_up_thread_storage();

    // test FD_STAT
    memset(message.capabilities, 0, sizeof(message.capabilities));
    FD_CALL_NUMBER(message) = FD_STAT;

    endpoint_send_fake.call_count = 0;
    endpoint_send_fake.custom_fake = fd_stat_passthru_fake;

    handle_directory_message(&state, &message);

    TEST_ASSERT(endpoint_send_fake.call_count == 1);
    clean_up_thread_storage();

    // test FD_LINK
    memset(message.capabilities, 0, sizeof(message.capabilities));
    FD_CALL_NUMBER(message) = FD_LINK;
    create_and_badge(THREAD_STORAGE_ADDRESS(state.thread_id), THREAD_STORAGE_DEPTH, 1, TYPE_ENDPOINT, 0xdeadbeef);
    FD_LINK_FD(message) = (struct ipc_capability) {THREAD_STORAGE_SLOT(state.thread_id, 1), SIZE_MAX};
    create_and_badge(THREAD_STORAGE_ADDRESS(state.thread_id), THREAD_STORAGE_DEPTH, 2, TYPE_UNTYPED, 0xabababab);
    FD_LINK_NAME_ADDRESS(message) = (struct ipc_capability) {THREAD_STORAGE_SLOT(state.thread_id, 2), SIZE_MAX};

    endpoint_send_fake.call_count = 0;
    endpoint_send_fake.custom_fake = fd_link_passthru_fake;

    handle_directory_message(&state, &message);

    TEST_ASSERT(endpoint_send_fake.call_count == 1);
    clean_up_thread_storage();

    // test FD_UNLINK
    memset(message.capabilities, 0, sizeof(message.capabilities));
    FD_CALL_NUMBER(message) = FD_UNLINK;
    create_and_badge(THREAD_STORAGE_ADDRESS(state.thread_id), THREAD_STORAGE_DEPTH, 1, TYPE_UNTYPED, 0xdeadbeef);
    FD_UNLINK_NAME_ADDRESS(message) = (struct ipc_capability) {THREAD_STORAGE_SLOT(state.thread_id, 1), SIZE_MAX};

    endpoint_send_fake.call_count = 0;
    endpoint_send_fake.custom_fake = fd_unlink_passthru_fake;

    handle_directory_message(&state, &message);

    TEST_ASSERT(endpoint_send_fake.call_count == 1);
    clean_up_thread_storage();

    // TODO: FD_UNLINK
}

void fd_read_directory_passthru_ipc(void) {
    TEST_FAIL_MESSAGE("TODO");
}

// ====================================================================================================
// FD_WRITE tests
// ====================================================================================================

// (FD_WRITE) should fail! not allowed in directories
void fd_write_should_fail(void) {
    mount_at(badge_values[1]);
    open_at(badge_values[1], 1234, S_IFDIR | 0777, "random_name");

    struct ipc_message message = {
        .badge = badge_values[1]
    };

    FD_REPLY_ENDPOINT(message) = (struct ipc_capability) {THREAD_STORAGE_SLOT(state.thread_id, 0), SIZE_MAX};
    allocate_reply_capability(&message);

    FD_CALL_NUMBER(message) = FD_WRITE;
    FD_WRITE_SIZE(message) = 32;
    FD_WRITE_POSITION(message) = 1234;
    create_and_badge(THREAD_STORAGE_ADDRESS(state.thread_id), THREAD_STORAGE_DEPTH, 1, TYPE_UNTYPED, 0xdeadbeef);
    FD_WRITE_BUFFER(message) = (struct ipc_capability) {THREAD_STORAGE_SLOT(state.thread_id, 1), SIZE_MAX};

    endpoint_send_fake.call_count = 0;
    endpoint_send_fake.custom_fake = response_should_fail_fake;

    handle_directory_message(&state, &message);

    TEST_ASSERT(endpoint_send_fake.call_count == 1);
}

// (FD_WRITE_FAST) should fail! not allowed in directories
void fd_write_fast_should_fail(void) {
    mount_at(badge_values[1]);
    open_at(badge_values[1], 1234, S_IFDIR | 0777, "random_name");

    struct ipc_message message = {
        .badge = badge_values[1]
    };

    FD_REPLY_ENDPOINT(message) = (struct ipc_capability) {THREAD_STORAGE_SLOT(state.thread_id, 0), SIZE_MAX};
    allocate_reply_capability(&message);

    FD_CALL_NUMBER(message) = FD_WRITE_FAST;
    FD_WRITE_FAST_SIZE(message) = 32;
    FD_WRITE_FAST_POSITION(message) = 1234;

    endpoint_send_fake.call_count = 0;
    endpoint_send_fake.custom_fake = response_should_fail_fake;

    handle_directory_message(&state, &message);

    TEST_ASSERT(endpoint_send_fake.call_count == 1);
}

// ====================================================================================================
// FD_STAT tests
// ====================================================================================================

// (FD_STAT) mount points should be partially passed thru, with some fields overwritten
void fd_stat_mount_points(void) {
    TEST_FAIL_MESSAGE("TODO");
}

void fd_stat_mount_points_ipc(void) {
    TEST_FAIL_MESSAGE("TODO");
}

// (FD_STAT) directories should be passed thru
void fd_stat_directories(void) {
    TEST_FAIL_MESSAGE("TODO");
}

void fd_stat_directories_ipc(void) {
    TEST_FAIL_MESSAGE("TODO");
}

// ====================================================================================================
// FD_OPEN tests
// ====================================================================================================

// (FD_OPEN) test directory proxying for mount points
void fd_open_mount_point_proxy(void) {
    TEST_FAIL_MESSAGE("TODO");
}

void fd_open_mount_point_proxy_ipc(void) {
    TEST_FAIL_MESSAGE("TODO");
}

// (FD_OPEN) test directory proxying for directories
void fd_open_directory_proxy(void) {
    TEST_FAIL_MESSAGE("TODO");
}

void fd_open_directory_proxy_ipc(void) {
    TEST_FAIL_MESSAGE("TODO");
}

// (FD_OPEN) test that OPEN_CREATE works
void fd_open_create(void) {
    TEST_FAIL_MESSAGE("TODO");
}

// (FD_OPEN) test that OPEN_EXCLUSIVE works
void fd_open_exclusive(void) {
    TEST_FAIL_MESSAGE("TODO");
}

// (FD_OPEN) test that OPEN_DIRECTORY works
void fd_open_directory(void) {
    TEST_FAIL_MESSAGE("TODO");
}

// (FD_OPEN) test non directory passthrough
void fd_open_file(void) {
    TEST_FAIL_MESSAGE("TODO");
}

// ====================================================================================================
// FD_LINK tests
// ====================================================================================================

// (FD_LINK) directories should be passed thru
void fd_link_in_directories(void) {
    TEST_FAIL_MESSAGE("TODO");
}

void fd_link_in_directories_ipc(void) {
    TEST_FAIL_MESSAGE("TODO");
}

// (FD_LINK) should call FD_LINK in every mounted filesystem with the MOUNT_CREATE flag set
void fd_link_in_mount_points(void) {
    TEST_FAIL_MESSAGE("TODO");
}

void fd_link_in_mount_points_ipc(void) {
    TEST_FAIL_MESSAGE("TODO");
}

// ====================================================================================================
// FD_UNLINK tests
// ====================================================================================================

// (FD_UNLINK) directories should be passed thru
void fd_unlink_in_directories(void) {
    TEST_FAIL_MESSAGE("TODO");
}

void fd_unlink_in_directories_ipc(void) {
    TEST_FAIL_MESSAGE("TODO");
}

// (FD_UNLINK) should call FD_UNLINK in every mounted filesystem until one of the calls returns successfully
void fd_unlink_in_mount_points(void) {
    TEST_FAIL_MESSAGE("TODO");
}

void fd_unlink_in_mount_points_ipc(void) {
    TEST_FAIL_MESSAGE("TODO");
}

// ====================================================================================================
// FD_TRUNCATE tests
// ====================================================================================================

// (FD_TRUNCATE) should fail! not allowed in directories
void fd_truncate_should_fail(void) {
    mount_at(badge_values[1]);
    open_at(badge_values[1], 1234, S_IFDIR | 0777, "random_name");

    struct ipc_message message = {
        .badge = badge_values[1]
    };

    FD_REPLY_ENDPOINT(message) = (struct ipc_capability) {THREAD_STORAGE_SLOT(state.thread_id, 0), SIZE_MAX};
    allocate_reply_capability(&message);

    FD_CALL_NUMBER(message) = FD_TRUNCATE;
    FD_TRUNCATE_SIZE(message) = 0xdeadbeef;

    endpoint_send_fake.call_count = 0;
    endpoint_send_fake.custom_fake = response_should_fail_fake;

    handle_directory_message(&state, &message);

    TEST_ASSERT(endpoint_send_fake.call_count == 1);
}

int main(void) {
    UNITY_BEGIN();

    // for all calls/variations
    //  - test that the functions that are called by them return properly when invoked
    //  - test that the call returns properly when invoked via ipc

    // TODO: test for error conditions when they aren't expected

    // VFS_NEW_PROCESS
    //  - VFS_SHARE_NAMESPACE
    //     - test that the namespace of the new process is the same as its creator
    RUN_TEST(new_process_share_namespace);
    RUN_TEST(new_process_share_namespace_ipc);
    //     - test that read-onlyness is preserved
    RUN_TEST(new_process_share_read_only_namespace);
    RUN_TEST(new_process_share_read_only_namespace_ipc);
    //  - VFS_READ_ONLY_NAMESPACE
    //     - TODO: test that the namespace of the new process cannot be modified with mount/unmount calls once implemented
    RUN_TEST(new_process_read_only_namespace);
    RUN_TEST(new_process_read_only_namespace_ipc);
    //  - no flags
    //     - test that a new namespace is created for this process
    RUN_TEST(new_process_new_namespace);
    RUN_TEST(new_process_new_namespace_ipc);
    // FD_READ
    //  - mount points (not implemented yet)
    //     - should emit one single instance of . and .. each
    RUN_TEST(fd_read_dot_entries);
    RUN_TEST(fd_read_dot_entries_ipc);
    //     - should emit all directory entries for all mounted directories in the order of which they were mounted
    RUN_TEST(fd_read_mount_ordering);
    RUN_TEST(fd_read_mount_ordering_ipc);
    //  - directories
    //     - should pass thru
    RUN_TEST(fd_read_directory_passthru);
    RUN_TEST(fd_read_directory_passthru_ipc);
    // FD_READ_FAST
    //  - same as above, maybe see initrd_fs tests
    // FD_WRITE (should fail! not allowed in directories)
    RUN_TEST(fd_write_should_fail);
    // FD_WRITE_FAST (should fail! not allowed in directories)
    RUN_TEST(fd_write_fast_should_fail);
    // FD_STAT
    //  - mount points
    //     - should be partially passed thru, with some fields overwritten
    RUN_TEST(fd_stat_mount_points);
    RUN_TEST(fd_stat_mount_points_ipc);
    //  - directories
    //     - should be passed thru
    RUN_TEST(fd_stat_directories);
    RUN_TEST(fd_stat_directories_ipc);
    // FD_OPEN
    //  - test directory proxying for mount points
    RUN_TEST(fd_open_mount_point_proxy);
    RUN_TEST(fd_open_mount_point_proxy_ipc);
    //  - test directory proxying for directories
    RUN_TEST(fd_open_directory_proxy);
    RUN_TEST(fd_open_directory_proxy_ipc);
    //  - test that OPEN_CREATE works
    RUN_TEST(fd_open_create);
    //  - test that OPEN_EXCLUSIVE works
    RUN_TEST(fd_open_exclusive);
    //  - test that OPEN_DIRECTORY works
    RUN_TEST(fd_open_directory);
    //  - test non directory passthrough
    RUN_TEST(fd_open_file);
    // FD_LINK
    //  - mount points
    //     - should call FD_LINK in every mounted filesystem with the MOUNT_CREATE flag set
    RUN_TEST(fd_link_in_mount_points);
    RUN_TEST(fd_link_in_mount_points_ipc);
    //  - directories
    //     - should be passed thru
    RUN_TEST(fd_link_in_directories);
    RUN_TEST(fd_link_in_directories_ipc);
    // FD_UNLINK
    //  - mount points
    //     - should call FD_UNLINK in every mounted filesystem until one of the calls returns successfully
    RUN_TEST(fd_unlink_in_mount_points);
    RUN_TEST(fd_unlink_in_mount_points_ipc);
    //  - directories
    //     - should be passed thru
    RUN_TEST(fd_unlink_in_directories);
    RUN_TEST(fd_unlink_in_directories_ipc);
    // FD_TRUNCATE (should fail! not allowed in directories)
    RUN_TEST(fd_truncate_should_fail);
    // FD_MOUNT
    //  - idk yet
    // FD_UNMOUNT
    //  - idk yet

    // TODO: test worker thread usage once implemented

    return UNITY_END();
}
