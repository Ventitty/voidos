#ifndef SPI_H
#define SPI_H

#include "src/types.h"

typedef struct {
    uint8_t  sclk;
    uint8_t  mosi;
    uint8_t  miso;
    uint8_t  cs;
    uint32_t clock_div;
    int      manual_cs;
} spi_config_t;

void spi_init(const spi_config_t *cfg);
void spi_transfer(const uint8_t *tx, uint8_t *rx, uint32_t len);
void spi_transfer_dma(const uint8_t *tx, uint8_t *rx, uint32_t len);

#endif /* SPI_H */
