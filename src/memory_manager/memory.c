#include "src/memory_manager/memory.h"

extern void uart_print_hex(uint32_t val);

extern uint8_t _heap_start[];
extern uint8_t _heap_end[];

mem_pool_t kernel_heap;

static mem_pool_t *pools[MEM_MAX_POOLS];
static spinlock_t registry_lock = SPINLOCK_INIT;

static void registry_add(mem_pool_t *pool) {
    spinlock_acquire(&registry_lock);
    for (int i = 0; i < MEM_MAX_POOLS; i++) {
        if (pools[i] == pool) { spinlock_release(&registry_lock); return; }
    }
    for (int i = 0; i < MEM_MAX_POOLS; i++) {
        if (pools[i] == NULL) { pools[i] = pool; break; }
    }
    spinlock_release(&registry_lock);
}

static mem_pool_t *pool_of(const void *ptr) {
    const uint8_t *p = (const uint8_t *)ptr;

    spinlock_acquire(&registry_lock);
    mem_pool_t *found = NULL;
    for (int i = 0; i < MEM_MAX_POOLS; i++) {
        if (pools[i] == NULL || !pools[i]->ready) continue;
        if (p >= pools[i]->start && p < pools[i]->end) { found = pools[i]; break; }
    }
    spinlock_release(&registry_lock);
    return found;
}

void pool_init(mem_pool_t *pool, const char *name, void *start, void *end) {
    if (pool == NULL || start == NULL || end == NULL || (uint8_t *)end <= (uint8_t *)start) return;

    uint8_t *s = (uint8_t *)(((uint32_t)start + 15u) & ~15u);

    pool->lock.locked = 0;
    pool->lock.saved_ps = 0;
    pool->start   = s;
    pool->end     = (uint8_t *)end;
    pool->current = s;
    pool->list    = NULL;
    pool->name    = name;
    pool->ready   = 1;

    registry_add(pool);
}

static void *pool_sbrk_locked(mem_pool_t *pool, size_t size) {
    if (size == 0) return NULL;

    size_t left = (size_t)(pool->end - pool->current);
    if (left < size) return NULL;

    void *ptr = pool->current;
    pool->current += size;
    return ptr;
}

void *pool_alloc(mem_pool_t *pool, size_t size) {
    if (pool == NULL || !pool->ready || size == 0) return NULL;

    spinlock_acquire(&pool->lock);

    size_t total_size = ALIGN_UP(size + HEADER_SIZE, 128);

    for (block_t *it = pool->list; it != NULL; it = it->next) {
        if (!it->free || it->size < size) continue;

        if (it->size > total_size) {
            block_t *rest = (block_t *)((uint8_t *)it + total_size);
            rest->size = it->size - total_size;
            rest->free = 1;
            rest->next = it->next;

            it->size = total_size - HEADER_SIZE;
            it->next = rest;
        }

        it->free = 0;
        void *res = (uint8_t *)it + HEADER_SIZE;
        spinlock_release(&pool->lock);
        return res;
    }

    block_t *fresh = (block_t *)pool_sbrk_locked(pool, total_size);
    if (fresh == NULL) {
        spinlock_release(&pool->lock);
        return NULL;
    }

    fresh->size = total_size - HEADER_SIZE;
    fresh->free = 0;
    fresh->next = NULL;

    if (pool->list == NULL) {
        pool->list = fresh;
    } else {
        block_t *last = pool->list;
        while (last->next != NULL) last = last->next;
        last->next = fresh;
    }

    void *res = (uint8_t *)fresh + HEADER_SIZE;
    spinlock_release(&pool->lock);
    return res;
}

void pool_free(mem_pool_t *pool, void *ptr) {
    if (pool == NULL || ptr == NULL) return;

    spinlock_acquire(&pool->lock);

    block_t *b = (block_t *)((uint8_t *)ptr - HEADER_SIZE);
    b->free = 1;

    block_t *cur = pool->list;
    while (cur && cur->next) {
        if (cur->free && cur->next->free &&
            (uint8_t *)cur + HEADER_SIZE + cur->size == (uint8_t *)cur->next) {
            cur->size += HEADER_SIZE + cur->next->size;
        cur->next = cur->next->next;
            } else {
                cur = cur->next;
            }
    }

    spinlock_release(&pool->lock);
}

void pool_stats(mem_pool_t *pool, uint32_t *total, uint32_t *used, uint32_t *largest_free) {
    if (pool == NULL || !pool->ready) return;

    spinlock_acquire(&pool->lock);

    uint32_t t = (uint32_t)(pool->end - pool->start);
    uint32_t u = 0, biggest = (uint32_t)(pool->end - pool->current);
    for (block_t *it = pool->list; it != NULL; it = it->next) {
        if (it->free) { if (it->size > biggest) biggest = (uint32_t)it->size; }
        else u += (uint32_t)(it->size + HEADER_SIZE);
    }

    spinlock_release(&pool->lock);

    if (total != NULL) *total = t;
    if (used != NULL) *used = u;
    if (largest_free != NULL) *largest_free = biggest;
}

void pool_print(mem_pool_t *pool) {
    if (pool == NULL || !pool->ready) return;

    uint32_t total, used, biggest;
    pool_stats(pool, &total, &used, &biggest);

    uart_print("  pool "); uart_print(pool->name);
    uart_print(" : total "); uart_print_hex(total);
    uart_print(", utilise "); uart_print_hex(used);
    uart_print(", plus grand bloc libre "); uart_print_hex(biggest);
    uart_print("\n");
}

void mm_init(void) {
    pool_init(&kernel_heap, "tas noyau (DRAM)", _heap_start, _heap_end);
}

void *sbrk(size_t size) {
    spinlock_acquire(&kernel_heap.lock);
    void *p = pool_sbrk_locked(&kernel_heap, size);
    spinlock_release(&kernel_heap.lock);
    return p;
}

void *nmap(size_t size) {
    return pool_alloc(&kernel_heap, size);
}

void unmap(void *ptr) {
    if (ptr == NULL) return;

    mem_pool_t *pool = pool_of(ptr);
    if (pool == NULL) return;

        pool_free(pool, ptr);
}
