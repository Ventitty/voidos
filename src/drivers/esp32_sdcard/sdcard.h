#ifndef SDCARD_H
#define SDCARD_H

#include "src/types.h"
#include "src/drivers/esp32_spi/spi.h"

#define SD_BLOCK_SIZE 512u

int sd_init(uint8_t sclk, uint8_t mosi, uint8_t miso, uint8_t cs);
int sd_read_block(uint32_t block_addr, uint8_t *buf);
int sd_write_block(uint32_t block_addr, const uint8_t *buf);

#endif /* SDCARD_H */
