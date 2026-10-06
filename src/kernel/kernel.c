#include "src/kernel/kernel.h"

static ram_fs_file_t *g_uart0_file = NULL;
static volatile int uart_in_panic = 0;

void uart_fs_bootstrap(void) {
    g_uart0_file = ram_fs_open("/dev/uart0", RAM_FS_O_WRITE | RAM_FS_O_READ);
}

void uart_enter_panic_mode(void) {
    uart_in_panic = 1;
}

void uart_putchar(char c) {
    if (g_uart0_file != NULL && !uart_in_panic) {
        ram_fs_write(g_uart0_file, &c, 1);
        return;
    }
    uart0_hw_putchar(c);
}

static spinlock_t uart_lock = SPINLOCK_INIT;

static void uart_write_str(const char *str) {
    while (*str != '\0') {
        if (*str == '\n') {
            uart_putchar('\r');
        }
        uart_putchar(*str);
        str++;
    }
}

void uart_print(const char *str) {
    if (uart_in_panic) {
        uart_write_str(str);
        return;
    }
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
                uart_putchar('\r');
                uart_putchar('\n');
                uart_print("Input > ");
                led_toggle();
            } else if (c == '\b' || c == 0x7F) {
                uart_print("\b \b");
            } else {
                uart_putchar(c);
            }
        }
    }
}

#define SD_SCLK 18
#define SD_MOSI 23
#define SD_MISO 19
#define SD_CS   5

static const char *programmes[] = {
    "/sd/BIN/PROG1.ELF",
    "/sd/BIN/PROG2.ELF",
    "/sd/BIN/PROG3.ELF",
};
#define NB_PROGRAMMES (sizeof(programmes) / sizeof(programmes[0]))

static elf_image_t images[NB_PROGRAMMES];

static void ok(int cond, const char *libelle) {
    uart_print(cond ? "  [OK]    " : "  [ECHEC] ");
    uart_print(libelle);
    uart_print("\n");
}

static void test_memoire(void) {
    uart_print("\n--- memoire ---\n");

    void *a = nmap(512);
    void *b = nmap(512);
    ok(a != NULL && b != NULL, "deux allocations sur le tas");
    ok(((uint32_t)a >> 24) == 0x3F, "le tas est bien en DRAM");

    unmap(a);
    void *c = nmap(512);
    ok(c == a, "bloc libere puis repris");
    unmap(b);
    unmap(c);

    elf_loader_print_layout();
}

static void lancer_programmes(void) {
    uart_print("\n--- programmes ---\n");

    uint32_t lances = 0;
    for (uint32_t i = 0; i < NB_PROGRAMMES; i++) {
        if (elf_load_image(programmes[i], &images[i]) != 0) continue;
            if (task_create(images[i].entry) < 0) {
                uart_print("[init] plus de tache disponible\n");
                elf_unload(&images[i]);
                continue;
            }
            lances++;
    }

    uart_print("[init] programmes lances = "); uart_print_hex(lances); uart_print("\n");
    elf_loader_print_layout();
}

static void tache_init(void) {
    uart_print("\n=== voidOS demarre ===\n");

    test_memoire();

    uart_print("\n--- carte SD ---\n");
    if (sd_init(SD_SCLK, SD_MOSI, SD_MISO, SD_CS) != 0) {
        uart_print("[init] pas de carte SD : le noyau tourne sans\n");
    } else if (fat32_mount() != 0) {
        uart_print("[init] carte illisible (formatee en FAT32 ?)\n");
    } else {
        uart_print("[init] racine de la carte :\n");
        fat32_ls("/");
        lancer_programmes();
    }

    for (uint32_t n = 1; ; n++) {
        for (volatile uint32_t d = 0; d < 8000000; d++) { }
        uart_print("[init] battement "); uart_print_hex(n); uart_print("\n");
    }
}

void kernel_main(void) {
    wdt_disable_all();
    mm_init();
    interrupts_init();
    scheduler_init();
    start_app_cpu();
    led_init();

    ram_fs_init();
    ram_fs_mkdir("/dev");
    dev_fs_register_hw_devices();
    uart_fs_bootstrap();

    task_create(echo);
    task_create(tache_init);

    scheduler_start();
}
