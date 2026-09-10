#include "src/drivers/esp32_dma/dma.h"

void dma_desc_init(dma_desc_t *desc, const uint8_t *buf, uint32_t len, int eof) {
    uint32_t ctrl = (len & DMA_DESC_SIZE_M) | ((len << DMA_DESC_LENGTH_S) & DMA_DESC_LENGTH_M);
    ctrl |= DMA_DESC_OWNER;
    if (eof) ctrl |= DMA_DESC_EOF;

    desc->ctrl = ctrl;
    desc->buf  = buf;
    desc->next = 0;
}

uint32_t dma_build_chain(dma_desc_t *descs, uint32_t count,
                         const uint8_t *buf, uint32_t len) {
    uint32_t needed = (len + DMA_MAX_DESC_LEN - 1) / DMA_MAX_DESC_LEN;
    if (needed == 0) needed = 1;
    if (needed > count) return 0;

    uint32_t offset = 0;
    for (uint32_t i = 0; i < needed; i++) {
        uint32_t chunk = len - offset;
        if (chunk > DMA_MAX_DESC_LEN) chunk = DMA_MAX_DESC_LEN;

        int is_last = (i == needed - 1);
        dma_desc_init(&descs[i], buf + offset, chunk, is_last);

        if (!is_last) {
            descs[i].next = &descs[i + 1];
        }

        offset += chunk;
    }

    return needed;
                         }
