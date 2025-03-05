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

    const struct alloc_args fd_alloc_args = {
        .type = TYPE_ENDPOINT,
        .size = 0,
        .address = ENDPOINT_ADDRESS,
        .depth = -1
    };
    INVOKE_ASSERT(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &fd_alloc_args);

    const struct alloc_args node_alloc_args = {
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

static size_t open(struct ipc_message *received, struct ipc_message *reply, size_t badge, const char *name, size_t *new_badge) {
    // allocate a capability to store the name
    const struct alloc_args alloc_args = {
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

    size_t result = FD_RETURN_VALUE(*reply);

    if (result == 0) {
        // get the badge of the new endpoint
        TEST_ASSERT(read_badge(FD_OPEN_REPLY_FD(*reply).address, FD_OPEN_REPLY_FD(*reply).depth, new_badge) == 0);
    }

    // delete capabilities so that this function can be called again
    syscall_invoke(NODE_ADDRESS, INIT_NODE_DEPTH, NODE_DELETE, IPC_CAPABILITY_SLOTS + 1); // used in handle_open()
    INVOKE_ASSERT(NODE_ADDRESS, INIT_NODE_DEPTH, NODE_DELETE, IPC_CAPABILITY_SLOTS + 2);

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
    INVOKE_ASSERT(0, -1, ADDRESS_SPACE_ALLOC, (size_t) &alloc_args);

    // fill the buffer with a set value to check nothing extra was read (this could be its own test but it's easier to just slap it onto all the existing cases)
    uint8_t *buffer = (uint8_t *) syscall_invoke(alloc_args.address, alloc_args.depth, UNTYPED_LOCK, 0);
    TEST_ASSERT(buffer != NULL);

    const uint8_t fill_value = 0xaa;
    memset(buffer, fill_value, alloc_args.size);

    INVOKE_ASSERT(alloc_args.address, alloc_args.depth, UNTYPED_UNLOCK, 0);

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

    INVOKE_ASSERT(alloc_args.address, alloc_args.depth, UNTYPED_UNLOCK, 0);

    // delete the read buffer
    INVOKE_ASSERT(NODE_ADDRESS, INIT_NODE_DEPTH, NODE_DELETE, IPC_CAPABILITY_SLOTS + 2);
}

// convenient wrapper to test both FD_READ and FD_READ_FAST
static void read_file(struct ipc_message *received, struct ipc_message *reply, size_t badge, size_t position, size_t size, const char *contents) {
    read_file_slow(received, reply, badge, position, size, contents);
    read_file_fast(received, reply, badge, position, size, contents);
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
    TEST_ASSERT(open(&received, &reply, 0, "directory1", &badge) == 0);

    list_directory(&received, &reply, badge, subdirectory_names, sizeof(subdirectory_names) / sizeof(subdirectory_names[0]));
}

void read_file_in_root(void) {
    struct ipc_message received, reply;
    size_t badge;
    TEST_ASSERT(open(&received, &reply, 0, "testing.txt", &badge) == 0);

    read_file(&received, &reply, badge, 0, 1024, "this is yet another test.\n");
}

void read_file_in_subdirectory(void) {
    struct ipc_message received, reply;
    size_t badge;
    TEST_ASSERT(open(&received, &reply, 0, "directory1", &badge) == 0);
    TEST_ASSERT(open(&received, &reply, badge, "test.txt", &badge) == 0);

    read_file(&received, &reply, badge, 0, 1024, "this is a test.\n");

    TEST_ASSERT(open(&received, &reply, 0, "directory3", &badge) == 0);
    TEST_ASSERT(open(&received, &reply, badge, "uiop", &badge) == 0);
    TEST_ASSERT(open(&received, &reply, badge, "owo.txt", &badge) == 0);

    read_file(&received, &reply, badge, 0, 1024, "OwO\n");
}

void open_nonexistent_file(void) {
    struct ipc_message received, reply;
    size_t badge;
    TEST_ASSERT(open(&received, &reply, 0, "thisdoesntexist", &badge) == ENOENT);
    TEST_ASSERT(open(&received, &reply, 0, "directory4", &badge) == ENOENT);
}

void open_nonexistent_file_in_subdirectory(void) {
    struct ipc_message received, reply;
    size_t badge;
    TEST_ASSERT(open(&received, &reply, 0, "directory2", &badge) == 0);
    TEST_ASSERT(open(&received, &reply, badge, "thisdoesntexist", &badge) == ENOENT);
}

void read_position(void) {
    struct ipc_message received, reply;
    size_t badge;
    TEST_ASSERT(open(&received, &reply, 0, "directory3", &badge) == 0);
    TEST_ASSERT(open(&received, &reply, badge, "uwu.txt", &badge) == 0);

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
    TEST_ASSERT(open(&received, &reply, 0, "directory1", &badge) == 0);
    TEST_ASSERT(open(&received, &reply, badge, "test.txt", &badge) == 0);

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

    TEST_ASSERT(open(&received, &reply, 0, "directory3", &badge) == 0);
    TEST_ASSERT(open(&received, &reply, badge, "uiop", &badge) == 0);
    TEST_ASSERT(open(&received, &reply, badge, "owo.txt", &badge) == 0);

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
    RUN_TEST(open_nonexistent_file);
    RUN_TEST(open_nonexistent_file_in_subdirectory);
    RUN_TEST(read_position);
    RUN_TEST(read_less_than_full_file);
    return UNITY_END();
}
