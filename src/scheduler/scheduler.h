#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "src/types.h"

#define MAX_TASKS         16
#define TASK_STACK_SIZE   2048   /* octets par tâche, ajuste selon tes besoins */

/* Valeur spéciale de task_t.pinned_core : la tâche peut être exécutée par
 * N'IMPORTE QUEL coeur, choisi dynamiquement à chaque tick (pas d'affinité
 * fixe). C'est la valeur utilisée par task_create(). */
#define TASK_ANY_CORE 0xFFu

typedef enum {
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
} task_state_t;

typedef struct task {
    uint32_t     *sp;           /* pointeur vers le cpu_context_t sauvegardé en haut de la pile */
    void         *stack;        /* base de la pile allouée (pour libération éventuelle) */
    task_state_t  state;
    uint8_t       pinned_core;  /* 0, 1, ou TASK_ANY_CORE (tâche "flottante") */
    uint8_t       running_on;   /* coeur qui l'exécute EN CE MOMENT (si state == RUNNING) */
    uint32_t      id;
} task_t;

/* A appeler UNE FOIS, depuis le PRO_CPU, avant de démarrer l'APP_CPU et
 * avant d'armer le tick timer sur l'un ou l'autre coeur. */
void scheduler_init(void);

/* Crée une tâche FLOTTANTE : elle pourra être exécutée indifféremment par
 * le PRO_CPU ou l'APP_CPU, au gré du round-robin (elle peut donc migrer
 * d'un coeur à l'autre entre deux réveils -- c'est sans risque ici : les
 * deux coeurs partagent la même RAM de façon cohérente, aucun cache par
 * coeur ne vient invalider les piles/tas en SRAM interne).
 * Retourne l'id de la tâche (>=0) ou -1 si la table est pleine. */
int task_create(void (*entry)(void));

/* Variante épinglée : la tâche ne s'exécutera JAMAIS ailleurs que sur le
 * coeur indiqué (0 = PRO_CPU, 1 = APP_CPU). Utile pour du code qui dépend
 * de ressources propres à un coeur (ex: un périphérique câblé à un coeur
 * précis, ou un besoin de latence déterministe sur un coeur donné). */
int task_create_pinned(void (*entry)(void), uint8_t core);

/* Démarre l'ordonnancement sur LE COEUR APPELANT : configure les registres
 * d'interruption de ce coeur, arme le tick timer et bascule sur la première
 * tâche disponible pour lui. Ne retourne jamais si au moins une tâche est
 * disponible pour ce coeur. A appeler en tout dernier, sur CHAQUE coeur. */
void scheduler_start(void);

/* Appelée depuis le tick timer (c_interrupt_handler, niveau 1) : choisit la
 * prochaine tâche disponible pour CE coeur en round-robin global et
 * retourne le nouveau sp à restaurer. Ne pas appeler directement depuis du
 * code applicatif. */
uint32_t *schedule_next_task(uint32_t *current_sp);
uint32_t get_core_id(void);

#endif /* SCHEDULER_H */
