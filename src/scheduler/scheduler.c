#include "src/scheduler/scheduler.h"
#include "src/interrupts/interrupts.h"
#include "src/memory_manager/memory.h"
#include "src/scheduler/spinlock.h"
#include "src/kernel/kernel.h"

/* Cycle CPU à 240 MHz => ~1 ms entre deux ticks. A ajuster selon ta
 * fréquence CPU réelle si tu configures les PLL différemment. */
#define TICK_CYCLES 240000

/* Bit d'IRQ correspondant au timer interne CCOMPARE0 sur le mapping
 * d'interruptions par défaut de l'ESP32 (Xtensa LX6) : interruption 6,
 * niveau 1. C'est un timer PAR COEUR (chaque coeur a son propre CCOMPARE0/
 * CCOUNT), donc scheduler_start() doit être appelé sur CHAQUE coeur. */
#define TIMER0_IRQ_LINE 6

static task_t tasks[MAX_TASKS];
static spinlock_t tasks_lock = SPINLOCK_INIT;

/* Index de la tâche courante, PAR COEUR (0 = PRO_CPU, 1 = APP_CPU). */
static volatile int current_task_idx[2] = { -1, -1 };

/* Curseur round-robin UNIQUE et PARTAGÉ (protégé par tasks_lock) : les deux
 * coeurs piochent dans la même file circulaire, ce qui permet à une tâche
 * flottante (TASK_ANY_CORE) de reprendre sur n'importe quel coeur, pas
 * forcément celui où elle avait tourné la fois d'avant. */
static int global_cursor = -1;

uint32_t get_core_id(void) {
    uint32_t prid;
    __asm__ volatile ("rsr.prid %0" : "=r"(prid));
    return (prid >> 13) & 1u;
}

/* Trampoline de fin de tâche : si une tâche "retourne" normalement au lieu
 * de boucler indéfiniment, elle atterrit ici plutôt que de sauter dans de
 * la mémoire arbitraire (le contenu de a0 au moment du ret). */
static void task_exit(void) {
    uint32_t core = get_core_id();

    spinlock_acquire(&tasks_lock);
    int cur = current_task_idx[core];
    if (cur >= 0) {
        tasks[cur].state = TASK_UNUSED;
    }
    spinlock_release(&tasks_lock);

    while (1) {
        __asm__ volatile ("waiti 0");
    }
}

void scheduler_init(void) {
    spinlock_acquire(&tasks_lock);
    for (int i = 0; i < MAX_TASKS; i++) {
        tasks[i].state = TASK_UNUSED;
        tasks[i].sp = 0;
        tasks[i].stack = 0;
        tasks[i].pinned_core = TASK_ANY_CORE;
        tasks[i].running_on = TASK_ANY_CORE;
    }
    global_cursor = -1;
    spinlock_release(&tasks_lock);

    current_task_idx[0] = -1;
    current_task_idx[1] = -1;
}

/* Prépare le cpu_context_t initial d'une tâche, en haut de sa pile dédiée,
 * exactement comme s'il venait d'y être écrit par une vraie interruption
 * (voir SaveCpuContext dans vector.S). Commun aux deux variantes. */
static int task_create_common(void (*entry)(void), uint8_t pinned_core) {
    spinlock_acquire(&tasks_lock);

    int slot = -1;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_UNUSED) { slot = i; break; }
    }
    if (slot < 0) {
        spinlock_release(&tasks_lock);
        return -1;
    }

    /* Réservation immédiate du slot avant de relâcher le verrou : évite
     * qu'un autre coeur ne pioche le même slot pendant l'initialisation. */
    tasks[slot].state = TASK_BLOCKED;
    spinlock_release(&tasks_lock);

    void *stack = nmap(TASK_STACK_SIZE);
    if (stack == NULL) {
        tasks[slot].state = TASK_UNUSED;
        return -1;
    }

    uint8_t *stack_top = (uint8_t *)stack + TASK_STACK_SIZE;
    cpu_context_t *ctx = (cpu_context_t *)(stack_top - sizeof(cpu_context_t));

    for (uint32_t *p = (uint32_t *)ctx; p < (uint32_t *)stack_top; p++) *p = 0;

    ctx->sp_orig  = (uint32_t)stack_top;   /* sp que la tâche aura une fois restaurée */
    ctx->epc      = (uint32_t)entry;       /* PC de départ = fonction de la tâche */
    ctx->a0       = (uint32_t)task_exit;   /* si entry() retourne -> atterrit ici */
    ctx->exccause = EXCCAUSE_LEVEL1_INTERRUPT;

    /* PS "normale" (interruptions autorisées, niveau utilisateur 0). Une
     * tâche flottante doit avoir un PS valide identiquement interprétable
     * sur les deux coeurs : rien de spécifique à un coeur n'est encodé
     * dans PS ici (call0 -> pas de WOE/CALLINC à se soucier), donc c'est
     * sans risque de migrer. */
    uint32_t ps;
    __asm__ volatile ("rsr %0, ps" : "=r"(ps));
    ctx->ps = ps;

    tasks[slot].sp          = (uint32_t *)ctx;
    tasks[slot].stack       = stack;
    tasks[slot].pinned_core = pinned_core;
    tasks[slot].running_on  = TASK_ANY_CORE;
    tasks[slot].id          = (uint32_t)slot;
    tasks[slot].state       = TASK_READY;

    return slot;
}

int task_create(void (*entry)(void)) {
    return task_create_common(entry, TASK_ANY_CORE);
}

int task_create_pinned(void (*entry)(void), uint8_t core) {
    if (core > 1) return -1;
    return task_create_common(entry, core);
}

/* Une tâche est éligible pour "core" si elle est flottante, ou épinglée à
 * ce coeur précis. */
static inline int task_eligible_for(const task_t *t, uint32_t core) {
    return t->pinned_core == TASK_ANY_CORE || t->pinned_core == core;
}

uint32_t *schedule_next_task(uint32_t *current_sp) {
    uint32_t core = get_core_id();

    spinlock_acquire(&tasks_lock);

    /* Sauve le sp courant dans la tâche qu'on quitte (si elle existe). */
    int cur = current_task_idx[core];
    if (cur >= 0 && tasks[cur].state == TASK_RUNNING) {
        tasks[cur].sp = current_sp;
        tasks[cur].state = TASK_READY;
        tasks[cur].running_on = TASK_ANY_CORE;
    }

    /* Round-robin sur le curseur GLOBAL (partagé entre les deux coeurs) :
     * une tâche flottante laissée READY par le PRO_CPU peut donc être
     * reprise l'instant d'après par l'APP_CPU, et inversement. */
    int start = global_cursor;
    int next = -1;
    for (int i = 1; i <= MAX_TASKS; i++) {
        int idx = (start + i + MAX_TASKS) % MAX_TASKS;
        if (tasks[idx].state == TASK_READY && task_eligible_for(&tasks[idx], core)) {
            next = idx;
            break;
        }
    }

    if (next < 0) {
        /* Rien d'éligible pour ce coeur : on continue avec le contexte
         * courant (ou on reste idle si rien ne tournait déjà). */
        if (cur >= 0) {
            tasks[cur].state = TASK_RUNNING;
            tasks[cur].running_on = core;
        }
        spinlock_release(&tasks_lock);
        return current_sp;
    }

    tasks[next].state = TASK_RUNNING;
    tasks[next].running_on = core;
    current_task_idx[core] = next;
    global_cursor = next;   /* partagé : l'autre coeur repartira d'ici */
    uint32_t *new_sp = tasks[next].sp;

    spinlock_release(&tasks_lock);

    return new_sp;
}

void scheduler_start(void) {
    uint32_t core = get_core_id();

    /* Indispensable ICI et pas seulement dans interrupts_init() : VECBASE
     * est un registre PROPRE À CHAQUE COEUR (voir interrupts_init_this_core
     * dans interrupts.c). */
    interrupts_init_this_core();

    /* On NE marque PAS de tâche comme RUNNING ici : le CPU exécute encore
     * le code de l'appelant (kernel_main/app_cpu_main), pas une tâche
     * réelle. On laisse current_task_idx[core] à -1 : schedule_next_task()
     * choisira la première tâche disponible au tout premier tick. */
    current_task_idx[core] = -1;

    interrupts_enable_line(TIMER0_IRQ_LINE);
    set_cpu_private_timer(0, TICK_CYCLES);
    interrupts_enable_global();

    /* A partir d'ici, le CPU tourne dans les tâches ordonnancées par les
     * ticks ; cette fonction ne retourne jamais si au moins une tâche est
     * disponible pour ce coeur. */
    while (1) {
        __asm__ volatile ("waiti 0");
    }
}
