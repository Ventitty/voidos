#include "src/kernel/kernel.h"

static spinlock_t uart_lock = SPINLOCK_INIT;

static void uart_write_str(const char *str) {
    while (*str != '\0') {
        if (*str == '\n') {
            uart0_putchar('\r');
        }
        uart0_putchar(*str);
        str++;
    }
}

void uart_print(const char *str) {
    spinlock_acquire(&uart_lock);
    uart_write_str(str);
    spinlock_release(&uart_lock);
}

void uart_print_hex(uint32_t val) {
    const char hex_chars[] = "0123456789ABCDEF";
    char buf[11];

    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 8; i++) {
        buf[2 + i] = hex_chars[(val >> (28 - 4 * i)) & 0xF];
    }
    buf[10] = '\0';

    uart_print(buf);
}

void echo(void) {
    UART0_CONF0_REG |= UART_RXFIFO_RST;
    UART0_CONF0_REG &= ~UART_RXFIFO_RST;
    UART0_INT_CLR_REG = UART_RX_ERROR_MASK;

    uart_print("Input > ");
    led_toggle();

    while (1) {
        uint32_t int_st = UART0_INT_ST_REG;
        if (int_st & UART_RX_ERROR_MASK) {
            UART0_INT_CLR_REG = UART_RX_ERROR_MASK;
            UART0_CONF0_REG |= UART_RXFIFO_RST;
            UART0_CONF0_REG &= ~UART_RXFIFO_RST;
            continue;
        }

        if (UART0_RX_FIFO_CNT > 0) {
            char c = (char)(UART0_FIFO & 0xFFu);

            if (c == '\r' || c == '\n') {
                uart0_putchar('\r');
                uart0_putchar('\n');
                uart_print("Input > ");
                led_toggle();
            } else if (c == '\b' || c == 0x7F) {
                uart_print("\b \b");
            } else {
                uart0_putchar(c);
            }
        }
    }
}

#define SD_SCLK 18
#define SD_MOSI 23
#define SD_MISO 19
#define SD_CS   5

static int spi2_write(const void *buf, uint32_t len) {
    spi_transfer((const uint8_t *)buf, NULL, len);
    return (int)len;
}

static int spi2_read(void *buf, uint32_t len) {
    spi_transfer(NULL, (uint8_t *)buf, len);
    return (int)len;
}

void kernel_main(void) {
    wdt_disable_all();
    mm_init();
    interrupts_init();
    scheduler_init();
    start_app_cpu();
    led_init();

    vfs_init();

    int sd = vfs_mount_sd(SD_SCLK, SD_MOSI, SD_MISO, SD_CS);
    if (sd == VFS_SD_NO_CARD) {
        uart_print("[init] pas de carte SD : le noyau tourne sans\n");
    } else if (sd == VFS_SD_BAD_FS) {
        uart_print("[init] carte illisible (formatee en FAT32 ?)\n");
    }

    if (vfs_mknod("/dev/uart0", uart0_read, uart0_write) != 0) {
        uart_print("[init] echec creation /dev/uart0\n");
    }
    if (vfs_mknod("/dev/spi2", spi2_read, spi2_write) != 0) {
        uart_print("[init] echec creation /dev/spi2\n");
    }

    task_create(echo);

    scheduler_start();
}
