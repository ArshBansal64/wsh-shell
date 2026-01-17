#ifndef HASH_MAP_H
#define HASH_MAP_H

#define TABLE_SIZE 101  // prime number for better hashing

// Entry in the key-value store
typedef struct Entry{
    char *key;
    char *value;
    struct Entry *next;  // for chaining
} Entry;

// Hash table
typedef struct {
    Entry *buckets[TABLE_SIZE];
} HashMap;

// Create a new HashMap
HashMap *hm_create(void);

// Insert or update key-value pair
void hm_put(HashMap *hm, const char *key, const char *value);

// Get value by key (NULL if not found)
char *hm_get(const HashMap *hm, const char *key);

// Delete Entry with given Key
void hm_delete(HashMap *hm, const char *key);

// Print the Key Value pairs in the HashMap
void hm_print(const HashMap *hm);

// Print the Key Value pairs in sorted order by Key
void hm_print_sorted(const HashMap *hm);

// Reset HashMap
void hm_reset(HashMap *hm);

// Free whole HashMap
void hm_free(HashMap *hm);

#endif // HASH_MAP_H
