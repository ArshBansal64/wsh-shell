#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hash_map.h"

/**
 * @Brief djb2 hash function by Dan Bernstein
 * @Ref https://theartincode.stanis.me/008-djb2/
 *
 * @param key The string to hash
 * @return The hash value
 */
unsigned int hash(const char *key)
{
  unsigned long h = 5381;
  int c;
  while ((c = *key++))
  {
    h = ((h << 5) + h) + c; // h * 33 + c
  }
  return h % TABLE_SIZE;
}

/**
 * @Brief Create a new HashMap
 *
 * @return Pointer to a newly created HashMap
 */
HashMap *hm_create(void)
{
  HashMap *ht = malloc(sizeof(HashMap));
  if (!ht)
  {
    perror("malloc");
    exit(-1);
  };
  for (int i = 0; i < TABLE_SIZE; i++)
  {
    ht->buckets[i] = NULL;
  }
  return ht;
}

/**
 * @Brief Insert or update key-value pair
 *
 * @param hm Pointer to the HashMap
 * @param key The key string
 * @param value The value string
 */
void hm_put(HashMap *hm, const char *key, const char *value)
{
  unsigned int idx = hash(key);
  Entry *e = hm->buckets[idx];
  
  // Check if key already exists
  while (e)
  {
    if (strcmp(e->key, key) == 0)
    {
      // Update value
      free(e->value);
      e->value = strdup(value);
      return;
    }
    e = e->next;
  }

  // Insert new entry at head of list
  Entry *new_entry = malloc(sizeof(Entry));
  new_entry->key = strdup(key);
  new_entry->value = strdup(value);
  new_entry->next = hm->buckets[idx];
  hm->buckets[idx] = new_entry;
}

/**
 * @Brief Get value by key (NULL if not found)
 *
 * @param hm Pointer to the HashMap
 * @param key The key string
 */
char *hm_get(const HashMap *hm, const char *key)
{
  const unsigned int idx = hash(key);
  const Entry *e = hm->buckets[idx];
  while (e)
  {
    if (strcmp(e->key, key) == 0)
    {
      return e->value;
    }
    e = e->next;
  }
  return NULL;
}

/* Delete the entry with a given key from the hashmap */
void hm_delete(HashMap *hm, const char *key)
{
  const unsigned int idx = hash(key);
  Entry *e = hm->buckets[idx];
  Entry *prev = NULL;

  while (e)
  {
    if (strcmp(e->key, key) == 0)
    {
      if (prev)
      {
        prev->next = e->next;
      }
      else
      {
        hm->buckets[idx] = e->next;
      }
      free(e->key);
      free(e->value);
      free(e);
      return;
    }
    prev = e;
    e = e->next;
  }
}

/* Print the entries in the hashmap, one in each line */
void hm_print(const HashMap *hm)
{
  for (int i = 0; i < TABLE_SIZE; i++)
  {
    const Entry *e = hm->buckets[i];
    while (e)
    {
      printf("%s = '%s'\n",e->key, e->value);
      e = e->next;
    }
  }
}

/* Print the entries in the hashmap sorted by key */
int cmp_keys(const void *a, const void *b) {
  const char *ka = *(const char **)a;
  const char *kb = *(const char **)b;
  return strcmp(ka, kb);
}

void hm_print_sorted(const HashMap *hm)
{
  // Count total entries
  int count = 0;
  for (int i = 0; i < TABLE_SIZE; i++) {
    Entry *e = hm->buckets[i];
    while (e) {
      count++;
      e = e->next;
    }
  }
  if (count == 0) return;
  // Collect keys
  char **keys = malloc(count * sizeof(char *));
  int idx = 0;
  for (int i = 0; i < TABLE_SIZE; i++) {
    Entry *e = hm->buckets[i];
    while (e) {
      keys[idx++] = e->key;
      e = e->next;
    }
  }
  // Sort keys
  qsort(keys, count, sizeof(char *), cmp_keys);
  // Print key-value pairs
  for (int i = 0; i < count; i++) {
    char *val = hm_get(hm, keys[i]);
    printf("%s = '%s'\n", keys[i], val);
  }
  free(keys);
}

/* Reinitialize the hashmap */
void hm_reset(HashMap *hm)
{
  hm_free(hm);
  hm = hm_create();
}

/* Free the memory used by the hashmap */
void hm_free(HashMap *hm)
{
  for (int i = 0; i < TABLE_SIZE; i++)
  {
    Entry *e = hm->buckets[i];
    while (e)
    {
      Entry *next = e->next;
      free(e->key);
      free(e->value);
      free(e);
      e = next;
    }
  }
  free(hm);
}

/* (Unused) Use this an example to show how to use this hashmap implementation */
int hm_usage_example(void)
{
  // Initialization
  HashMap *hm = hm_create();

  // Add Elements
  hm_put(hm, "name", "Alice");
  hm_put(hm, "bird", "flamingo");
  hm_put(hm, "city", "Madison");

  // Get Elements
  printf("name = %s\n", hm_get(hm, "name"));
  printf("city = %s\n", hm_get(hm, "city"));

  // Update value for a Key
  hm_put(hm, "city", "New York");
  printf("city (updated) = %s\n", hm_get(hm, "city"));

  // Delete Key & corresponding value
  hm_delete(hm, "language");
  printf("language = %s\n", hm_get(hm, "language"));

  // Free memory once done
  hm_free(hm);
  return 0;
}
