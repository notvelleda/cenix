#include "capabilities_layout.h"
#include "structures.h"
#include <stdint.h>
#include "sys/kernel.h"
#include "utils.h"

void return_value(size_t reply_capability, size_t error_code) {
    struct ipc_message reply = {
        .capabilities = {}
    };
    *(size_t *) &reply.buffer = error_code;

    syscall_invoke(reply_capability, SIZE_MAX, ENDPOINT_SEND, (size_t) &reply);
}

size_t badge_and_send(const struct state *state, size_t badge, size_t reply_endpoint_address) {
    const struct node_copy_args copy_args = {
        .source_address = state->endpoint_address,
        .source_depth = SIZE_MAX,
        .dest_slot = state->temp_slot,
        .access_rights = UINT8_MAX, // TODO: set access rights so that other processes can't listen on this endpoint
        .badge = badge,
        .should_set_badge = 1
    };
    size_t result = syscall_invoke(THREAD_STORAGE_ADDRESS(state->thread_id), THREAD_STORAGE_DEPTH, NODE_COPY, (size_t) &copy_args);

    if (result != 0) {
        return result;
    }

    struct ipc_message reply = {
        .capabilities = {{THREAD_STORAGE_SLOT(state->thread_id, state->temp_slot), THREAD_STORAGE_SLOT_DEPTH}}
    };
    *(size_t *) &reply.buffer = 0; // TODO: is this necessary? will the compiler zero out the buffer anyway?

    syscall_invoke(reply_endpoint_address, SIZE_MAX, ENDPOINT_SEND, (size_t) &reply);

    // copied capability is deleted just in case
    syscall_invoke(THREAD_STORAGE_ADDRESS(state->thread_id), THREAD_STORAGE_DEPTH, NODE_DELETE, state->temp_slot);

    return 0;
}
