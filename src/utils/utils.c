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

size_t strlen(const char *s) {
    size_t n = 0;
    while (s[n] != '\0') n++;
    return n;
}

int strcmp(const char *a, const char *b) {
    while (*a != '\0' && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i] || a[i] == '\0') {
            return (unsigned char)a[i] - (unsigned char)b[i];
        }
    }
    return 0;
}

char *strcpy(char *dest, const char *src) {
    char *ret = dest;
    while ((*dest++ = *src++) != '\0') { }
    return ret;
}

char *strncpy(char *dest, const char *src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i] != '\0'; i++) dest[i] = src[i];
    for (; i < n; i++) dest[i] = '\0';
    return dest;
}

char *strcat(char *dest, const char *src) {
    char *ret = dest;
    while (*dest != '\0') dest++;
    while ((*dest++ = *src++) != '\0') { }
    return ret;
}

char *strncat(char *dest, const char *src, size_t n) {
    char *ret = dest;
    while (*dest != '\0') dest++;
    size_t i = 0;
    for (; i < n && src[i] != '\0'; i++) dest[i] = src[i];
    dest[i] = '\0';
    return ret;
}

char *strchr(const char *s, int c) {
    while (*s != '\0') {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    return (c == '\0') ? (char *)s : NULL;
}

char *strrchr(const char *s, int c) {
    const char *last = NULL;
    while (*s != '\0') {
        if (*s == (char)c) last = s;
        s++;
    }
    if (c == '\0') return (char *)s;
    return (char *)last;
}

char *strstr(const char *haystack, const char *needle) {
    if (*needle == '\0') return (char *)haystack;

    for (; *haystack != '\0'; haystack++) {
        const char *h = haystack;
        const char *n = needle;
        while (*h != '\0' && *n != '\0' && *h == *n) { h++; n++; }
        if (*n == '\0') return (char *)haystack;
    }
    return NULL;
}

int is_digit(int c) { return c >= '0' && c <= '9'; }
int is_alpha(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
int is_space(int c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'; }
int to_upper(int c) { return (c >= 'a' && c <= 'z') ? c - ('a' - 'A') : c; }
int to_lower(int c) { return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c; }

int atoi(const char *s) {
    while (is_space((unsigned char)*s)) s++;

    int sign = 1;
    if (*s == '+' || *s == '-') {
        if (*s == '-') sign = -1;
        s++;
    }

    int result = 0;
    while (is_digit((unsigned char)*s)) {
        result = result * 10 + (*s - '0');
        s++;
    }
    return sign * result;
}

char *utoa_base(uint32_t value, char *buf, int base) {
    static const char digits[] = "0123456789abcdef";

    if (base < 2 || base > 16) base = 10;

    char tmp[33];
    int i = 0;

    if (value == 0) {
        tmp[i++] = '0';
    } else {
        while (value > 0) {
            tmp[i++] = digits[value % (uint32_t)base];
            value /= (uint32_t)base;
        }
    }

    int j = 0;
    while (i > 0) {
        buf[j++] = tmp[--i];
    }
    buf[j] = '\0';

    return buf;
}

char *itoa(int value, char *buf) {
    if (value < 0) {
        buf[0] = '-';
        /* -value peut déborder pour INT_MIN, mais on reste volontairement
         * simple ici (cas limite non géré, comme documenté dans le header). */
        utoa_base((uint32_t)(-value), buf + 1, 10);
        return buf;
    }
    return utoa_base((uint32_t)value, buf, 10);
}
