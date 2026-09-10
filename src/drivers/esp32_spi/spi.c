#include "src/drivers/esp32_spi/spi.h"
#include "src/drivers/esp32_gpio/gpio.h"
#include "src/drivers/esp32_dma/dma.h"
#include "src/interrupts/interrupts.h"
#include "src/memory_manager/memory.h"
#include "src/scheduler/semaphore.h"

extern void uart_print(const char *str);
extern void uart_print_hex(uint32_t val);

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

static semaphore_t spi_dma_done = SEMAPHORE_INIT(0);

static void spi_isr(void *arg) {
    (void)arg;
    REG32(SPI_SLAVE_REG) &= ~SPI_TRANS_DONE;
    sem_post(&spi_dma_done);
}

static void spi_route_output(uint8_t pin, uint32_t signal_idx) {
    gpio_set_mode(pin, GPIO_MODE_OUTPUT, GPIO_PULL_NONE);
    REG32(GPIO_FUNC0_OUT_SEL_CFG_REG + 4u * pin) = signal_idx;
}

static void spi_route_input(uint8_t pin, uint32_t signal_idx) {
    gpio_set_mode(pin, GPIO_MODE_INPUT, GPIO_PULL_NONE);
    REG32(GPIO_FUNC0_IN_SEL_CFG_REG + 4u * signal_idx) = pin | GPIO_SIG_IN_SEL;
}

void spi_init(const spi_config_t *cfg) {
    REG32(DPORT_PERIP_CLK_EN_REG) |= DPORT_SPI2_CLK_EN;
    REG32(DPORT_PERIP_RST_EN_REG) &= ~DPORT_SPI2_RST;

    spi_route_output(cfg->sclk, HSPICLK_OUT_IDX);
    spi_route_output(cfg->mosi, HSPID_OUT_IDX);
    spi_route_input(cfg->miso, HSPIQ_IN_IDX);

    if (cfg->manual_cs) {
        gpio_set_mode(cfg->cs, GPIO_MODE_OUTPUT, GPIO_PULL_NONE);
        gpio_write(cfg->cs, 1);
    } else {
        spi_route_output(cfg->cs, HSPICS0_OUT_IDX);
    }

    REG32(SPI_CTRL_REG) = 0;
    REG32(SPI_USER_REG) = SPI_USR_MOSI | SPI_USR_MISO | SPI_DOUTDIN;

    uint32_t div = cfg->clock_div < 2 ? 2 : cfg->clock_div;
    uint32_t pre, n;
    if (div <= 64) {
        pre = 0;
        n = div - 1;
    } else {
        n = 63;                            /* (n+1) = 64, le maximum pour ce champ */
        pre = (div + 63) / 64 - 1;          /* ceil(div/64) - 1 */
    }
    uint32_t h = ((n + 1) / 2) - 1;
    REG32(SPI_CLOCK_REG) = (pre << 18) | (n << 12) | (h << 6) | n; /* CLK_EQU_SYSCLK=0 */

    REG32(DPORT_PRO_SPI2_DMA_INT_MAP_REG) = SPI2_IRQ_LINE;
    interrupts_register_handler(SPI2_IRQ_LINE, spi_isr, 0);
}

void spi_transfer(const uint8_t *tx, uint8_t *rx, uint32_t len) {
    uint32_t offset = 0;

    while (offset < len) {
        uint32_t chunk = len - offset;
        if (chunk > 64) chunk = 64;

            for (uint32_t i = 0; i < chunk; i += 4) {
                uint32_t word = 0;
                for (uint32_t b = 0; b < 4 && (i + b) < chunk; b++) {
                    uint8_t byte = tx ? tx[offset + i + b] : 0;
                    word |= (uint32_t)byte << (8 * b);
                }
                REG32(SPI_W0_REG + i) = word;
            }

            REG32(SPI_MOSI_DLEN_REG) = (chunk * 8) - 1;
            REG32(SPI_MISO_DLEN_REG) = (chunk * 8) - 1;

            REG32(SPI_CMD_REG) = SPI_USR;
            int timeout = 4000000;
            while ((REG32(SPI_CMD_REG) & SPI_USR) && --timeout > 0) { __asm__ volatile ("nop"); }
            if (timeout <= 0) {
                uart_print("[spi][diag] spi_transfer: TIMEOUT, SPI_CMD_REG bloque a ");
                uart_print_hex(REG32(SPI_CMD_REG));
                uart_print(" (offset dans le transfert="); uart_print_hex(offset); uart_print(")\n");
            }

            if (rx) {
                for (uint32_t i = 0; i < chunk; i += 4) {
                    uint32_t word = REG32(SPI_W0_REG + i);
                    for (uint32_t b = 0; b < 4 && (i + b) < chunk; b++) {
                        rx[offset + i + b] = (uint8_t)(word >> (8 * b));
                    }
                }
            }

            offset += chunk;
    }
}

void spi_transfer_dma(const uint8_t *tx, uint8_t *rx, uint32_t len) {
    static int irq_enabled = 0;
    if (!irq_enabled) {
        interrupts_enable_line(SPI2_IRQ_LINE);
        irq_enabled = 1;
    }

    while (sem_trywait(&spi_dma_done)) {}

    uart_print("[spi][diag] entree spi_transfer_dma, len="); uart_print_hex(len); uart_print("\n");

    if (len == 0) return;

    uint32_t desc_count = (len + DMA_MAX_DESC_LEN - 1) / DMA_MAX_DESC_LEN;
    uart_print("[spi][diag] desc_count="); uart_print_hex(desc_count); uart_print("\n");

    dma_desc_t *tx_descs = (dma_desc_t *)nmap(desc_count * sizeof(dma_desc_t));
    dma_desc_t *rx_descs = (dma_desc_t *)nmap(desc_count * sizeof(dma_desc_t));
    uart_print("[spi][diag] tx_descs="); uart_print_hex((uint32_t)tx_descs);
    uart_print(" rx_descs="); uart_print_hex((uint32_t)rx_descs); uart_print("\n");
    if (!tx_descs || !rx_descs) {
        uart_print("[spi][diag] ECHEC allocation descripteurs\n");
        return;
    }

    dma_build_chain(tx_descs, desc_count, tx, len);
    dma_build_chain(rx_descs, desc_count, rx, len);
    uart_print("[spi][diag] descripteurs construits, ctrl tx[0]="); uart_print_hex(tx_descs[0].ctrl); uart_print("\n");

    REG32(DPORT_SPI_DMA_CHAN_SEL_REG) =
    (REG32(DPORT_SPI_DMA_CHAN_SEL_REG) & ~SPI2_DMA_CHAN_SEL_M) |
    (SPI2_DMA_CHANNEL << SPI2_DMA_CHAN_SEL_S);

    REG32(SPI_DMA_CONF_REG) |= SPI_OUT_RST | SPI_IN_RST | SPI_AHBM_RST | SPI_AHBM_FIFO_RST;
    REG32(SPI_DMA_CONF_REG) &= ~(SPI_OUT_RST | SPI_IN_RST | SPI_AHBM_RST | SPI_AHBM_FIFO_RST);

    REG32(SPI_MOSI_DLEN_REG) = (len * 8) - 1;
    REG32(SPI_MISO_DLEN_REG) = (len * 8) - 1;

    REG32(SPI_DMA_OUT_LINK_REG) = ((uint32_t)tx_descs & SPI_LINK_ADDR_MASK) | SPI_OUTLINK_START;
    REG32(SPI_DMA_IN_LINK_REG)  = ((uint32_t)rx_descs & SPI_LINK_ADDR_MASK) | SPI_INLINK_START;

    REG32(SPI_SLAVE_REG) |= SPI_TRANS_DONE_INT_ENA;

    uart_print("[spi][diag] declenchement SPI_CMD_REG=SPI_USR...\n");
    REG32(SPI_CMD_REG) = SPI_USR;
    uart_print("[spi][diag] declenche, attente...\n");

    int got_sem = 0;
    for (volatile int i = 0; i < 4000000 && !got_sem; i++) {
        got_sem = sem_trywait(&spi_dma_done);
    }

    if (!got_sem) {
        uart_print("[spi][diag] semaphore jamais poste (IRQ ligne "); uart_print_hex(SPI2_IRQ_LINE);
        uart_print(" suspecte) -- verification directe du registre matériel...\n");

        int timeout = 8000000;
        while ((REG32(SPI_CMD_REG) & SPI_USR) && --timeout > 0) { __asm__ volatile ("nop"); }

        if (timeout <= 0) {
            uart_print("[spi][diag] SPI_CMD_REG.USR ne se libere JAMAIS : le transfert materiel lui-meme ne termine pas.\n");
        } else {
            uart_print("[spi][diag] Le transfert materiel a bien termine (USR libere) : seule l'interruption ne s'est pas declenchee.\n");
            got_sem = 1;
        }
    }

    unmap(tx_descs);
    unmap(rx_descs);
}
