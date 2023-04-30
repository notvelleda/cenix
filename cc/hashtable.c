#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hashtable.h"

/* 32-bit fnv-1a hash, as described in http://isthe.com/chongo/tech/comp/fnv/
 * calculates the hash value of a given null terminated string, excluding the
 * null byte
 * returns the length of the string including the null terminator to make
 * duplicating it easier, since it avoids a strlen call */
unsigned int hash_string(const char *string, unsigned long *value) {
    char c;
    unsigned int length = 1;
    *value = 0x811c9dc5;
    while (c = *(string ++)) {
        *value ^= c;
        *value *= 0x01000193;
        length ++;
    }
    return length;
}

void hashtable_init(struct hashtable *table) {
    unsigned int i = 0;
    for (; i < NUM_BUCKETS; i ++)
        table->buckets[i] = NULL;
}

void hashtable_free(struct hashtable *table, void (*free_value)(void *value)) {
    unsigned int i = 0;
    struct bucket *current;
    struct bucket *next;
    for (; i < NUM_BUCKETS; i ++)
        if ((current = table->buckets[i]) != NULL)
            /* walk the linked list, freeing all links as we go */
            while (1) {
                next = current->next;

                free(current->key);
                if (free_value != NULL)
                    free_value(current->value);
                free(current);

                if (next == NULL)
                    break;
                else
                    current = next;
            }
}

char hashtable_insert(struct hashtable *table, const char *key, void *value) {
    unsigned long hash_value;
    unsigned int length;
    struct bucket *bucket;

    length = hash_string(key, &hash_value);
    bucket = table->buckets[hash_value % NUM_BUCKETS];

    if (bucket == NULL) {
        bucket = table->buckets[hash_value % NUM_BUCKETS] = (struct bucket *)
            malloc(sizeof(struct bucket));
        if (bucket == NULL) {
            perror("couldn't allocate memory for hash table bucket, aborting");
            exit(1);
        }
    } else {
        /* bucket isn't empty, make sure the value isn't already in it */
        while (1) {
            if (bucket->hash == hash_value)
                if (strcmp(bucket->key, key) == 0)
                    return 0;

            if (bucket->next == NULL)
                break;
            else
                bucket = bucket->next;
        }

        /* allocate a new bucket and add it to the linked list */
        bucket->next = (struct bucket *) malloc(sizeof(struct bucket));
        if (bucket->next == NULL) {
            perror("couldn't allocate memory for hash table bucket, aborting");
            exit(1);
        }
        bucket = bucket->next;
    }

    bucket->hash = hash_value;
    bucket->key = (char *) malloc(length);
    if (bucket->key == NULL) {
        perror("couldn't allocate memory for hash table key, aborting");
        exit(1);
    }
    strcpy(bucket->key, key);
    bucket->value = value;
    bucket->next = NULL;
    return 1;
}

void *hashtable_lookup_hashed(
    struct hashtable *table,
    const char *key,
    unsigned long hash_value
) {
    struct bucket *bucket = table->buckets[hash_value % NUM_BUCKETS];

    if (bucket == NULL)
        return NULL;

    while (1) {
        if (bucket->hash == hash_value)
            if (strcmp(bucket->key, key) == 0)
                return bucket->value;

        if (bucket->next == NULL)
            return NULL;
        else
            bucket = bucket->next;
    }
}

void *hashtable_lookup(struct hashtable *table, const char *key) {
    unsigned long hash_value;
    hash_string(key, &hash_value);
    return hashtable_lookup_hashed(table, key, hash_value);
}
