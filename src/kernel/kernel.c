#include "src/kernel/kernel.h"

/* ------------------------------------------------------------------- */
/* UART bas niveau                                                      */
/* ------------------------------------------------------------------- */

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

/* Toutes les tâches de test impriment via ceci plutôt que uart_print()
 * directement : sans ça, deux tâches qui impriment au même moment (y
 * compris sur les deux coeurs en parallèle, puisque nos tâches sont
 * flottantes) s'entrelaceraient sur la sortie série. */
static mutex_t print_mutex = MUTEX_INIT;

static void safe_print(const char *str) {
    mutex_lock(&print_mutex);
    uart_print(str);
    mutex_unlock(&print_mutex);
}

/* Petite pause "active" pour espacer les affichages et rendre le
 * comportement lisible sur le moniteur série (pas un vrai sleep, juste de
 * l'attente occupée -- suffisant pour un test). */
static void busy_delay(int spins) {
    for (volatile int i = 0; i < spins; i++) { __asm__ volatile ("nop"); }
}

/* ------------------------------------------------------------------- */
/* Shell UART (déjà existant)                                           */
/* ------------------------------------------------------------------- */

void echo(void) {
    UART0_CONF0_REG |= UART_RXFIFO_RST;
    UART0_CONF0_REG &= ~UART_RXFIFO_RST;
    UART0_INT_CLR_REG = UART_RX_ERROR_MASK;

    uart_print("Input > ");

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
            } else if (c == '\b' || c == 0x7F) {
                uart_print("\b \b");
            } else {
                uart_putchar(c);
            }
        }
    }
}

/* ------------------------------------------------------------------- */
/* Test 1 : mutex + sémaphore -- producteur / consommateur               */
/* ------------------------------------------------------------------- */

#define QUEUE_CAP 4

static int queue_buf[QUEUE_CAP];
static int queue_head = 0;
static int queue_tail = 0;
static int queue_next_value = 0;

static mutex_t     queue_mutex     = MUTEX_INIT;
static semaphore_t queue_items     = SEMAPHORE_INIT(0);          /* nb d'éléments disponibles */
static semaphore_t queue_free_slots = SEMAPHORE_INIT(QUEUE_CAP); /* nb de places libres */

static void producer_task(void) {
    while (1) {
        sem_wait(&queue_free_slots);   /* attend qu'il y ait de la place */

        mutex_lock(&queue_mutex);
        int value = queue_next_value++;
        queue_buf[queue_tail] = value;
        queue_tail = (queue_tail + 1) % QUEUE_CAP;
        mutex_unlock(&queue_mutex);

        sem_post(&queue_items);        /* signale qu'un élément est prêt */

        safe_print("[Producteur] a depose  ");
        uart_print_hex((uint32_t)value);
        safe_print("\n");

        busy_delay(300000);
    }
}

static void consumer_task(void) {
    while (1) {
        sem_wait(&queue_items);        /* attend qu'un élément soit dispo */

        mutex_lock(&queue_mutex);
        int value = queue_buf[queue_head];
        queue_head = (queue_head + 1) % QUEUE_CAP;
        mutex_unlock(&queue_mutex);

        sem_post(&queue_free_slots);   /* libère une place */

        safe_print("[Consommateur] a recu  ");
        uart_print_hex((uint32_t)value);
        safe_print("\n");

        busy_delay(500000);
    }
}

/* ------------------------------------------------------------------- */
/* Test 2 : condition variable                                          */
/* ------------------------------------------------------------------- */

static mutex_t ready_mutex = MUTEX_INIT;
static cond_t  ready_cond  = COND_INIT;
static int     ready_flag  = 0;

static void waiter_task(void) {
    while (1) {
        mutex_lock(&ready_mutex);
        while (!ready_flag) {
            cond_wait(&ready_cond, &ready_mutex);   /* relache ready_mutex pendant l'attente */
        }
        ready_flag = 0;   /* consomme le signal */
        mutex_unlock(&ready_mutex);

        safe_print("[CondVar] waiter_task reveille par signal !\n");
    }
}

static void signaler_task(void) {
    while (1) {
        busy_delay(2000000);   /* simule un travail avant de signaler */

        mutex_lock(&ready_mutex);
        ready_flag = 1;
        mutex_unlock(&ready_mutex);

        safe_print("[CondVar] signaler_task envoie le signal.\n");
        cond_signal(&ready_cond);
    }
}

/* ------------------------------------------------------------------- */
/* Test 3 : event group -- attente ANY sur deux sources                 */
/* ------------------------------------------------------------------- */

#define EVENT_BIT_A (1u << 0)
#define EVENT_BIT_B (1u << 1)

static event_group_t demo_events;

static void event_source_a_task(void) {
    while (1) {
        busy_delay(1500000);
        safe_print("[EventGroup] source A positionne EVENT_BIT_A\n");
        event_group_set_bits(&demo_events, EVENT_BIT_A);
    }
}

static void event_source_b_task(void) {
    while (1) {
        busy_delay(2500000);
        safe_print("[EventGroup] source B positionne EVENT_BIT_B\n");
        event_group_set_bits(&demo_events, EVENT_BIT_B);
    }
}

static void event_waiter_task(void) {
    while (1) {
        /* ANY : se réveille dès qu'AU MOINS UN des deux bits est mis,
         * et les efface (clear_on_exit = 1) pour la prochaine attente. */
        uint32_t bits = event_group_wait_bits(&demo_events,
                                              EVENT_BIT_A | EVENT_BIT_B,
                                              1 /* clear_on_exit */,
                                              0 /* wait_for_all = ANY */);

        mutex_lock(&print_mutex);
        uart_print("[EventGroup] waiter reveille, bits observes = ");
        uart_print_hex(bits);
        uart_print("\n");
        mutex_unlock(&print_mutex);
    }
}

/* ------------------------------------------------------------------- */
/* kernel_main                                                          */
/* ------------------------------------------------------------------- */

void kernel_main(void) {
    wdt_disable_all();
    mm_init();
    interrupts_init();
    scheduler_init();
    start_app_cpu();

    event_group_init(&demo_events);

    uart_print("Hello world !\n");
    uart_print("Demarrage des taches de test (mutex/sem/condvar/eventgroup)...\n");

    /* Producteur / consommateur (mutex + semaphores) */
    task_create(producer_task);
    task_create(consumer_task);

    /* Condition variable */
    task_create(waiter_task);
    task_create(signaler_task);

    /* Event group */
    task_create(event_source_a_task);
    task_create(event_source_b_task);
    task_create(event_waiter_task);

    /* Shell UART existant */
    task_create(echo);

    scheduler_start();
}
