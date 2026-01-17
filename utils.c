#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Replace `n` characters from position `i` with the given string (`value`) in `command` */
char *replaceAt(const char *command, const size_t i, const size_t n, const char *value)
{
  size_t prefixLength = i;
  size_t valueLength = strlen(value);
  size_t suffixLength = strlen(command + i + n);
  size_t newResultLength = prefixLength + valueLength + suffixLength + 1;
  char *new_result = malloc(newResultLength);
  if (new_result == NULL)
  {
    perror("malloc");
    exit(-1);
  }
  memcpy(new_result, command, prefixLength);
  memcpy(new_result + prefixLength, value, valueLength);
  memcpy(new_result + prefixLength + valueLength, command + i + n, suffixLength + 1);
  return new_result;
}

/* Replace first occurrence of the key in command with the given value.
   (If the key isn’t found, simply return a duplicate of the command.) */
char *replaceKey(const char *command, const char *key, const char *value)
{
  char *found = strstr(command, key);
  if (!found)
    return strdup(command);
  return replaceAt(command, found - command, strlen(key), value);
}

/* Append src to the end of dest */
char *append(char *dest, const char *src)
{
  if (!src) return dest;
  size_t dest_len = dest ? strlen(dest) : 0;
  size_t src_len  = strlen(src);

  char *new_str = realloc(dest, dest_len + src_len + 1);
  if (!new_str)
  {
    perror("realloc");
    free(dest);
    return NULL;
  }

  memcpy(new_str + dest_len, src, src_len + 1); // copy including '\0'
  return new_str;
}
