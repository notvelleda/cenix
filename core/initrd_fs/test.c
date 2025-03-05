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

FAKE_VOID_FUNC(syscall_yield);

void entry_point(size_t initrd_start, size_t initrd_end);

#define ENDPOINT_ADDRESS 3
#define NODE_ADDRESS 4
#define FREE_SLOTS_START 4

#define _STRINGIFY(s) #s
#define STRINGIFY(s) _STRINGIFY(s)

struct state state;

void custom_setup(void) {
    RESET_FAKE(syscall_yield);

    FFF_RESET_HISTORY();

    struct alloc_args fd_alloc_args = {
        .type = TYPE_ENDPOINT,
        .size = 0,
        .address = ENDPOINT_ADDRESS,
        .depth = -1
    };
    INVOKE_ASSERT(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &fd_alloc_args);

    struct alloc_args node_alloc_args = {
        .type = TYPE_NODE,
        .size = 3,
        .address = NODE_ADDRESS,
        .depth = INIT_NODE_DEPTH
    };
    INVOKE_ASSERT(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &node_alloc_args);

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

// test utility functions

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

static size_t open(struct ipc_message *received, struct ipc_message *reply, size_t badge, const char *name) {
    // allocate a capability to store the name
    struct alloc_args alloc_args = {
        .type = TYPE_UNTYPED,
        .size = strlen(name) + 1,
        .address = ((IPC_CAPABILITY_SLOTS + 2) << INIT_NODE_DEPTH) | NODE_ADDRESS,
        .depth = -1
    };
    INVOKE_ASSERT(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &alloc_args);

    // copy the name into it
    char *buffer = (char *) syscall_invoke(alloc_args.address, alloc_args.depth, UNTYPED_LOCK, 0);
    TEST_ASSERT(buffer != NULL);

    memcpy(buffer, name, alloc_args.size);

    INVOKE_ASSERT(alloc_args.address, alloc_args.depth, UNTYPED_UNLOCK, 0);

    // call fd_open
    received->badge = badge;
    FD_CALL_NUMBER(*received) = FD_OPEN;
    FD_OPEN_MODE(*received) = MODE_READ;
    FD_OPEN_NAME_ADDRESS(*received) = (struct ipc_capability) {alloc_args.address, alloc_args.depth};

    handle_ipc_message(&state, received, reply);

    TEST_ASSERT(FD_RETURN_VALUE(*reply) == 0);

    // make sure directory entries match up
    size_t new_badge;
    TEST_ASSERT(read_badge(FD_OPEN_REPLY_FD(*reply).address, FD_OPEN_REPLY_FD(*reply).depth, &new_badge) == 0);

    // delete capabilities so that this function can be called again
    INVOKE_ASSERT(NODE_ADDRESS, INIT_NODE_DEPTH, NODE_DELETE, IPC_CAPABILITY_SLOTS + 1); // used in handle_open()
    INVOKE_ASSERT(NODE_ADDRESS, INIT_NODE_DEPTH, NODE_DELETE, IPC_CAPABILITY_SLOTS + 2);

    return new_badge;
}

static void read_file(struct ipc_message *received, struct ipc_message *reply, size_t badge, const char *contents) {
    received->badge = badge;
    FD_CALL_NUMBER(*received) = FD_READ_FAST;
    FD_READ_FAST_SIZE(*received) = FD_READ_FAST_MAX_SIZE;
    FD_READ_FAST_POSITION(*received) = 0;

    handle_ipc_message(&state, received, reply);

    TEST_ASSERT(FD_RETURN_VALUE(*reply) == 0);

    char *buffer = alloca(FD_READ_FAST_BYTES_READ(*reply) + 1);
    memcpy(buffer, FD_READ_FAST_DATA(*reply), FD_READ_FAST_BYTES_READ(*reply));
    buffer[FD_READ_FAST_BYTES_READ(*reply)] = 0;

#ifdef DEBUG
    printf("file contents: \"%s\"\n", buffer);
#endif

    TEST_ASSERT(strcmp(buffer, contents) == 0);
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
    size_t badge = open(&received, &reply, 0, "directory1");

    list_directory(&received, &reply, badge, subdirectory_names, sizeof(subdirectory_names) / sizeof(subdirectory_names[0]));
}

void read_file_in_root(void) {
    struct ipc_message received, reply;
    size_t badge = open(&received, &reply, 0, "testing.txt");

    read_file(&received, &reply, badge, "this is yet another test.\n");
}

void read_file_in_subdirectory(void) {
    struct ipc_message received, reply;
    size_t badge = open(&received, &reply, 0, "directory1");
    badge = open(&received, &reply, badge, "test.txt");

    read_file(&received, &reply, badge, "this is a test.\n");
}

void read_file_in_subdirectory_2(void) {
    struct ipc_message received, reply;
    size_t badge = open(&received, &reply, 0, "directory3");
    badge = open(&received, &reply, badge, "uiop");
    badge = open(&received, &reply, badge, "owo.txt");

    read_file(&received, &reply, badge, "OwO\n");
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
    RUN_TEST(read_file_in_subdirectory_2);
    return UNITY_END();
}
