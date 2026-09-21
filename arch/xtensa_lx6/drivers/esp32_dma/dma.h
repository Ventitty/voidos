#ifndef DMA_H
#define DMA_H

#include "src/types.h"

#define DMA_DESC_OWNER_CPU 0u
#define DMA_DESC_OWNER_DMA 1u

typedef struct dma_desc {
    uint32_t ctrl;
    const uint8_t *buf;
    struct dma_desc *next;
} __attribute__((packed, aligned(4))) dma_desc_t;

#define DMA_DESC_SIZE_S    0
#define DMA_DESC_SIZE_M    (0xFFFu << DMA_DESC_SIZE_S)
#define DMA_DESC_LENGTH_S  12
#define DMA_DESC_LENGTH_M  (0xFFFu << DMA_DESC_LENGTH_S)
#define DMA_DESC_SOSF      (1u << 29)
#define DMA_DESC_EOF       (1u << 30)
#define DMA_DESC_OWNER     (1u << 31)

#define DMA_MAX_DESC_LEN 4095u

void dma_desc_init(dma_desc_t *desc, const uint8_t *buf, uint32_t len, int eof);
uint32_t dma_build_chain(dma_desc_t *descs, uint32_t count, const uint8_t *buf, uint32_t len);

#endif /* DMA_H */
