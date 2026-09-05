#ifndef UTILS_H
#define UTILS_H

#include "src/types.h"

void *memset(void *s, int c, size_t count);
void *memcpy(void *dest, const void *src, size_t count);
void *memmove(void *dest, const void *src, size_t count);
int memcmp(const void *cs, const void *ct, size_t count);
void memzero_explicit(void *s, size_t count);

#endif /* UTILS_H */
