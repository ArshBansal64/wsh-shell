#include "dynamic_array.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/**
 * TODO: Implement all the methods with declarations in dynamic_array.h here, before proceeding with your project
 */

// Create a new DynamicArray with given initial capacity
DynamicArray *da_create(size_t init_capacity)
{
    DynamicArray *da = malloc(sizeof(DynamicArray));
    da->data = malloc(sizeof(char *) * init_capacity);
    da->size = 0;
    da->capacity = init_capacity;
    return da;
}

// Add element to Dynamic Array at the end. Handles resizing if necessary
void da_put(DynamicArray *da, const char *val)
{
    // if full
    if (da->size == da->capacity)
    {
        da->capacity *= 2;

        char **tmp = realloc(da->data, da->capacity * sizeof(char *));
        if (!tmp)
        {
            exit(1);
        }
        da->data = tmp;
    }
    da->data[da->size] = strdup(val);
    da->size++;
}

// Get element at an index (NULL if not found)
char *da_get(DynamicArray *da, const size_t ind)
{
    if (ind >= da->size)
    {
        return NULL;
    }
    char *result = da->data[ind];
    return result;
}

// Delete Element at an index (handles packing)
void da_delete(DynamicArray *da, const size_t ind)
{
    if (ind >= da->size)
    {
        return;
    }
    for (size_t i = ind; i < da->size - 1; i++)
    {
        da->data[i] = da->data[i + 1];
    }
    da->size -= 1;
}

// Print Elements line after line
void da_print(DynamicArray *da)
{
    for (size_t i = 0; i < da->size; i++)
    {
        if (da->data[i] != NULL)
        {
            printf("%s\n", da->data[i]);
            fflush(stdout);
        }
    }
}

// Free whole DynamicArray
void da_free(DynamicArray *da)
{
    for (size_t i = 0; i < da->size; i++)
    {
        free(da->data[i]);
    }
    free(da->data);
    free(da);
}