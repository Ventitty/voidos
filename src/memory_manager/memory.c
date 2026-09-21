#include "src/memory_manager/memory.h"
#include "src/scheduler/spinlock.h"

extern uint8_t _heap_start[];
extern uint8_t _heap_end[];

static void *_heap_current = NULL;
static block_t *alloc_list = NULL;

static spinlock_t mm_lock = SPINLOCK_INIT;

void  mm_init(void) {
    spinlock_acquire(&mm_lock);
    _heap_current = _heap_start;
    alloc_list = NULL;
    spinlock_release(&mm_lock);
}

static void *sbrk_locked(size_t size) {
    if (size == 0) {
        return NULL;
    }

    size_t heap_size = (size_t)(_heap_end - (uint8_t *) _heap_current);

    if (heap_size >= size) {
        void *ptr = _heap_current;
        _heap_current = (uint8_t *)_heap_current + size;

        return ptr;
    }

    return NULL;
}

void *sbrk(size_t size) {
    spinlock_acquire(&mm_lock);
    void *ptr = sbrk_locked(size);
    spinlock_release(&mm_lock);
    return ptr;
}

void *nmap(size_t size) {
    if (size == 0) {
        return NULL;
    }

    size_t total_size = ALIGN_UP(size + HEADER_SIZE, 128);

    spinlock_acquire(&mm_lock);

    block_t *iter_free = alloc_list;

    while (iter_free != NULL) {
        if (iter_free->free && iter_free->size >= size) {
            if (iter_free->size > total_size) {
                block_t *rest = (block_t *)((uint8_t *) iter_free + total_size);
                rest->size = iter_free->size - total_size;
                rest->free = 1;
                rest->next = iter_free->next;

                iter_free->size = size;
                iter_free->next = rest;
            }

            iter_free->free = 0;
            void * res = (uint8_t *) iter_free + HEADER_SIZE;
            spinlock_release(&mm_lock);
            return res;
        }

        iter_free = iter_free->next;
    }

    block_t *new_alloc = sbrk_locked(total_size);
    if (new_alloc != NULL) {
        new_alloc->size = total_size - HEADER_SIZE;
        new_alloc->free = 0;
        new_alloc->next = NULL;

        if (alloc_list == NULL) {
            alloc_list = new_alloc;
        } else {
            iter_free = alloc_list;

            while (iter_free->next != NULL) {
                iter_free = iter_free->next;
            }

            iter_free->next = new_alloc;
        }

        void * res = (uint8_t *) new_alloc + HEADER_SIZE;
        spinlock_release(&mm_lock);
        return res;
    }

    spinlock_release(&mm_lock);
    return NULL;
}

void unmap(void *ptr) {
    if (ptr == NULL) {
        return;
    }

    spinlock_acquire(&mm_lock);

    block_t *b = (block_t *)((uint8_t *) ptr - HEADER_SIZE);
    b->free = 1;

    block_t *cur = alloc_list;
    while (cur && cur->next) {
        if (cur->free && cur->next->free && (uint8_t *) cur + HEADER_SIZE + cur->size == (uint8_t *) cur->next) {
            cur->size += HEADER_SIZE + cur->next->size;
            cur->next = cur->next->next;
        } else {
            cur = cur->next;
        }
    }

    spinlock_release(&mm_lock);
}
