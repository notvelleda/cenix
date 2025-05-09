#include "fff.h"
#include "ipc.h"
#include "namespaces.h"
#include "structures.h"
#include "sys/kernel.h"
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
    return 0;
}

size_t endpoint_error_fake(size_t address, size_t depth, struct capability *slot, size_t argument) {
    return EUNKNOWN;
}

size_t badge_values[2];

size_t new_process_response_fake(size_t address, size_t depth, struct capability *slot, size_t argument) {
    struct ipc_message *message = (struct ipc_message *) argument;

    badge_values[0] = badge_values[1];
    TEST_ASSERT(read_badge(message->capabilities[0].address, message->capabilities[0].depth, &badge_values[1]) == 0);

#ifdef DEBUG
    printf("got badge %d\n", badge_values[1]);
#endif

    return 0;
}

void custom_setup(void) {
    RESET_FAKE(endpoint_send);
    RESET_FAKE(endpoint_receive);

    init_vfs_structures();

    const struct alloc_args endpoint_alloc_args = {
        .type = TYPE_ENDPOINT,
        .size = 0,
        .address = 3,
        .depth = -1
    };
    TEST_ASSERT(syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &endpoint_alloc_args) == 0);

    state = (struct state) {
        .thread_id = 0,
        .temp_slot = IPC_CAPABILITY_SLOTS + 1,
        .endpoint_address = endpoint_alloc_args.address
    };

    const struct alloc_args reply_alloc_args = {
        .type = TYPE_ENDPOINT,
        .size = 0,
        .address = THREAD_STORAGE_SLOT(state.thread_id, 0),
        .depth = -1
    };
    TEST_ASSERT(syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &reply_alloc_args) == 0);

    endpoint_send_fake.custom_fake = new_process_response_fake;
    TEST_ASSERT(set_up_filesystem_for_process(&state, 1, 1, 0, reply_alloc_args.address) == 0);

    syscall_invoke(THREAD_STORAGE_ADDRESS(state.thread_id), THREAD_STORAGE_DEPTH, NODE_DELETE, 0);

    RESET_FAKE(endpoint_send);
    FFF_RESET_HISTORY();
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
        .depth = -1
    };
    syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &reply_alloc_args);

    endpoint_send_fake.custom_fake = new_process_response_fake;
    TEST_ASSERT(set_up_filesystem_for_process(&state, 1, 2, VFS_SHARE_NAMESPACE, reply_alloc_args.address) == 0);

#ifdef DEBUG
    printf("pid 1: flags: 0x%lx, id: 0x%lx\n", IPC_FLAGS(badge_values[0]), IPC_ID(badge_values[0]));
    printf("pid 2: flags: 0x%lx, id: 0x%lx\n", IPC_FLAGS(badge_values[1]), IPC_ID(badge_values[1]));
#endif

    TEST_ASSERT(IPC_ID(badge_values[0]) == IPC_ID(badge_values[1]));
    TEST_ASSERT(IPC_FLAGS(badge_values[0]) == IPC_FLAGS(badge_values[1]));
    TEST_ASSERT(IPC_FLAGS(badge_values[1]) == IPC_FLAG_CAN_MODIFY);
}

size_t new_process_pid_2_share_fake(size_t address, size_t depth, struct capability *slot, size_t argument) {
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

    const struct alloc_args reply_alloc_args = {
        .type = TYPE_ENDPOINT,
        .size = 0,
        .address = message->capabilities[0].address,
        .depth = message->capabilities[0].depth
    };
    TEST_ASSERT(syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &reply_alloc_args) == 0);
    message->transferred_capabilities = 1;

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

#ifdef DEBUG
    printf("pid 1: flags: 0x%lx, id: 0x%lx\n", IPC_FLAGS(badge_values[0]), IPC_ID(badge_values[0]));
    printf("pid 2: flags: 0x%lx, id: 0x%lx\n", IPC_FLAGS(badge_values[1]), IPC_ID(badge_values[1]));
#endif

    TEST_ASSERT(endpoint_send_fake.call_count == 1); // called once per call, 1 call in this test
    TEST_ASSERT(endpoint_receive_fake.call_count == 2); // called once per call plus an extra at the end to break

    TEST_ASSERT(IPC_ID(badge_values[0]) == IPC_ID(badge_values[1]));
    TEST_ASSERT(IPC_FLAGS(badge_values[0]) == IPC_FLAGS(badge_values[1]));
    TEST_ASSERT(IPC_FLAGS(badge_values[1]) == IPC_FLAG_CAN_MODIFY);
}

// (VFS_NEW_PROCESS + VFS_SHARE_NAMESPACE) test that read-onlyness is preserved
void new_process_share_read_only_namespace(void) {
    const struct alloc_args reply_alloc_args = {
        .type = TYPE_ENDPOINT,
        .size = 0,
        .address = THREAD_STORAGE_SLOT(state.thread_id, 0),
        .depth = -1
    };
    syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &reply_alloc_args);

    endpoint_send_fake.custom_fake = new_process_response_fake;
    TEST_ASSERT(set_up_filesystem_for_process(&state, 1, 2, VFS_SHARE_NAMESPACE | VFS_READ_ONLY_NAMESPACE, reply_alloc_args.address) == 0);

    endpoint_send_fake.custom_fake = new_process_response_fake;
    TEST_ASSERT(set_up_filesystem_for_process(&state, 2, 3, VFS_SHARE_NAMESPACE, reply_alloc_args.address) == 0);

#ifdef DEBUG
    printf("pid 2: flags: 0x%lx, id: 0x%lx\n", IPC_FLAGS(badge_values[0]), IPC_ID(badge_values[0]));
    printf("pid 3: flags: 0x%lx, id: 0x%lx\n", IPC_FLAGS(badge_values[1]), IPC_ID(badge_values[1]));
#endif

    TEST_ASSERT(IPC_ID(badge_values[0]) == IPC_ID(badge_values[1]));
    TEST_ASSERT(IPC_FLAGS(badge_values[0]) == IPC_FLAGS(badge_values[1]));
    TEST_ASSERT(IPC_FLAGS(badge_values[1]) == 0);
}

size_t new_process_pid_2_read_only_fake(size_t address, size_t depth, struct capability *slot, size_t argument) {
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

    const struct alloc_args reply_alloc_args = {
        .type = TYPE_ENDPOINT,
        .size = 0,
        .address = message->capabilities[0].address,
        .depth = message->capabilities[0].depth
    };
    TEST_ASSERT(syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &reply_alloc_args) == 0);
    message->transferred_capabilities = 1;

    return 0;
}

size_t new_process_pid_3_share_fake(size_t address, size_t depth, struct capability *slot, size_t argument) {
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

    const struct alloc_args reply_alloc_args = {
        .type = TYPE_ENDPOINT,
        .size = 0,
        .address = message->capabilities[0].address,
        .depth = message->capabilities[0].depth
    };
    TEST_ASSERT(syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &reply_alloc_args) == 0);
    message->transferred_capabilities = 1;

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

#ifdef DEBUG
    printf("pid 2: flags: 0x%lx, id: 0x%lx\n", IPC_FLAGS(badge_values[0]), IPC_ID(badge_values[0]));
    printf("pid 3: flags: 0x%lx, id: 0x%lx\n", IPC_FLAGS(badge_values[1]), IPC_ID(badge_values[1]));
#endif

    TEST_ASSERT(endpoint_send_fake.call_count == 2); // called once per call, 2 calls in this test
    TEST_ASSERT(endpoint_receive_fake.call_count == 3); // called once per call plus an extra at the end to break

    TEST_ASSERT(IPC_ID(badge_values[0]) == IPC_ID(badge_values[1]));
    TEST_ASSERT(IPC_FLAGS(badge_values[0]) == IPC_FLAGS(badge_values[1]));
    TEST_ASSERT(IPC_FLAGS(badge_values[1]) == 0);
}

// (VFS_NEW_PROCESS + VFS_READ_ONLY_NAMESPACE) test that the namespace of the new process cannot be modified with mount/unmount calls
void new_process_read_only_namespace(void) {
    // TODO: implement this once mounting has been reworked
    TEST_FAIL_MESSAGE("TODO");
}

void new_process_read_only_namespace_ipc(void) {
    TEST_FAIL_MESSAGE("TODO");
}

// (VFS_NEW_PROCESS + no flags) test that a new namespace is created for this process
void new_process_new_namespace(void) {
    const struct alloc_args reply_alloc_args = {
        .type = TYPE_ENDPOINT,
        .size = 0,
        .address = THREAD_STORAGE_SLOT(state.thread_id, 0),
        .depth = -1
    };
    syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &reply_alloc_args);

    endpoint_send_fake.custom_fake = new_process_response_fake;
    TEST_ASSERT(set_up_filesystem_for_process(&state, 1, 2, 0, reply_alloc_args.address) == 0);

#ifdef DEBUG
    printf("pid 1: flags: 0x%lx, id: 0x%lx\n", IPC_FLAGS(badge_values[0]), IPC_ID(badge_values[0]));
    printf("pid 2: flags: 0x%lx, id: 0x%lx\n", IPC_FLAGS(badge_values[1]), IPC_ID(badge_values[1]));
#endif

    TEST_ASSERT(IPC_ID(badge_values[0]) == 0);
    TEST_ASSERT(IPC_ID(badge_values[1]) == 1);
    TEST_ASSERT(IPC_FLAGS(badge_values[0]) == IPC_FLAGS(badge_values[1]));
    TEST_ASSERT(IPC_FLAGS(badge_values[1]) == IPC_FLAG_CAN_MODIFY);
}

size_t new_process_pid_2_new_namespace_fake(size_t address, size_t depth, struct capability *slot, size_t argument) {
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

    const struct alloc_args reply_alloc_args = {
        .type = TYPE_ENDPOINT,
        .size = 0,
        .address = message->capabilities[0].address,
        .depth = message->capabilities[0].depth
    };
    TEST_ASSERT(syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &reply_alloc_args) == 0);
    message->transferred_capabilities = 1;

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

#ifdef DEBUG
    printf("pid 1: flags: 0x%lx, id: 0x%lx\n", IPC_FLAGS(badge_values[0]), IPC_ID(badge_values[0]));
    printf("pid 2: flags: 0x%lx, id: 0x%lx\n", IPC_FLAGS(badge_values[1]), IPC_ID(badge_values[1]));
#endif

    TEST_ASSERT(endpoint_send_fake.call_count == 1); // called once per call, 1 call in this test
    TEST_ASSERT(endpoint_receive_fake.call_count == 2); // called once per call plus an extra at the end to break

    TEST_ASSERT(IPC_ID(badge_values[0]) == 0);
    TEST_ASSERT(IPC_ID(badge_values[1]) == 1);
    TEST_ASSERT(IPC_FLAGS(badge_values[0]) == IPC_FLAGS(badge_values[1]));
    TEST_ASSERT(IPC_FLAGS(badge_values[1]) == IPC_FLAG_CAN_MODIFY);
}

// ====================================================================================================
// FD_READ tests
// ====================================================================================================

// (FD_READ) mount points should emit one single instance of . and .. each
void fd_read_dot_entries(void) {
    TEST_FAIL_MESSAGE("TODO");
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

// (FD_READ) directories should pass thru
void fd_read_directory_passthru(void) {
    TEST_FAIL_MESSAGE("TODO");
}

void fd_read_directory_passthru_ipc(void) {
    TEST_FAIL_MESSAGE("TODO");
}

// ====================================================================================================
// FD_WRITE tests
// ====================================================================================================

// (FD_WRITE) should fail! not allowed in directories
void fd_write_should_fail(void) {
    TEST_FAIL_MESSAGE("TODO");
}

// (FD_WRITE_FAST) should fail! not allowed in directories
void fd_write_fast_should_fail(void) {
    TEST_FAIL_MESSAGE("TODO");
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

// (FD_LINK) should call FD_LINK in every mounted filesystem with the MCREATE flag set
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
    TEST_FAIL_MESSAGE("TODO");
}

int main(void) {
    UNITY_BEGIN();

    // should VFS_OPEN_ROOT/VFS_MOUNT/VFS_UNMOUNT be tested if that part of the api is gonna be reworked?

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
    //     - should call FD_LINK in every mounted filesystem with the MCREATE flag set
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

    // TODO: test worker thread usage once implemented

    return UNITY_END();
}
