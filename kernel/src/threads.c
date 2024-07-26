#include "threads.h"
#include <stdint.h>
#include <stdbool.h>
#include "arch.h"
#include "capabilities.h"
#include "heap.h"
#include "string.h"
#include "scheduler.h"
#include "sys/kernel.h"

#undef DEBUG_THREADS

#define NUM_BUCKETS 128

static struct thread_queue thread_hash_table[NUM_BUCKETS];

#define MAX_THREADS 1024

#define PTR_BITS (sizeof(size_t) * 8)
#define USED_THREAD_IDS_SIZE (MAX_THREADS / PTR_BITS)
static size_t used_thread_ids[USED_THREAD_IDS_SIZE];

// 32-bit fnv-1a hash, as described in http://isthe.com/chongo/tech/comp/fnv/
static uint32_t hash(uint16_t value) {
    uint8_t *data = (uint8_t *) &value;
    uint32_t result = 0x811c9dc5;
    result ^= data[0];
    result *= 0x01000193;
    result ^= data[1];
    result *= 0x01000193;
    return result;
}

void init_threads(void) {
    for (int i = 0; i < NUM_BUCKETS; i ++) {
        thread_hash_table[i].start = NULL;
        thread_hash_table[i].end = NULL;
    }

    for (int i = 0; i < USED_THREAD_IDS_SIZE; i ++) {
        used_thread_ids[i] = 0;
    }
    used_thread_ids[0] = 1; // id 0 is reserved for kernel resources
}

extern struct invocation_handlers untyped_handlers;

static size_t read_registers(size_t address, size_t depth, struct capability *slot, size_t argument, bool from_userland) {
    struct read_write_register_args *args = (struct read_write_register_args *) argument;

    bool should_unlock = heap_lock(slot->resource);

    struct thread_capability *thread = (struct thread_capability *) slot->resource;

    memcpy(args->address, (const void *) &thread->registers, args->size > sizeof(struct thread_registers) ? sizeof(struct thread_registers) : args->size);

    if (should_unlock) {
        heap_unlock(slot->resource);
    }

    return 0;
}

static size_t write_registers(size_t address, size_t depth, struct capability *slot, size_t argument, bool from_userland) {
    struct read_write_register_args *args = (struct read_write_register_args *) argument;

    bool should_unlock = heap_lock(slot->resource);

    struct thread_capability *thread = (struct thread_capability *) slot->resource;

    memcpy((void *) &thread->registers, args->address, args->size > sizeof(struct thread_registers) ? sizeof(struct thread_registers) : args->size);

    if (should_unlock) {
        heap_unlock(slot->resource);
    }

    return 0;
}

static size_t resume(size_t address, size_t depth, struct capability *slot, size_t argument, bool from_userland) {
    bool should_unlock = heap_lock(slot->resource);

    resume_thread((struct thread_capability *) slot->resource);

    if (should_unlock) {
        heap_unlock(slot->resource);
    }

    return 0;
}

static size_t suspend(size_t address, size_t depth, struct capability *slot, size_t argument, bool from_userland) {
    bool should_unlock = heap_lock(slot->resource);

    suspend_thread((struct thread_capability *) slot->resource);

    if (should_unlock) {
        heap_unlock(slot->resource);
    }

    return 0;
}

static size_t set_root_node(size_t address, size_t depth, struct capability *slot, size_t argument, bool from_userland) {
    //bool should_unlock = heap_lock(slot->resource);

    struct thread_capability *thread = (struct thread_capability *) slot->resource;
    struct set_root_node_args *args = (struct set_root_node_args *) argument;
    struct look_up_result result;

    if (!look_up_capability_relative(args->address, args->depth, from_userland, &result)) {
        /*if (should_unlock) {
            heap_unlock(slot->resource);
        }*/

        return 0;
    }

    size_t return_value = 0;

    if (result.slot->handlers == &node_handlers) {
        return_value = 1;

        struct capability *root_slot = &thread->root_capability;
        move_capability(result.slot, root_slot);

        // update the address of the new root capability
        root_slot->address.address = 0;
        root_slot->address.depth = 0;
        root_slot->address.thread_id = thread->thread_id;
        root_slot->address.bucket_number = thread->bucket_number;

        if (root_slot->resource_list.prev == NULL) {
            // only update the owner id of the node if it's the owner of the resource
            heap_set_update_capability(root_slot->resource, &root_slot->address);
        }

        // TODO: recursively search through the slots of the root node and change the thread id and bucket number in the same way
    }

    unlock_looked_up_capability(&result);

    /*if (should_unlock) {
        heap_unlock(slot->resource);
    }*/

    return return_value;
}

struct invocation_handlers thread_handlers = {
    5,
    {read_registers, write_registers, resume, suspend, set_root_node}
    // TODO: thread destructor
};

void alloc_thread(struct heap *heap, void **resource, struct invocation_handlers **handlers) {
    uint16_t thread_id = 0;
    bool has_thread_id = false;

    // allocate a new thread id for this thread
    for (int i = 0; i < USED_THREAD_IDS_SIZE; i ++) {
        if (used_thread_ids[i] == SIZE_MAX) {
            continue;
        }

        int bit;
        for (bit = 0; bit < PTR_BITS && (used_thread_ids[i] & (1 << bit)) != 0; bit ++);

        used_thread_ids[i] |= (1 << bit);

        thread_id = i * PTR_BITS + bit;
        has_thread_id = true;
        break;
    }

    // if a thread id couldn't be found, give up
    if (!has_thread_id) {
#ifdef DEBUG_THREADS
        printk("threads: couldn't allocate id for new thread!\n");
#endif
        *resource = NULL;
        *handlers = NULL;
        return;
    }

#ifdef DEBUG_THREADS
    printk("threads: allocated id %d for new thread\n", thread_id);
#endif

    struct thread_capability *thread = heap_alloc(heap, sizeof(struct thread_capability));

    if (thread == NULL) {
#ifdef DEBUG_THREADS
        printk("threads: heap_alloc for new thread failed!\n");
#endif
        used_thread_ids[thread_id / PTR_BITS] &= ~(1 << (thread_id % PTR_BITS)); // release thread id
        *resource = NULL;
        *handlers = NULL;
        return;
    }

    memset((char *) thread, 0, sizeof(struct thread_capability));
    thread->exec_mode = SUSPENDED;
    thread->thread_id = thread_id;
    // queue entries don't need to be set to NULL here as long as NULL is 0

    uint32_t id_hash = hash(thread_id);
    thread->bucket_number = id_hash % NUM_BUCKETS;

#ifdef DEBUG_THREADS
    printk("threads: hashed thread id is 0x%08x, bucket number is %d\n", id_hash, thread->bucket_number);
#endif

    // insert this thread into the thread hash table
    struct thread_queue *bucket = &thread_hash_table[thread->bucket_number];

    if (bucket->start == NULL) {
#ifdef DEBUG_THREADS
        printk("threads: adding thread to new bucket\n");
#endif
        bucket->start = thread;
        bucket->end = thread;
    } else {
#ifdef DEBUG_THREADS
        printk("threads: adding thread to end of existing bucket\n");
#endif

        bool should_unlock = heap_lock(bucket->end);

        bucket->end->table_entry.next = thread;
        thread->table_entry.prev = bucket->end;
        bucket->end = thread;

        if (should_unlock) {
            heap_unlock(bucket->end);
        }
    }

    *resource = (void *) thread;
    *handlers = &thread_handlers;
}

void on_thread_moved(struct thread_capability *thread) {
    if (thread->runqueue != NULL) {
        // update references to this thread in its current runqueue
        if (thread->runqueue_entry.next == NULL) {
            thread->runqueue->end = thread;
        } else {
            thread->runqueue_entry.next->runqueue_entry.prev = thread;
        }

        if (thread->runqueue_entry.prev == NULL) {
            thread->runqueue->start = thread;
        } else {
            thread->runqueue_entry.prev->runqueue_entry.next = thread;
        }
    }

    // update references to this thread in the thread hash table
    if (thread->table_entry.next == NULL) {
        thread_hash_table[thread->bucket_number].end = thread;
    } else {
        thread->table_entry.next->table_entry.prev = thread;
    }

    if (thread->table_entry.prev == NULL) {
        thread_hash_table[thread->bucket_number].start = thread;
    } else {
        thread->table_entry.prev->table_entry.next = thread;
    }

    if ((thread->scheduler_flags & THREAD_NEEDS_CPU_UPDATE) != 0) {
        // update references to this thread in the needs cpu update queue
        if (thread->cpu_update_entry.next == NULL) {
            scheduler_state.needs_cpu_time_update.end = thread;
        } else {
            thread->cpu_update_entry.next->cpu_update_entry.prev = thread;
        }

        if (thread->cpu_update_entry.prev == NULL) {
            scheduler_state.needs_cpu_time_update.start = thread;
        } else {
            thread->cpu_update_entry.prev->cpu_update_entry.next = thread;
        }
    }

    if ((thread->scheduler_flags & THREAD_CURRENTLY_RUNNING) != 0) {
        scheduler_state.current_thread = thread;
    }
}

bool look_up_thread_by_id(uint16_t thread_id, uint8_t bucket_number, struct thread_capability **thread) {
    *thread = NULL;
    return false;
}
