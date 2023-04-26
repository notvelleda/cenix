#ifndef HASHTABLE_H
#define HASHTABLE_H

#define NUM_BUCKETS 128

struct bucket {
    /* the hash of this entry's key */
    unsigned long hash;
    /* a pointer to the unhashed key of this entry */
    char *key;
    /* a pointer to the value of this entry */
    void *value;
    /* a pointer to the next entry in the bucket, or NULL if there isn't one */
    struct bucket *next;
};

struct hashtable {
    struct bucket *buckets[NUM_BUCKETS];
};

/* 32-bit fnv-1a hash, as described in http://isthe.com/chongo/tech/comp/fnv/
 * calculates the hash value of a given null terminated string, excluding the
 * null byte
 * returns the length of the string including the null terminator to make
 * duplicating it easier, since it avoids a strlen call */
unsigned int hash_string(const char *string, unsigned long *value);

/* ensures initial values of the hash table are set properly upon creation */
void hashtable_init(struct hashtable *table);

/* frees all memory associated with a hash table. free_value will be called for
 * every value in the table unless it's NULL */
void hashtable_free(struct hashtable *table, void (*free_value)(void *value));

/* inserts the given key and value into the hash table
 * returns 1 on success, 0 if the value is already in the table */
char hashtable_insert(struct hashtable *table, const char *key, void *value);

/* same as `hashtable_lookup` but the hash value is provided, so it doesn't have
 * to be hashed a lot of times on many consecutive lookups */
void *hashtable_lookup_hashed(
    struct hashtable *table,
    const char *key,
    unsigned long hash_value
);

/* looks up the value for the given key in the hash table, returning the pointer
 * to the value on success and NULL on failure */
void *hashtable_lookup(struct hashtable *table, const char *key);

#endif
