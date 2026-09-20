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

void uart_print(const char *str) {
    while (*str != '\0') {
        if (*str == '\n') {
            uart_putchar('\r');
        }
        uart_putchar(*str);
        str++;
    }
}

void uart_print_hex(uint32_t val) {
    const char hex_chars[] = "0123456789ABCDEF";
    uart_print("0x");
    for (int i = 28; i >= 0; i -= 4) {
        uart_putchar(hex_chars[(val >> i) & 0xF]);
    }
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

#define ELF_PROGRAM_PATH "/bin/PROG.ELF"

static void elf_exec_task(void) {
    uart_print("[elf-run] init carte SD...\n");
    if (sd_init(SD_SCLK, SD_MOSI, SD_MISO, SD_CS) != 0) {
        uart_print("[elf-run] ECHEC : init carte SD\n");
        while (1) { __asm__ volatile ("waiti 0"); }
    }

    uart_print("[elf-run] montage FAT32...\n");
    if (fat32_mount() != 0) {
        uart_print("[elf-run] ECHEC : montage FAT32 (carte formatee en FAT32 ?)\n");
        while (1) { __asm__ volatile ("waiti 0"); }
    }

    elf_loader_print_layout();

    uart_print("[elf-run] contenu de la racine de la carte :\n");
    fat32_ls("/");

    uart_print("[elf-run] execution de "); uart_print(ELF_PROGRAM_PATH); uart_print("...\n");
    if (elf_exec(ELF_PROGRAM_PATH) < 0) {
        uart_print("[elf-run] ECHEC : voir les messages [elf] ci-dessus\n");
    }

    while (1) { __asm__ volatile ("waiti 0"); }
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
    task_create(elf_exec_task);

    scheduler_start();
}
