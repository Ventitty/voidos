#ifndef TYPES_H
#define TYPES_H

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

typedef signed char      int8_t;
typedef signed short     int16_t;
typedef signed int       int32_t;
typedef signed long long int64_t;
typedef __typeof__(sizeof(0)) size_t;

#define NULL ((void *)0)

void uart_print(const char *str);
void uart_print_int(int num);

#endif /* TYPES_H */
