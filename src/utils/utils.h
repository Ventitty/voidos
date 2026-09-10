#ifndef UTILS_H
#define UTILS_H

#include "src/types.h"

void *memset(void *s, int c, size_t count);
void *memcpy(void *dest, const void *src, size_t count);
void *memmove(void *dest, const void *src, size_t count);
int memcmp(const void *cs, const void *ct, size_t count);
void memzero_explicit(void *s, size_t count);
size_t strlen(const char *s);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
char  *strcpy(char *dest, const char *src);
char  *strncpy(char *dest, const char *src, size_t n);
char  *strcat(char *dest, const char *src);
char  *strncat(char *dest, const char *src, size_t n);
char  *strchr(const char *s, int c);
char  *strrchr(const char *s, int c);
char  *strstr(const char *haystack, const char *needle);

int  is_digit(int c);
int  is_alpha(int c);
int  is_space(int c);
int  to_upper(int c);
int  to_lower(int c);

int atoi(const char *s);
char *itoa(int value, char *buf);
char *utoa_base(uint32_t value, char *buf, int base);

#endif /* UTILS_H */
