#include <unistd.h>

typedef struct {
  char **data;
  size_t size; // Number of elements in the Array
  size_t capacity; // Current Capacity of the Array
} DynamicArray;

// Create a new DynamicArray with given initial capacity
DynamicArray* da_create(size_t init_capacity);

// Add element to Dynamic Array at the end. Handles resizing if necessary
void da_put(DynamicArray *da, const char* val);

// Get element at an index (NULL if not found)
char *da_get(DynamicArray *da, const size_t ind);

// Delete Element at an index (handles packing)
void da_delete(DynamicArray *da, const size_t ind);

// Print Elements line after line
void da_print(DynamicArray *da);

// Free whole DynamicArray
void da_free(DynamicArray *da);
