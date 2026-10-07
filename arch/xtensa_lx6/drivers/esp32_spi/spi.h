#ifndef SPI_H
#define SPI_H

#include "src/types.h"
#include "arch/xtensa_lx6/drivers/esp32_gpio/gpio.h"
#include "arch/xtensa_lx6/drivers/esp32_dma/dma.h"
#include "src/interrupts/interrupts.h"
#include "src/memory_manager/memory.h"
#include "src/scheduler/semaphore.h"

#define REG32(addr) (*(volatile uint32_t *)(addr))

#define DR_REG_DPORT_BASE  0x3FF00000u
#define DR_REG_SPI2_BASE   0x3FF64000u
#define DR_REG_GPIO_BASE   0x3FF44000u

#define SPI_CMD_REG          (DR_REG_SPI2_BASE + 0x00)
#define SPI_CTRL_REG         (DR_REG_SPI2_BASE + 0x08)
#define SPI_CLOCK_REG        (DR_REG_SPI2_BASE + 0x18)
#define SPI_USER_REG         (DR_REG_SPI2_BASE + 0x1C)
#define SPI_MOSI_DLEN_REG    (DR_REG_SPI2_BASE + 0x28)
#define SPI_MISO_DLEN_REG    (DR_REG_SPI2_BASE + 0x2C)
#define SPI_SLAVE_REG        (DR_REG_SPI2_BASE + 0x38)
#define SPI_W0_REG           (DR_REG_SPI2_BASE + 0x80)
#define SPI_DMA_CONF_REG     (DR_REG_SPI2_BASE + 0x100)
#define SPI_DMA_OUT_LINK_REG (DR_REG_SPI2_BASE + 0x104)
#define SPI_DMA_IN_LINK_REG  (DR_REG_SPI2_BASE + 0x108)

#define SPI_USR            (1u << 18)
#define SPI_USR_MISO       (1u << 28)
#define SPI_USR_MOSI       (1u << 27)
#define SPI_DOUTDIN        (1u << 0)

#define SPI_TRANS_DONE          (1u << 4)
#define SPI_TRANS_DONE_INT_ENA  (1u << 9)

#define SPI_OUT_RST        (1u << 3)
#define SPI_IN_RST         (1u << 2)
#define SPI_AHBM_FIFO_RST  (1u << 4)
#define SPI_AHBM_RST       (1u << 5)

#define SPI_OUTLINK_START  (1u << 29)
#define SPI_INLINK_START   (1u << 29)
#define SPI_LINK_ADDR_MASK 0x000FFFFFu

#define DPORT_PERIP_CLK_EN_REG (DR_REG_DPORT_BASE + 0x0C0)
#define DPORT_PERIP_RST_EN_REG (DR_REG_DPORT_BASE + 0x0C4)
#define DPORT_SPI2_CLK_EN      (1u << 6)
#define DPORT_SPI2_RST         (1u << 6)

#define DPORT_SPI_DMA_CHAN_SEL_REG (DR_REG_DPORT_BASE + 0x5A8)
#define SPI2_DMA_CHAN_SEL_S 2
#define SPI2_DMA_CHAN_SEL_M (0x3u << SPI2_DMA_CHAN_SEL_S)
#define SPI2_DMA_CHANNEL    1u

#define DPORT_PRO_SPI2_DMA_INT_MAP_REG (DR_REG_DPORT_BASE + 0x1D8)
#define SPI2_IRQ_LINE 13

#define HSPICLK_OUT_IDX  8
#define HSPIQ_IN_IDX     9   /* MISO */
#define HSPID_OUT_IDX    10  /* MOSI */
#define HSPICS0_OUT_IDX  11

#define GPIO_FUNC0_OUT_SEL_CFG_REG (DR_REG_GPIO_BASE + 0x0530)
#define GPIO_FUNC0_IN_SEL_CFG_REG  (DR_REG_GPIO_BASE + 0x0130)
#define GPIO_SIG_IN_SEL            (1u << 7)

typedef struct {
    uint8_t  sclk;
    uint8_t  mosi;
    uint8_t  miso;
    uint8_t  cs;
    uint32_t clock_div;
    int      manual_cs;
} spi_config_t;

extern void uart_print(const char *str);
extern void uart_print_hex(uint32_t val);

void spi_init(const spi_config_t *cfg);
void spi_transfer(const uint8_t *tx, uint8_t *rx, uint32_t len);
void spi_transfer_dma(const uint8_t *tx, uint8_t *rx, uint32_t len);

#endif /* SPI_H */
