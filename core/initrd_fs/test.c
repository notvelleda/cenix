#include "errno.h"
#include "fff.h"
#include "jax.h"
#include "main.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sys/kernel.h"
#include "sys/vfs.h"
#include "test_macros.h"
#include "unity.h"
#include "userland_low_level.h"

DEFINE_FFF_GLOBALS;

FAKE_VALUE_FUNC(size_t, endpoint_send, size_t, size_t, struct capability *, size_t);
FAKE_VALUE_FUNC(size_t, endpoint_receive, size_t, size_t, struct capability *, size_t);

void entry_point(size_t initrd_start, size_t initrd_end);

#define ENDPOINT_ADDRESS 10
#define NODE_ADDRESS 11

#define _STRINGIFY(s) #s
#define STRINGIFY(s) _STRINGIFY(s)

struct state state;

void custom_setup(void) {
    RESET_FAKE(endpoint_send);
    RESET_FAKE(endpoint_receive);

    FFF_RESET_HISTORY();

    // allocate capabilities that would otherwise be allocated in _start()
    const struct alloc_args fd_alloc_args = {
        .type = TYPE_ENDPOINT,
        .size = 0,
        .address = ENDPOINT_ADDRESS,
        .depth = -1
    };
    TEST_ASSERT(syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &fd_alloc_args) == 0);

    const struct alloc_args node_alloc_args = {
        .type = TYPE_NODE,
        .size = 3,
        .address = NODE_ADDRESS,
        .depth = INIT_NODE_DEPTH
    };
    TEST_ASSERT(syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &node_alloc_args) == 0);

    // load the test jax archive from disk
    FILE *jax_file = fopen(STRINGIFY(TEST_JAX_LOCATION), "rb");
    TEST_ASSERT(jax_file != NULL);

    fseek(jax_file, 0, SEEK_END);
    long file_size = ftell(jax_file);
    fseek(jax_file, 0, SEEK_SET);

    char *jax_buffer = malloc(file_size);
    TEST_ASSERT(jax_buffer != NULL);

    TEST_ASSERT(fread(jax_buffer, file_size, 1, jax_file) == 1);

    struct jax_iterator iterator;

    // set up state structure
    state = (struct state) {
        .initrd_start = (size_t) jax_buffer,
        .initrd_end = (size_t) jax_buffer + file_size,
        .endpoint = (struct ipc_capability) {ENDPOINT_ADDRESS, -1},
        .node = (struct ipc_capability) {NODE_ADDRESS, INIT_NODE_DEPTH}
    };

    TEST_ASSERT(open_jax(&state.iterator, (const uint8_t *) state.initrd_start, (const uint8_t *) state.initrd_end));
}

void custom_teardown(void) {
    free((void *) state.initrd_start);
}

// utility functions used by tests over handle_ipc_message()

static void list_directory(struct ipc_message *received, struct ipc_message *reply, size_t badge, const char *entry_names[], size_t num_entries) {
    struct vfs_directory_entry *directory_entry = (struct vfs_directory_entry *) FD_READ_FAST_DATA(*reply);

    FD_CALL_NUMBER(*received) = FD_READ_FAST;
    FD_READ_FAST_SIZE(*received) = FD_READ_FAST_MAX_SIZE;
    directory_entry->next_entry_position = 0;

    for (int i = 0; i < num_entries; i ++) {
        received->badge = badge;
        FD_READ_FAST_POSITION(*received) = directory_entry->next_entry_position;

        handle_ipc_message(&state, received, reply);

        TEST_ASSERT(FD_RETURN_VALUE(*reply) == 0);
        TEST_ASSERT(FD_READ_FAST_BYTES_READ(*reply) > sizeof(struct vfs_directory_entry));

        // this isn't the most efficient way to do this but it allows for the name to be printed
        char *name_buffer = alloca(directory_entry->name_length + 1); // alloca is used to hopefully mitigate memory leaks when tests fail
        memcpy(name_buffer, directory_entry->name, directory_entry->name_length);
        name_buffer[directory_entry->name_length] = 0;

#ifdef DEBUG
        printf("inode: %d, next_entry_position: %d, name_length: %d, name: %s\n", directory_entry->inode, directory_entry->next_entry_position, directory_entry->name_length, name_buffer);
#endif

        if (i == num_entries - 1) {
            TEST_ASSERT(directory_entry->next_entry_position == 0);
        } else {
            TEST_ASSERT(directory_entry->next_entry_position != 0);
        }

        TEST_ASSERT(strcmp(entry_names[i], name_buffer) == 0);
    }
}

static size_t open(struct ipc_message *received, struct ipc_message *reply, size_t badge, const char *name, uint8_t mode, uint8_t flags, size_t *new_badge) {
    // allocate a capability to store the name
    const struct alloc_args alloc_args = {
        .type = TYPE_UNTYPED,
        .size = strlen(name) + 1,
        .address = ((IPC_CAPABILITY_SLOTS + 2) << INIT_NODE_DEPTH) | NODE_ADDRESS,
        .depth = -1
    };
    TEST_ASSERT(syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &alloc_args) == 0);

    // copy the name into it
    char *buffer = (char *) syscall_invoke(alloc_args.address, alloc_args.depth, UNTYPED_LOCK, 0);
    TEST_ASSERT(buffer != NULL);

    memcpy(buffer, name, alloc_args.size);

    TEST_ASSERT(syscall_invoke(alloc_args.address, alloc_args.depth, UNTYPED_UNLOCK, 0) == 0);

    // call fd_open
    received->badge = badge;
    FD_CALL_NUMBER(*received) = FD_OPEN;
    FD_OPEN_MODE(*received) = mode;
    FD_OPEN_FLAGS(*received) = flags;
    FD_OPEN_NAME_ADDRESS(*received) = (struct ipc_capability) {alloc_args.address, alloc_args.depth};

    handle_ipc_message(&state, received, reply);

    size_t result = FD_RETURN_VALUE(*reply);

    if (result == 0) {
        // get the badge of the new endpoint
        TEST_ASSERT(read_badge(FD_OPEN_REPLY_FD(*reply).address, FD_OPEN_REPLY_FD(*reply).depth, new_badge) == 0);

#ifdef DEBUG
        printf("got badge %d\n", *new_badge);
#endif
    }

    // delete capabilities so that this function can be called again
    syscall_invoke(NODE_ADDRESS, INIT_NODE_DEPTH, NODE_DELETE, IPC_CAPABILITY_SLOTS + 1); // used in handle_open()
    TEST_ASSERT(syscall_invoke(NODE_ADDRESS, INIT_NODE_DEPTH, NODE_DELETE, IPC_CAPABILITY_SLOTS + 2) == 0);

    return result;
}

static void read_file_fast(struct ipc_message *received, struct ipc_message *reply, size_t badge, size_t position, size_t size, const char *contents) {
    received->badge = badge;
    FD_CALL_NUMBER(*received) = FD_READ_FAST;
    FD_READ_FAST_SIZE(*received) = size > FD_READ_FAST_MAX_SIZE ? FD_READ_FAST_MAX_SIZE : size;
    FD_READ_FAST_POSITION(*received) = position;

    handle_ipc_message(&state, received, reply);

    TEST_ASSERT(FD_RETURN_VALUE(*reply) == 0);

    char *buffer = alloca(FD_READ_FAST_BYTES_READ(*reply) + 1);
    memcpy(buffer, FD_READ_FAST_DATA(*reply), FD_READ_FAST_BYTES_READ(*reply));
    buffer[FD_READ_FAST_BYTES_READ(*reply)] = 0;

#ifdef DEBUG
    printf("file contents (FD_READ_FAST): \"%s\"\n", buffer);
#endif

    TEST_ASSERT(strcmp(buffer, contents) == 0);
}

static void read_file_slow(struct ipc_message *received, struct ipc_message *reply, size_t badge, size_t position, size_t size, const char *contents) {
    size_t contents_length = strlen(contents);

    // allocate a capability for memory to be read into
    const struct alloc_args alloc_args = {
        .type = TYPE_UNTYPED,
        .size = size > contents_length ? size : size + 1, // makes sure memory is allocated for the null terminator
        .address = ((IPC_CAPABILITY_SLOTS + 2) << INIT_NODE_DEPTH) | NODE_ADDRESS,
        .depth = -1
    };
    TEST_ASSERT(syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &alloc_args) == 0);

    // fill the buffer with a set value to check nothing extra was read (this could be its own test but it's easier to just slap it onto all the existing cases)
    uint8_t *buffer = (uint8_t *) syscall_invoke(alloc_args.address, alloc_args.depth, UNTYPED_LOCK, 0);
    TEST_ASSERT(buffer != NULL);

    const uint8_t fill_value = 0xaa;
    memset(buffer, fill_value, alloc_args.size);

    TEST_ASSERT(syscall_invoke(alloc_args.address, alloc_args.depth, UNTYPED_UNLOCK, 0) == 0);

    // do the fd call
    received->badge = badge;
    FD_CALL_NUMBER(*received) = FD_READ;
    FD_READ_SIZE(*received) = size;
    FD_READ_POSITION(*received) = position;
    FD_READ_BUFFER(*received) = (struct ipc_capability) {alloc_args.address, alloc_args.depth};

    handle_ipc_message(&state, received, reply);

    TEST_ASSERT(FD_RETURN_VALUE(*reply) == 0);

    // check that the buffer is correct
    buffer = (uint8_t *) syscall_invoke(alloc_args.address, alloc_args.depth, UNTYPED_LOCK, 0);
    TEST_ASSERT(buffer != NULL);

    TEST_ASSERT(FD_READ_BYTES_READ(*reply) == contents_length);

    TEST_ASSERT(buffer[FD_READ_BYTES_READ(*reply)] == fill_value); // check that the byte that'll be overwritten with a null terminator is correct
    buffer[FD_READ_BYTES_READ(*reply)] = 0;

    for (size_t i = FD_READ_BYTES_READ(*reply) + 1; i < alloc_args.size; i ++) {
        TEST_ASSERT(buffer[i] == fill_value);
    }

#ifdef DEBUG
    printf("file contents (FD_READ): \"%s\"\n", buffer);
#endif

    TEST_ASSERT(strcmp(buffer, contents) == 0);

    TEST_ASSERT(syscall_invoke(alloc_args.address, alloc_args.depth, UNTYPED_UNLOCK, 0) == 0);

    // delete the read buffer
    TEST_ASSERT(syscall_invoke(NODE_ADDRESS, INIT_NODE_DEPTH, NODE_DELETE, IPC_CAPABILITY_SLOTS + 2) == 0);
}

/// convenient wrapper to test both FD_READ and FD_READ_FAST
static void read_file(struct ipc_message *received, struct ipc_message *reply, size_t badge, size_t position, size_t size, const char *contents) {
    read_file_slow(received, reply, badge, position, size, contents);
    read_file_fast(received, reply, badge, position, size, contents);
}

static void stat(struct ipc_message *received, struct ipc_message *reply, size_t badge, struct stat *stat_ptr) {
    received->badge = badge;
    FD_CALL_NUMBER(*received) = FD_STAT;
    handle_ipc_message(&state, received, reply);
    TEST_ASSERT(FD_RETURN_VALUE(*reply) == 0);
    memcpy(stat_ptr, &FD_STAT_STRUCT(*reply), sizeof(struct stat));
}

// test functions to make sure open_jax works properly

void initrd_as_null(void) {
    struct jax_iterator iterator;
    TEST_ASSERT(!open_jax(&iterator, NULL, NULL));
}

void initrd_as_random_data(void) {
    char *random_data = "qwertyuiopasdfghjklzxcvbnm";
    struct jax_iterator iterator;
    TEST_ASSERT(!open_jax(&iterator, (const uint8_t *) random_data, (const uint8_t *) random_data + strlen(random_data)));
}

void initrd_invalid_end(void) {
    char *data = "^jax";
    struct jax_iterator iterator;
    TEST_ASSERT(!open_jax(&iterator, (const uint8_t *) data + strlen(data), (const uint8_t *) data));
}

void initrd_valid_header(void) {
    char *data = "^jax";
    struct jax_iterator iterator;
    TEST_ASSERT(open_jax(&iterator, (const uint8_t *) data, (const uint8_t *) data + strlen(data)));
}

// fs api tests

const char *root_directory_names[] = {"directory1", "directory2", "directory3", "another_directory", "testing.txt"};

void list_root_directory(void) {
    struct ipc_message received, reply;
    list_directory(&received, &reply, 0, root_directory_names, sizeof(root_directory_names) / sizeof(root_directory_names[0]));
}

const char *subdirectory_names[] = {"test.txt"};

void list_subdirectory(void) {
    struct ipc_message received, reply;
    size_t badge;
    TEST_ASSERT(open(&received, &reply, 0, "directory1", MODE_READ, 0, &badge) == 0);

    list_directory(&received, &reply, badge, subdirectory_names, sizeof(subdirectory_names) / sizeof(subdirectory_names[0]));
}

void read_file_in_root(void) {
    struct ipc_message received, reply;
    size_t badge;
    TEST_ASSERT(open(&received, &reply, 0, "testing.txt", MODE_READ, 0, &badge) == 0);

    read_file(&received, &reply, badge, 0, 1024, "this is yet another test.\n");
}

void read_file_in_subdirectory(void) {
    struct ipc_message received, reply;
    size_t badge;
    TEST_ASSERT(open(&received, &reply, 0, "directory1", MODE_READ, 0, &badge) == 0);
    TEST_ASSERT(open(&received, &reply, badge, "test.txt", MODE_READ, 0, &badge) == 0);

    read_file(&received, &reply, badge, 0, 1024, "this is a test.\n");

    TEST_ASSERT(open(&received, &reply, 0, "directory3", MODE_READ, 0, &badge) == 0);
    TEST_ASSERT(open(&received, &reply, badge, "uiop", MODE_READ, 0, &badge) == 0);
    TEST_ASSERT(open(&received, &reply, badge, "owo.txt", MODE_READ, 0, &badge) == 0);

    read_file(&received, &reply, badge, 0, 1024, "OwO\n");
}

void read_position(void) {
    struct ipc_message received, reply;
    size_t badge;
    TEST_ASSERT(open(&received, &reply, 0, "directory3", MODE_READ, 0, &badge) == 0);
    TEST_ASSERT(open(&received, &reply, badge, "uwu.txt", MODE_READ, 0, &badge) == 0);

    read_file(&received, &reply, badge, 0, 1024, "UwU\n");
    read_file(&received, &reply, badge, 1, 1024, "wU\n");
    read_file(&received, &reply, badge, 2, 1024, "U\n");
    read_file(&received, &reply, badge, 3, 1024, "\n");
    read_file(&received, &reply, badge, 4, 1024, "");
    read_file(&received, &reply, badge, 1234567890, 1024, "");
    read_file(&received, &reply, badge, 0xdeadbeef, 1024, "");
    read_file(&received, &reply, badge, -1, 1024, "");
}

void read_less_than_full_file(void) {
    struct ipc_message received, reply;
    size_t badge;

    TEST_ASSERT(open(&received, &reply, 0, "directory1", MODE_READ, 0, &badge) == 0);
    TEST_ASSERT(open(&received, &reply, badge, "test.txt", MODE_READ, 0, &badge) == 0);

    read_file(&received, &reply, badge, 0, 16, "this is a test.\n");
    read_file(&received, &reply, badge, 0, 15, "this is a test.");
    read_file(&received, &reply, badge, 0, 14, "this is a test");
    read_file(&received, &reply, badge, 0, 13, "this is a tes");
    read_file(&received, &reply, badge, 0, 12, "this is a te");
    read_file(&received, &reply, badge, 0, 11, "this is a t");
    read_file(&received, &reply, badge, 0, 10, "this is a ");
    read_file(&received, &reply, badge, 0, 9, "this is a");
    read_file(&received, &reply, badge, 0, 8, "this is ");
    read_file(&received, &reply, badge, 0, 7, "this is");
    read_file(&received, &reply, badge, 0, 6, "this i");
    read_file(&received, &reply, badge, 0, 5, "this ");
    read_file(&received, &reply, badge, 0, 4, "this");
    read_file(&received, &reply, badge, 0, 3, "thi");
    read_file(&received, &reply, badge, 0, 2, "th");
    read_file(&received, &reply, badge, 0, 1, "t");
    read_file(&received, &reply, badge, 0, 0, "");

    TEST_ASSERT(open(&received, &reply, 0, "directory3", MODE_READ, 0, &badge) == 0);
    TEST_ASSERT(open(&received, &reply, badge, "uiop", MODE_READ, 0, &badge) == 0);
    TEST_ASSERT(open(&received, &reply, badge, "owo.txt", MODE_READ, 0, &badge) == 0);

    read_file(&received, &reply, badge, 0, 4, "OwO\n");
    read_file(&received, &reply, badge, 0, 3, "OwO");
    read_file(&received, &reply, badge, 0, 2, "Ow");
    read_file(&received, &reply, badge, 0, 1, "O");
    read_file(&received, &reply, badge, 0, 0, "");

    read_file(&received, &reply, badge, 1, 3, "wO\n");
    read_file(&received, &reply, badge, 2, 2, "O\n");
    read_file(&received, &reply, badge, 3, 1, "\n");
    read_file(&received, &reply, badge, 4, 0, "");
}

void read_invalid_directory_entry(void) {
    struct ipc_message received, reply;

    received.badge = 0;
    FD_CALL_NUMBER(received) = FD_READ_FAST;
    FD_READ_FAST_SIZE(received) = FD_READ_FAST_MAX_SIZE;
    FD_READ_FAST_POSITION(received) = 1;
    handle_ipc_message(&state, &received, &reply);
    TEST_ASSERT(FD_RETURN_VALUE(reply) == EINVAL);

    received.badge = 0;
    FD_CALL_NUMBER(received) = FD_READ_FAST;
    FD_READ_FAST_SIZE(received) = FD_READ_FAST_MAX_SIZE;
    FD_READ_FAST_POSITION(received) = 1234567890;
    handle_ipc_message(&state, &received, &reply);
    TEST_ASSERT(FD_RETURN_VALUE(reply) == EINVAL);

    received.badge = 0;
    FD_CALL_NUMBER(received) = FD_READ_FAST;
    FD_READ_FAST_SIZE(received) = FD_READ_FAST_MAX_SIZE;
    FD_READ_FAST_POSITION(received) = -1;
    handle_ipc_message(&state, &received, &reply);
    TEST_ASSERT(FD_RETURN_VALUE(reply) == EINVAL);

    received.badge = 0;
    FD_CALL_NUMBER(received) = FD_READ_FAST;
    FD_READ_FAST_SIZE(received) = FD_READ_FAST_MAX_SIZE;
    FD_READ_FAST_POSITION(received) = state.initrd_start;
    handle_ipc_message(&state, &received, &reply);
    TEST_ASSERT(FD_RETURN_VALUE(reply) == EINVAL);

    received.badge = 0;
    FD_CALL_NUMBER(received) = FD_READ_FAST;
    FD_READ_FAST_SIZE(received) = FD_READ_FAST_MAX_SIZE;
    FD_READ_FAST_POSITION(received) = state.initrd_end;
    handle_ipc_message(&state, &received, &reply);
    TEST_ASSERT(FD_RETURN_VALUE(reply) == EINVAL);

    received.badge = 0;
    FD_CALL_NUMBER(received) = FD_READ_FAST;
    FD_READ_FAST_SIZE(received) = FD_READ_FAST_MAX_SIZE;
    FD_READ_FAST_POSITION(received) = state.initrd_end - 1;
    handle_ipc_message(&state, &received, &reply);
    TEST_ASSERT(FD_RETURN_VALUE(reply) == EINVAL);
}

void open_nonexistent_file(void) {
    struct ipc_message received, reply;
    size_t badge;
    TEST_ASSERT(open(&received, &reply, 0, "thisdoesntexist", MODE_READ, 0, &badge) == ENOENT);
    TEST_ASSERT(open(&received, &reply, 0, "directory4", MODE_READ, 0, &badge) == ENOENT);
}

void open_nonexistent_file_in_subdirectory(void) {
    struct ipc_message received, reply;
    size_t badge;
    TEST_ASSERT(open(&received, &reply, 0, "directory2", MODE_READ, 0, &badge) == 0);
    TEST_ASSERT(open(&received, &reply, badge, "thisdoesntexist", MODE_READ, 0, &badge) == ENOENT);
}

void open_with_write(void) {
    struct ipc_message received, reply;
    size_t badge;
    TEST_ASSERT(open(&received, &reply, 0, "testing.txt", MODE_WRITE, 0, &badge) == EROFS);
    TEST_ASSERT(open(&received, &reply, 0, "testing.txt", MODE_READ | MODE_WRITE, 0, &badge) == EROFS);
}

void open_with_append(void) {
    struct ipc_message received, reply;
    size_t badge;
    TEST_ASSERT(open(&received, &reply, 0, "testing.txt", MODE_APPEND, 0, &badge) == EROFS);
}

void open_with_create(void) {
    struct ipc_message received, reply;
    size_t badge;
    TEST_ASSERT(open(&received, &reply, 0, "testing.txt", MODE_READ, OPEN_CREATE, &badge) == 0);
    TEST_ASSERT(open(&received, &reply, 0, "this-file-does-not-exist.txt", MODE_READ, OPEN_CREATE, &badge) == EROFS);
}

void open_with_exclusive(void) {
    struct ipc_message received, reply;
    size_t badge;
    TEST_ASSERT(open(&received, &reply, 0, "testing.txt", MODE_READ, OPEN_CREATE | OPEN_EXCLUSIVE, &badge) == EROFS);
}

// FD_STAT would be tested here but due to native libc types being different it won't work

// function mockups for the ipc interface test

size_t fd_open_badge;

size_t endpoint_success_fake(size_t address, size_t depth, struct capability *slot, size_t argument) {
    struct ipc_message *message = (struct ipc_message *) argument;

    // just hardcoded to delete this endpoint since it isn't copied
    syscall_invoke(3, INIT_NODE_DEPTH, NODE_DELETE, IPC_CAPABILITY_SLOTS + 1);

    return 0;
}

size_t endpoint_error_fake(size_t address, size_t depth, struct capability *slot, size_t argument) {
    return EUNKNOWN;
}

size_t vfs_call_success_fake(size_t address, size_t depth, struct capability *slot, size_t argument) {
    struct ipc_message *message = (struct ipc_message *) argument;

    message->transferred_capabilities = 0;
    FD_RETURN_VALUE(*message) = 0;

    return endpoint_success_fake(address, depth, slot, argument);
}

size_t fd_open_request_fake(size_t address, size_t depth, struct capability *slot, size_t argument) {
    struct ipc_message *message = (struct ipc_message *) argument;

    message->badge = 0;
    FD_CALL_NUMBER(*message) = FD_OPEN;
    FD_OPEN_MODE(*message) = MODE_READ;
    FD_OPEN_FLAGS(*message) = 0;
    const char *name = "testing.txt";

    // allocate the reply endpoint
    const struct alloc_args reply_alloc_args = {
        .type = TYPE_ENDPOINT,
        .size = 0,
        .address = FD_REPLY_ENDPOINT(*message).address,
        .depth = FD_REPLY_ENDPOINT(*message).depth
    };
    TEST_ASSERT(syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &reply_alloc_args) == 0);

    // allocate a capability to store the name
    const struct alloc_args name_alloc_args = {
        .type = TYPE_UNTYPED,
        .size = strlen(name) + 1,
        .address = FD_OPEN_NAME_ADDRESS(*message).address,
        .depth = FD_OPEN_NAME_ADDRESS(*message).depth
    };
    TEST_ASSERT(syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &name_alloc_args) == 0);

    // copy the name into it
    char *buffer = (char *) syscall_invoke(name_alloc_args.address, name_alloc_args.depth, UNTYPED_LOCK, 0);
    TEST_ASSERT(buffer != NULL);

    memcpy(buffer, name, name_alloc_args.size);

    TEST_ASSERT(syscall_invoke(name_alloc_args.address, name_alloc_args.depth, UNTYPED_UNLOCK, 0) == 0);

    message->transferred_capabilities = 3;

    return 0;
}

size_t fd_open_response_fake(size_t address, size_t depth, struct capability *slot, size_t argument) {
    struct ipc_message *message = (struct ipc_message *) argument;

    TEST_ASSERT(FD_RETURN_VALUE(*message) == 0);

    TEST_ASSERT(read_badge(FD_OPEN_REPLY_FD(*message).address, FD_OPEN_REPLY_FD(*message).depth, &fd_open_badge) == 0);

#ifdef DEBUG
    printf("got badge %d\n", fd_open_badge);
#endif

    return 0;
}

size_t fd_read_request_fake(size_t address, size_t depth, struct capability *slot, size_t argument) {
    struct ipc_message *message = (struct ipc_message *) argument;

    message->badge = fd_open_badge;
    FD_CALL_NUMBER(*message) = FD_READ_FAST;
    FD_READ_FAST_SIZE(*message) = FD_READ_FAST_MAX_SIZE;
    FD_READ_FAST_POSITION(*message) = 0;

    // allocate the reply endpoint
    const struct alloc_args reply_alloc_args = {
        .type = TYPE_ENDPOINT,
        .size = 0,
        .address = FD_REPLY_ENDPOINT(*message).address,
        .depth = FD_REPLY_ENDPOINT(*message).depth
    };
    TEST_ASSERT(syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &reply_alloc_args) == 0);

    message->transferred_capabilities = 1;

    return 0;
}

size_t fd_read_response_fake(size_t address, size_t depth, struct capability *slot, size_t argument) {
    struct ipc_message *message = (struct ipc_message *) argument;

    TEST_ASSERT(FD_RETURN_VALUE(*message) == 0);

    char *buffer = alloca(FD_READ_FAST_BYTES_READ(*message) + 1);
    memcpy(buffer, FD_READ_FAST_DATA(*message), FD_READ_FAST_BYTES_READ(*message));
    buffer[FD_READ_FAST_BYTES_READ(*message)] = 0;

#ifdef DEBUG
    printf("bytes read: %d, file contents: \"%s\"\n", FD_READ_FAST_BYTES_READ(*message), buffer);
#endif

    TEST_ASSERT(strcmp(buffer, "this is yet another test.\n") == 0);

    return 0;
}

// basic test to make sure the actual ipc interface works
void test_ipc_interface(void) {
    // allocate the vfs endpoint
    const struct alloc_args endpoint_alloc_args = {
        .type = TYPE_ENDPOINT,
        .size = 0,
        .address = 2,
        .depth = -1
    };
    TEST_ASSERT(syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &endpoint_alloc_args) == 0);

    // hacky fix to get vfs_open_root working here before it's removed
    const struct alloc_args endpoint_alloc_args_2 = {
        .type = TYPE_ENDPOINT,
        .size = 0,
        .address = 6,
        .depth = -1
    };
    TEST_ASSERT(syscall_invoke(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &endpoint_alloc_args_2) == 0);

    size_t (*send_fakes[])(size_t, size_t, struct capability *, size_t) = {
        endpoint_success_fake, // vfs_open_root
        endpoint_success_fake, // fd_mount
        fd_open_response_fake,
        fd_read_response_fake
    };
    SET_CUSTOM_FAKE_SEQ(endpoint_send, send_fakes, sizeof(send_fakes) / sizeof(send_fakes[0]));

    size_t (*receive_fakes[])(size_t, size_t, struct capability *, size_t) = {
        vfs_call_success_fake, // vfs_open_root
        vfs_call_success_fake, // fd_mount
        fd_open_request_fake,
        fd_read_request_fake,
        endpoint_error_fake
    };
    SET_CUSTOM_FAKE_SEQ(endpoint_receive, receive_fakes, sizeof(receive_fakes) / sizeof(receive_fakes[0]));

    entry_point(state.initrd_start, state.initrd_end);

    TEST_ASSERT(endpoint_send_fake.call_count == sizeof(send_fakes) / sizeof(send_fakes[0]));
    TEST_ASSERT(endpoint_receive_fake.call_count == sizeof(receive_fakes) / sizeof(receive_fakes[0]));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(initrd_as_null);
    RUN_TEST(initrd_as_random_data);
    RUN_TEST(initrd_invalid_end);
    RUN_TEST(initrd_valid_header);
    RUN_TEST(list_root_directory);
    RUN_TEST(list_subdirectory);
    RUN_TEST(read_file_in_root);
    RUN_TEST(read_file_in_subdirectory);
    RUN_TEST(read_position);
    RUN_TEST(read_less_than_full_file);
    RUN_TEST(read_invalid_directory_entry);
    RUN_TEST(open_nonexistent_file);
    RUN_TEST(open_nonexistent_file_in_subdirectory);
    RUN_TEST(open_with_write);
    RUN_TEST(open_with_append);
    RUN_TEST(open_with_create);
    RUN_TEST(open_with_exclusive);
    RUN_TEST(test_ipc_interface);
    return UNITY_END();
}
