#include "src/kernel/kernel.h"

void uart_putchar(char c) {
    while (((UART0_STATUS >> 16) & 0xFF) >= 128);

    UART0_FIFO = (uint32_t)c;
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

static mutex_t demo_print_mutex = MUTEX_INIT;

static void safe_sys_print(const char *str) {
    mutex_lock(&demo_print_mutex);
    sys_print(str);
    mutex_unlock(&demo_print_mutex);
}

#define SD_SCLK 18
#define SD_MOSI 23
#define SD_MISO 19
#define SD_CS   5

#define SD_STRING_TEST_BLOCK 2000001u   /* différent de SD_TEST_BLOCK, pour ne pas se marcher dessus */

static void sd_string_test_task(void) {
    const char *message = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";

    /* Manquait : sans ça, la carte n'est jamais initialisée (SPI/GPIO/CS
     * jamais configurés) et l'écriture échoue silencieusement. */
    safe_sys_print("[sd-string] initialisation de la carte...\n");
    if (sd_init(SD_SCLK, SD_MOSI, SD_MISO, SD_CS) != 0) {
        safe_sys_print("[sd-string] ECHEC : initialisation de la carte\n");
        sys_exit();
    }
    safe_sys_print("[sd-string] carte initialisee.\n");

    safe_sys_print("[sd-string] test : ecriture d'une chaine personnalisee...\n");

    uint8_t *sector = (uint8_t *)nmap(SD_BLOCK_SIZE);
    if (!sector) {
        safe_sys_print("[sd-string] ECHEC : allocation memoire\n");
        sys_exit();
    }

    /* Prépare le secteur : la chaîne + son octet nul, le reste à zéro
     * (pour que la relecture affiche proprement une chaîne terminée). */
    memset(sector, 0, SD_BLOCK_SIZE);
    size_t msg_len = strlen(message);
    if (msg_len >= SD_BLOCK_SIZE) msg_len = SD_BLOCK_SIZE - 1;   /* sécurité, jamais atteint ici */
        memcpy(sector, message, msg_len);

    if (sd_write_block(SD_STRING_TEST_BLOCK, sector) != 0) {
        safe_sys_print("[sd-string] ECHEC : ecriture\n");
        unmap(sector);
        sys_exit();
    }
    safe_sys_print("[sd-string] ecriture reussie.\n");

    uint8_t *readback = (uint8_t *)nmap(SD_BLOCK_SIZE);
    if (!readback || sd_read_block(SD_STRING_TEST_BLOCK, readback) != 0) {
        safe_sys_print("[sd-string] ECHEC : relecture\n");
        if (readback) unmap(readback);
        unmap(sector);
        sys_exit();
    }

    /* Garantit une terminaison NUL avant d'imprimer, quoi qu'il arrive
     * (si les données relues étaient corrompues et ne contenaient plus le
     * NUL attendu, on ne veut pas déborder en lisant au-delà du secteur). */
    readback[SD_BLOCK_SIZE - 1] = '\0';

    /* CORRIGÉ : un seul verrouillage, des uart_print() bruts à l'intérieur
     * -- appeler safe_sys_print() ici aurait re-verrouillé un mutex déjà
     * tenu par la même tâche (mutex NON réentrant -> deadlock garanti). */
    mutex_lock(&demo_print_mutex);
    uart_print("[sd-string] chaine relue depuis la carte SD : \"");
    uart_print((const char *)readback);
    uart_print("\"\n");
    mutex_unlock(&demo_print_mutex);

    int ok = (memcmp(sector, readback, msg_len + 1) == 0);   /* +1 pour inclure le octet nul */
    safe_sys_print(ok ? "[sd-string] OK : la chaine relue correspond exactement a celle ecrite.\n"
    : "[sd-string] ECHEC : la chaine relue ne correspond PAS a celle ecrite !\n");

    unmap(sector);
    unmap(readback);
    sys_exit();
}

void kernel_main(void) {
    wdt_disable_all();
    mm_init();
    interrupts_init();
    scheduler_init();
    start_app_cpu();
    led_init();

    task_create(echo);
    //task_create_user(user_task_good);
    //task_create(sd_test_task);
    task_create(sd_string_test_task);

    scheduler_start();
}
