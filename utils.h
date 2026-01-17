#include <unistd.h>

char *replaceAt(const char *command, const size_t i, const size_t n, const char *value);

char *replaceKey(const char *command, const char *key, const char *value);

/* Append src to the end of dest */
char *append(char *dest, const char *src);
