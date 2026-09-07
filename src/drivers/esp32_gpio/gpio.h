#ifndef GPIO_H
#define GPIO_H

#include "src/types.h"

#define GPIO_NUM_MAX 40

typedef enum {
    GPIO_MODE_INPUT = 0,
    GPIO_MODE_OUTPUT,
    GPIO_MODE_INPUT_OUTPUT,   /* sortie avec relecture possible (open-drain simulé) */
} gpio_mode_t;

typedef enum {
    GPIO_PULL_NONE = 0,
    GPIO_PULL_UP,
    GPIO_PULL_DOWN,
} gpio_pull_t;

/* Valeurs alignées sur le champ matériel GPIO_PINn_INT_TYPE (voir TRM) --
 * ne pas réordonner. */
typedef enum {
    GPIO_INTR_DISABLE   = 0,
    GPIO_INTR_RISING    = 1,
    GPIO_INTR_FALLING   = 2,
    GPIO_INTR_ANY_EDGE  = 3,
    GPIO_INTR_LOW_LEVEL = 4,
    GPIO_INTR_HIGH_LEVEL = 5,
} gpio_intr_type_t;

typedef void (*gpio_isr_t)(void *arg);

/* A appeler une fois avant toute autre fonction gpio_*. Route l'interruption
 * GPIO matérielle vers une ligne d'IRQ dédiée sur le PRO_CPU (voir
 * gpio.c) et prépare la table de callbacks par broche. */
void gpio_init(void);

/* Configure une broche. GPIO 6-11 sont câblées à la flash SPI interne :
 * ne JAMAIS les utiliser en usage général (la fonction les accepte sans
 * vérifier, à toi de ne pas t'en servir). GPIO 34-39 sont entrée SEULE
 * (pas de driver de sortie en silicium) : demander GPIO_MODE_OUTPUT dessus
 * ne fait rien côté sortie et est silencieusement ignoré pour ce sous-
 * ensemble de broches. */
void gpio_set_mode(uint8_t pin, gpio_mode_t mode, gpio_pull_t pull);

void gpio_write(uint8_t pin, int level);
int  gpio_read(uint8_t pin);

/* Enregistre (ou remplace) le gestionnaire d'interruption d'une broche et
 * l'active immédiatement. handler est appelé en contexte d'interruption
 * (niveau 1) : pas d'allocation, pas de blocage (mutex/sem), impression
 * UART à éviter directement -- préférer positionner un flag/event_group et
 * traiter le "vrai" travail depuis une tâche. */
void gpio_set_interrupt(uint8_t pin, gpio_intr_type_t type, gpio_isr_t handler, void *arg);

/* Désactive l'interruption d'une broche (ne désinscrit pas le handler,
 * juste GPIO_INTR_DISABLE côté matériel). */
void gpio_clear_interrupt(uint8_t pin);

#endif /* GPIO_H */
