#ifndef MEMORY_H
#define MEMORY_H

#include "src/types.h"
#include "src/scheduler/spinlock.h"

#define HEADER_SIZE ((size_t)sizeof(block_t))
#define ALIGN_UP(size, alignment) (((size) + ((alignment) - 1)) & ~((alignment) - 1))
#define MEM_MAX_POOLS 4

typedef struct block {
    size_t size;
    int    free;
    struct block *next;
} block_t;

typedef struct {
    uint8_t   *start;
    uint8_t   *end;
    uint8_t   *current;
    block_t   *list;
    spinlock_t lock;
    const char *name;
    int        ready;
} mem_pool_t;

void  pool_init(mem_pool_t *pool, const char *name, void *start, void *end);
void *pool_alloc(mem_pool_t *pool, size_t size);
void  pool_free(mem_pool_t *pool, void *ptr);
void  pool_stats(mem_pool_t *pool, uint32_t *total, uint32_t *used, uint32_t *largest_free);
void  pool_print(mem_pool_t *pool);

extern mem_pool_t kernel_heap;

void  mm_init(void);
void *sbrk(size_t size);
void *nmap(size_t size);
void  unmap(void *ptr);

#endif /* MEMORY_H */
