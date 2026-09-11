#include "src/file_system/devfs/devfs.h"

void uart0_hw_putchar(char c) {
    while (((UART0_STATUS >> 16) & 0xFFu) >= 128) { __asm__ volatile ("nop"); }
    UART0_FIFO = (uint32_t)c;
}

int uart0_hw_read(void *buf, uint32_t len) {
    uint8_t *out = (uint8_t *)buf;
    uint32_t n = 0;
    while (n < len && UART0_RX_FIFO_CNT > 0) {
        out[n++] = (uint8_t)(UART0_FIFO & 0xFFu);
    }
    return (int)n;
}

/* ---- /dev/uart0 --------------------------------------------------------
 * write() bloquant (attend que la FIFO TX ait de la place, comme
 * uart_putchar). read() NON bloquant : retourne immédiatement ce qui est
 * déjà dans le FIFO RX (0 si rien -- à l'appelant de réessayer/attendre
 * s'il veut du bloquant). */

static int uart0_read(void *ctx, void *buf, uint32_t len) {
    (void)ctx;
    return uart0_hw_read(buf, len);
}

static int uart0_write(void *ctx, const void *buf, uint32_t len) {
    (void)ctx;
    const uint8_t *in = (const uint8_t *)buf;
    for (uint32_t i = 0; i < len; i++) {
        uart0_hw_putchar((char)in[i]);
    }
    return (int)len;
}

static const ram_fs_dev_ops_t uart0_ops = {
    .read = uart0_read,
    .write = uart0_write,
    .ioctl = 0,
};

/* ---- /dev/spi2 -----------------------------------------------------------
 * write() envoie buf sur MOSI (ignore ce qui revient sur MISO).
 * read() clocke des 0xFF sur MOSI et récupère ce qui revient sur MISO --
 * c'est le seul moyen de "lire" en SPI (bus synchrone, pas d'émission
 * spontanée par l'esclave). CS est géré automatiquement par spi_transfer()
 * (matériel, une assertion par appel) -- voir spi.h pour les limites
 * (64 octets max par appel dans cette voie, pas de mode manuel ici). */

static int spi2_read(void *ctx, void *buf, uint32_t len) {
    (void)ctx;
    spi_transfer(NULL, (uint8_t *)buf, len);
    return (int)len;
}

static int spi2_write(void *ctx, const void *buf, uint32_t len) {
    (void)ctx;
    spi_transfer((const uint8_t *)buf, NULL, len);
    return (int)len;
}

static const ram_fs_dev_ops_t spi2_ops = {
    .read = spi2_read,
    .write = spi2_write,
    .ioctl = 0,
};

void dev_fs_register_hw_devices(void) {
    ram_fs_mknod("/dev/uart0", &uart0_ops, 0);
    ram_fs_mknod("/dev/spi2", &spi2_ops, 0);
}
