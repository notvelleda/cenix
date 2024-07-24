#include "threads.h"
#include <stdint.h>
#include <stdbool.h>
#include "arch.h"
#include "capabilities.h"
#include "heap.h"
#include "string.h"
#include "scheduler.h"

#define DEBUG_THREADS

#define NUM_BUCKETS 128

struct thread_queue thread_hash_table[NUM_BUCKETS];

#define MAX_THREADS 1024

#define PTR_BITS (sizeof(size_t) * 8)
#define USED_THREAD_IDS_SIZE (MAX_THREADS / PTR_BITS)
size_t used_thread_ids[USED_THREAD_IDS_SIZE];

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
}

extern struct invocation_handlers untyped_handlers;

static size_t read_registers(struct capability *slot, size_t argument, bool from_userland) {
    struct read_write_register_args *args = (struct read_write_register_args *) argument;

    bool should_unlock = heap_lock(slot->resource);

    struct thread_capability *thread = (struct thread_capability *) slot->resource;

    memcpy(args->address, (const void *) &thread->registers, args->size > sizeof(struct registers) ? sizeof(struct registers) : args->size);

    if (should_unlock) {
        heap_unlock(slot->resource);
    }

    return 0;
}

static size_t write_registers(struct capability *slot, size_t argument, bool from_userland) {
    struct read_write_register_args *args = (struct read_write_register_args *) argument;

    bool should_unlock = heap_lock(slot->resource);

    struct thread_capability *thread = (struct thread_capability *) slot->resource;

    memcpy((void *) &thread->registers, args->address, args->size > sizeof(struct registers) ? sizeof(struct registers) : args->size);

    if (should_unlock) {
        heap_unlock(slot->resource);
    }

    return 0;
}

static size_t resume(struct capability *slot, size_t argument, bool from_userland) {
    bool should_unlock = heap_lock(slot->resource);

    resume_thread((struct thread_capability *) slot->resource);

    if (should_unlock) {
        heap_unlock(slot->resource);
    }

    return 0;
}

static size_t suspend(struct capability *slot, size_t argument, bool from_userland) {
    bool should_unlock = heap_lock(slot->resource);

    suspend_thread((struct thread_capability *) slot->resource);

    if (should_unlock) {
        heap_unlock(slot->resource);
    }

    return 0;
}

struct invocation_handlers thread_handlers = {
    4,
    {read_registers, write_registers, resume, suspend}
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
        printk("couldn't allocate id for new thread!\n");
#endif
        *resource = NULL;
        *handlers = NULL;
        return;
    }

#ifdef DEBUG_THREADS
    printk("allocated id %d for new thread\n", thread_id);
#endif

    struct thread_capability *thread = heap_alloc(heap, sizeof(struct thread_capability));

    memset((char *) thread, 0, sizeof(struct thread_capability));
    thread->exec_mode = SUSPENDED;
    thread->thread_id = thread_id;
    thread->id_hash = hash(thread_id);
    // queue entries don't need to be set to NULL here as long as NULL is 0

#ifdef DEBUG_THREADS
    printk("hashed thread id is 0x%08x\n", thread->id_hash);
#endif

    // insert this thread into the thread hash table
    size_t bucket_number = thread->id_hash % NUM_BUCKETS;
    struct thread_queue *bucket = &thread_hash_table[bucket_number];

    if (bucket->start == NULL) {
#ifdef DEBUG_THREADS
        printk("adding thread to new bucket %d\n", bucket_number);
#endif
        bucket->start = thread;
        bucket->end = thread;
    } else {
#ifdef DEBUG_THREADS
        printk("adding thread to end of existing bucket %d\n", bucket_number);
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
        thread_hash_table[thread->id_hash % NUM_BUCKETS].end = thread;
    } else {
        thread->table_entry.next->table_entry.prev = thread;
    }

    if (thread->table_entry.prev == NULL) {
        thread_hash_table[thread->id_hash % NUM_BUCKETS].start = thread;
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
