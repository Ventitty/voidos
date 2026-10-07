#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "src/types.h"
#include "src/interrupts/interrupts.h"
#include "src/memory_manager/memory.h"
#include "src/scheduler/spinlock.h"

#define STACK_FILL        0xA5A5A5A5u
#define STACK_GUARD_BYTES 32u

typedef enum {
    TASK_READY = 0,   /* prête, en attente d'un cœur                        */
    TASK_RUNNING,     /* en cours d'exécution sur un cœur                   */
    TASK_BLOCKED,     /* en attente d'une primitive (mutex, sémaphore...)   */
    TASK_ZOMBIE,      /* terminée, mémoire libérée au prochain changement   */
} task_state_t;

/* Descripteur de tâche, alloué sur le tas à la création (nmap) et libéré à
 * sa terminaison : le nombre de tâches n'est limité que par la mémoire. */
typedef struct task {
    uint32_t     *sp;            /* contexte sauvegardé (cpu_context_t)     */
    void         *stack;         /* bas de la pile (bloc nmap)              */
    uint32_t      stack_size;
    volatile task_state_t state;
    uint8_t       pinned_core;   /* 0, 1 ou TASK_ANY_CORE                   */
    uint8_t       running_on;    /* cœur qui l'exécute, ou TASK_ANY_CORE    */
    int           id;            /* identifiant unique, jamais réutilisé    */

    struct task  *next;          /* liste globale des tâches ordonnançables */

    /* File d'attente d'une primitive de synchronisation. Une tâche bloquée
     * n'attend qu'une seule chose à la fois : un seul maillon suffit. Ces
     * champs appartiennent à la primitive et sont protégés par SON verrou. */
    struct task  *wait_next;
    uint32_t      wait_mask;     /* event group : bits attendus             */
    uint8_t       wait_all;      /* event group : tous les bits ou un seul  */
} task_t;

/* ---- Files d'attente (FIFO) des primitives de synchronisation ----------
 * À manipuler uniquement sous le verrou de la primitive propriétaire. */

typedef struct {
    task_t *head;
    task_t *tail;
} wait_queue_t;

#define WAIT_QUEUE_INIT { NULL, NULL }

static inline void wait_queue_init(wait_queue_t *q) {
    q->head = NULL;
    q->tail = NULL;
}

static inline void wait_queue_push(wait_queue_t *q, task_t *t) {
    t->wait_next = NULL;
    if (q->tail) q->tail->wait_next = t;
    else         q->head = t;
    q->tail = t;
}

static inline task_t *wait_queue_pop(wait_queue_t *q) {
    task_t *t = q->head;
    if (t) {
        q->head = t->wait_next;
        if (q->head == NULL) q->tail = NULL;
        t->wait_next = NULL;
    }
    return t;
}

/* Détache toute la file et retourne sa tête (chaînée par wait_next). Lire
 * t->wait_next AVANT de réveiller t : une fois réveillée, la tâche peut se
 * remettre en attente ailleurs et réécrire ce champ. */
static inline task_t *wait_queue_take_all(wait_queue_t *q) {
    task_t *t = q->head;
    q->head = NULL;
    q->tail = NULL;
    return t;
}

/* ---- Création ----------------------------------------------------------
 * Retournent l'identifiant de la tâche, ou -1 si la mémoire manque. */

void scheduler_init(void);
int task_create(void (*entry)(void));
int task_create_pinned(void (*entry)(void), uint8_t core);
int task_create_user(void (*entry)(void));
int task_create_user_pinned(void (*entry)(void), uint8_t core);

void scheduler_start(void);
uint32_t *schedule_next_task(uint32_t *current_sp);
uint32_t *scheduler_terminate_current(uint32_t *sp);

/* ---- Tâche courante et blocage ----------------------------------------- */

uint32_t get_core_id(void);
task_t  *scheduler_current_task(void);      /* NULL hors tâche (boot)       */
int      scheduler_current_task_id(void);   /* -1 hors tâche                */

/* Protocole de blocage d'une primitive :
 *   1. sous le verrou de la primitive : wait_queue_push() puis
 *      scheduler_mark_blocked_self() ;
 *   2. relâcher le verrou ;
 *   3. scheduler_yield_blocked(me) : attend que quelqu'un appelle
 *      scheduler_unblock(me). */
task_t *scheduler_mark_blocked_self(void);
void    scheduler_yield_blocked(task_t *me);
void    scheduler_block_current(void);
void    scheduler_unblock(task_t *t);

/* Octets de pile jamais utilisés par la tâche (0 si id inconnu). */
uint32_t scheduler_stack_unused(int task_id);
/* Nombre de tâches existantes (hors tâches idle). */
uint32_t scheduler_task_count(void);

#endif /* SCHEDULER_H */
