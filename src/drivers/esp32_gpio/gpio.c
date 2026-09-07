#include "src/drivers/esp32_gpio/gpio.h"
#include "src/interrupts/interrupts.h"

#define REG32(addr) (*(volatile uint32_t *)(addr))

/* ---- Bases (vérifiées esp-idf reg_base.h) --------------------------- */
#define DR_REG_GPIO_BASE    0x3FF44000u
#define DR_REG_IO_MUX_BASE  0x3FF49000u
#define DR_REG_DPORT_BASE   0x3FF00000u

/* ---- GPIO (pins 0-31) ------------------------------------------------ */
#define GPIO_OUT_REG            (DR_REG_GPIO_BASE + 0x0004)
#define GPIO_OUT_W1TS_REG       (DR_REG_GPIO_BASE + 0x0008)
#define GPIO_OUT_W1TC_REG       (DR_REG_GPIO_BASE + 0x000C)
#define GPIO_ENABLE_REG         (DR_REG_GPIO_BASE + 0x0020)
#define GPIO_ENABLE_W1TS_REG    (DR_REG_GPIO_BASE + 0x0024)
#define GPIO_ENABLE_W1TC_REG    (DR_REG_GPIO_BASE + 0x0028)
#define GPIO_IN_REG             (DR_REG_GPIO_BASE + 0x003C)
#define GPIO_STATUS_REG         (DR_REG_GPIO_BASE + 0x0044)
#define GPIO_STATUS_W1TC_REG    (DR_REG_GPIO_BASE + 0x004C)
#define GPIO_PIN0_REG           (DR_REG_GPIO_BASE + 0x0088)
#define GPIO_FUNC0_IN_SEL_CFG_REG  (DR_REG_GPIO_BASE + 0x0130)
#define GPIO_FUNC0_OUT_SEL_CFG_REG (DR_REG_GPIO_BASE + 0x0530)

/* ---- GPIO (pins 32-39) ------------------------------------------------ */
#define GPIO_OUT1_REG           (DR_REG_GPIO_BASE + 0x0010)
#define GPIO_OUT1_W1TS_REG      (DR_REG_GPIO_BASE + 0x0014)
#define GPIO_OUT1_W1TC_REG      (DR_REG_GPIO_BASE + 0x0018)
#define GPIO_ENABLE1_REG        (DR_REG_GPIO_BASE + 0x002C)
#define GPIO_ENABLE1_W1TS_REG   (DR_REG_GPIO_BASE + 0x0030)
#define GPIO_ENABLE1_W1TC_REG   (DR_REG_GPIO_BASE + 0x0034)
#define GPIO_IN1_REG            (DR_REG_GPIO_BASE + 0x0040)
#define GPIO_STATUS1_REG        (DR_REG_GPIO_BASE + 0x0050)
#define GPIO_STATUS1_W1TC_REG   (DR_REG_GPIO_BASE + 0x0058)

/* GPIO_PINn_REG(n) = GPIO_PIN0_REG + 4*n pour n = 0..39 (vérifié : PIN32 =
 * PIN0 + 4*32 correspond bien à l'offset publié séparément). */
#define GPIO_PINn_REG(n) (GPIO_PIN0_REG + 4u * (n))
#define GPIO_PIN_INT_TYPE_S   7
#define GPIO_PIN_INT_TYPE_M   (0x7u << GPIO_PIN_INT_TYPE_S)
#define GPIO_PIN_INT_ENA_S    13
#define GPIO_PIN_INT_ENA_M    (0x1Fu << GPIO_PIN_INT_ENA_S)
#define GPIO_PIN_INT_ENA_PRO_APP (0x5u << GPIO_PIN_INT_ENA_S) /* bit0=PRO_CPU, bit2=APP_CPU */

/* ---- IO_MUX ------------------------------------------------------------ */
#define IOMUX_MCU_SEL_S   12
#define IOMUX_MCU_SEL_M   (0x7u << IOMUX_MCU_SEL_S)
#define IOMUX_FUN_PD      (1u << 7)
#define IOMUX_FUN_PU      (1u << 8)
#define IOMUX_FUN_IE      (1u << 9)
#define PIN_FUNC_GPIO     2u

/* ---- Interrupt matrix (DPORT) ------------------------------------------ */
#define DPORT_PRO_GPIO_INTERRUPT_MAP_REG (DR_REG_DPORT_BASE + 0x15C)

/* Ligne d'IRQ CPU choisie pour le GPIO (niveau 1, table Xtensa/ESP32
 * standard -- voir le commentaire dans interrupts.c pour la table
 * complète). Ne pas réutiliser pour un autre périphérique. */
#define GPIO_IRQ_LINE 10

/* Offset IO_MUX (depuis DR_REG_IO_MUX_BASE) pour chaque GPIO 0-39, vérifié
 * contre esp-idf io_mux_reg.h. 0xFFFFFFFF = broche inexistante sur ce
 * boîtier (GPIO 28-31 ne sont pas sorties sur l'ESP32). GPIO 6-11 sont
 * câblées à la flash SPI interne (SD_CLK/DATA0-3/CMD) : présentes dans la
 * table pour rester cohérent avec la numérotation, mais NE JAMAIS les
 * piloter en usage général sous peine de planter la lecture de la flash. */
static const uint32_t iomux_offset[GPIO_NUM_MAX] = {
    /*0*/ 0x44, /*1*/ 0x88, /*2*/ 0x40, /*3*/ 0x84, /*4*/ 0x48,
    /*5*/ 0x6c, /*6*/ 0x60, /*7*/ 0x64, /*8*/ 0x68, /*9*/ 0x54,
    /*10*/0x58, /*11*/0x5c, /*12*/0x34, /*13*/0x38, /*14*/0x30,
    /*15*/0x3c, /*16*/0x4c, /*17*/0x50, /*18*/0x70, /*19*/0x74,
    /*20*/0x78, /*21*/0x7c, /*22*/0x80, /*23*/0x8c, /*24*/0x90,
    /*25*/0x24, /*26*/0x28, /*27*/0x2c,
    /*28*/0xFFFFFFFF, /*29*/0xFFFFFFFF, /*30*/0xFFFFFFFF, /*31*/0xFFFFFFFF,
    /*32*/0x1c, /*33*/0x20, /*34*/0x14, /*35*/0x18,
    /*36*/0x04, /*37*/0x08, /*38*/0x0c, /*39*/0x10,
};

static inline uint32_t iomux_reg(uint8_t pin) {
    return DR_REG_IO_MUX_BASE + iomux_offset[pin];
}

/* GPIO 34-39 n'ont pas de driver de sortie en silicium (entrée seule). */
static inline int gpio_has_output_driver(uint8_t pin) {
    return pin < 34;
}

static gpio_isr_t isr_table[GPIO_NUM_MAX];
static void      *isr_arg[GPIO_NUM_MAX];

static void gpio_isr_dispatch(void *arg) {
    (void)arg;

    uint32_t status0 = REG32(GPIO_STATUS_REG);
    uint32_t status1 = REG32(GPIO_STATUS1_REG);

    for (uint8_t pin = 0; pin < 32; pin++) {
        if (status0 & (1u << pin)) {
            if (isr_table[pin]) isr_table[pin](isr_arg[pin]);
        }
    }
    for (uint8_t pin = 32; pin < GPIO_NUM_MAX; pin++) {
        if (status1 & (1u << (pin - 32))) {
            if (isr_table[pin]) isr_table[pin](isr_arg[pin]);
        }
    }

    /* Acquitte tout ce qui a été vu ci-dessus (write-1-to-clear). */
    REG32(GPIO_STATUS_W1TC_REG)  = status0;
    REG32(GPIO_STATUS1_W1TC_REG) = status1;
}

void gpio_init(void) {
    for (int i = 0; i < GPIO_NUM_MAX; i++) {
        isr_table[i] = 0;
        isr_arg[i] = 0;
    }

    /* Route la source d'interruption matérielle "GPIO" (PRO_CPU) vers la
     * ligne CPU choisie, puis l'enregistre/l'active comme n'importe quel
     * autre périphérique (voir dispatch_pending_interrupts dans
     * interrupts.c, qui appellera gpio_isr_dispatch quand cette ligne se
     * déclenche). */
    REG32(DPORT_PRO_GPIO_INTERRUPT_MAP_REG) = GPIO_IRQ_LINE;
    interrupts_register_handler(GPIO_IRQ_LINE, gpio_isr_dispatch, 0);
    interrupts_enable_line(GPIO_IRQ_LINE);
}

void gpio_set_mode(uint8_t pin, gpio_mode_t mode, gpio_pull_t pull) {
    if (pin >= GPIO_NUM_MAX || iomux_offset[pin] == 0xFFFFFFFFu) return;

    uint32_t iomux = iomux_reg(pin);

    /* Fonction = GPIO simple (pas une fonction péripherique dédiée). */
    uint32_t v = REG32(iomux);
    v = (v & ~IOMUX_MCU_SEL_M) | (PIN_FUNC_GPIO << IOMUX_MCU_SEL_S);
    REG32(iomux) = v;

    /* Pull-up / pull-down (mutuellement exclusifs). */
    v = REG32(iomux);
    v &= ~(IOMUX_FUN_PU | IOMUX_FUN_PD);
    if (pull == GPIO_PULL_UP)   v |= IOMUX_FUN_PU;
    if (pull == GPIO_PULL_DOWN) v |= IOMUX_FUN_PD;
    REG32(iomux) = v;

    /* Entrée : active FUN_IE. Sortie : active le driver de sortie (si la
     * broche en a un -- 34-39 n'en ont pas, silencieusement ignoré). */
    if (mode == GPIO_MODE_INPUT || mode == GPIO_MODE_INPUT_OUTPUT) {
        REG32(iomux) |= IOMUX_FUN_IE;
    } else {
        REG32(iomux) &= ~IOMUX_FUN_IE;
    }

    if (mode == GPIO_MODE_OUTPUT || mode == GPIO_MODE_INPUT_OUTPUT) {
        if (gpio_has_output_driver(pin)) {
            if (pin < 32) REG32(GPIO_ENABLE_W1TS_REG)  = (1u << pin);
            else          REG32(GPIO_ENABLE1_W1TS_REG) = (1u << (pin - 32));
        }
        if (mode == GPIO_MODE_INPUT_OUTPUT) {
            REG32(iomux) |= IOMUX_FUN_IE;
        }
    } else {
        if (pin < 32) REG32(GPIO_ENABLE_W1TC_REG)  = (1u << pin);
        else          REG32(GPIO_ENABLE1_W1TC_REG) = (1u << (pin - 32));
    }
}

void gpio_write(uint8_t pin, int level) {
    if (pin >= GPIO_NUM_MAX || !gpio_has_output_driver(pin)) return;

    if (pin < 32) {
        REG32(level ? GPIO_OUT_W1TS_REG : GPIO_OUT_W1TC_REG) = (1u << pin);
    } else {
        REG32(level ? GPIO_OUT1_W1TS_REG : GPIO_OUT1_W1TC_REG) = (1u << (pin - 32));
    }
}

int gpio_read(uint8_t pin) {
    if (pin >= GPIO_NUM_MAX) return 0;

    if (pin < 32) {
        return (REG32(GPIO_IN_REG) >> pin) & 1u;
    } else {
        return (REG32(GPIO_IN1_REG) >> (pin - 32)) & 1u;
    }
}

void gpio_set_interrupt(uint8_t pin, gpio_intr_type_t type, gpio_isr_t handler, void *arg) {
    if (pin >= GPIO_NUM_MAX || iomux_offset[pin] == 0xFFFFFFFFu) return;

    isr_table[pin] = handler;
    isr_arg[pin] = arg;

    uint32_t reg = GPIO_PINn_REG(pin);
    uint32_t v = REG32(reg);
    v &= ~(GPIO_PIN_INT_TYPE_M | GPIO_PIN_INT_ENA_M);
    v |= ((uint32_t)type << GPIO_PIN_INT_TYPE_S) & GPIO_PIN_INT_TYPE_M;
    if (type != GPIO_INTR_DISABLE) {
        v |= GPIO_PIN_INT_ENA_PRO_APP;
    }
    REG32(reg) = v;
}

void gpio_clear_interrupt(uint8_t pin) {
    if (pin >= GPIO_NUM_MAX || iomux_offset[pin] == 0xFFFFFFFFu) return;

    uint32_t reg = GPIO_PINn_REG(pin);
    REG32(reg) &= ~(GPIO_PIN_INT_TYPE_M | GPIO_PIN_INT_ENA_M);
}
