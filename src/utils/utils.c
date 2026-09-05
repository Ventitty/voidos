#include "src/utils/utils.h"

void *memset(void *s, int c, size_t count) {
    char *xs = s;
    while (count--)
        *xs++ = c;
    return s;
}

void *memcpy(void *dest, const void *src, size_t count) {
    char *tmp = dest;
    const char *s = src;
    while (count--)
        *tmp++ = *s++;
    return dest;
}

void *memmove(void *dest, const void *src, size_t count) {
    char *tmp;
    const char *s;

    if (dest <= src) {
        tmp = dest;
        s = src;
        while (count--)
            *tmp++ = *s++;
    } else {
        tmp = (char *)dest + count;
        s = (const char *)src + count;
        while (count--)
            *--tmp = *--s;
    }
    return dest;
}

int memcmp(const void *cs, const void *ct, size_t count) {
    const unsigned char *su1 = cs, *su2 = ct;
    int res = 0;

    for (; 0 < count; ++su1, ++su2, count--) {
        if ((res = *su1 - *su2) != 0)
            break;
    }
    return res;
}

void memzero_explicit(void *s, size_t count) {
    memset(s, 0, count);
    __asm__ __volatile__("" : : "r"(s) : "memory");
}
