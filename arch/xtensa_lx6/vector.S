/******************************************************************************************
  Filename    : vector.S
  Core        : Xtensa LX6
  MCU         : ESP32
  Description : Interrupt vector tables and context switching for ESP32 Bare-Metal
******************************************************************************************/

/* =======================================================================================
   MACROS DE SAUVEGARDE ET RESTAURATION DU CONTEXTE CPU (Compatible Call0 ABI)
   ======================================================================================= */

.macro SaveCpuContext level
    /* Allocation de 96 octets sur la pile (alignement 16 octets) */
    addi    sp, sp, -96

    /* Sauvegarde des registres généraux a0 et a2..a15 */
    s32i.n  a0,  sp, 0*4
    s32i.n  a2,  sp, 2*4
    s32i.n  a3,  sp, 3*4
    s32i.n  a4,  sp, 4*4
    s32i.n  a5,  sp, 5*4
    s32i.n  a6,  sp, 6*4
    s32i.n  a7,  sp, 7*4
    s32i.n  a8,  sp, 8*4
    s32i.n  a9,  sp, 9*4
    s32i.n  a10, sp, 10*4
    s32i.n  a11, sp, 11*4
    s32i.n  a12, sp, 12*4
    s32i.n  a13, sp, 13*4
    s32i.n  a14, sp, 14*4
    s32i.n  a15, sp, 15*4

    /* Sauvegarde du pointeur de pile initial (sp_original = sp + 96) */
    addi    a2, sp, 96
    s32i.n  a2, sp, 1*4

    /* Sauvegarde EPCx et registre d'état */
    .if \level == 1
        rsr     a2, epc1
        s32i    a2, sp, 16*4
        rsr     a2, ps
        s32i    a2, sp, 17*4
    .else
        rsr     a2, epc\level
        s32i    a2, sp, 16*4
        rsr     a2, eps\level
        s32i    a2, sp, 17*4
    .endif

    rsr     a2, sar
    s32i    a2, sp, 18*4

    rsr     a2, exccause
    s32i    a2, sp, 19*4
.endm

.macro RestoreCpuContext level
    /* 1. Restauration de EPCx */
    l32i    a2, sp, 16*4
    .if \level == 1
        wsr     a2, epc1
    .else
        wsr     a2, epc\level
    .endif

    /* 2. Restauration de SAR */
    l32i    a2, sp, 18*4
    wsr     a2, sar
    rsync

    /* 3. Restauration du registre d'état */
    l32i    a2, sp, 17*4
    .if \level == 1
        /* Maintenir EXCM=1 (bit 0x10) pendant le dépilement */
        movi    a3, 0x10
        or      a2, a2, a3
        wsr     a2, ps
    .else
        wsr     a2, eps\level
    .endif
    rsync

    /* 4. Restauration des registres généraux */
    l32i.n  a0,  sp, 0*4
    l32i.n  a2,  sp, 2*4
    l32i.n  a3,  sp, 3*4
    l32i.n  a4,  sp, 4*4
    l32i.n  a5,  sp, 5*4
    l32i.n  a6,  sp, 6*4
    l32i.n  a7,  sp, 7*4
    l32i.n  a8,  sp, 8*4
    l32i.n  a9,  sp, 9*4
    l32i.n  a10, sp, 10*4
    l32i.n  a11, sp, 11*4
    l32i.n  a12, sp, 12*4
    l32i.n  a13, sp, 13*4
    l32i.n  a14, sp, 14*4
    l32i.n  a15, sp, 15*4

    /* 5. Restauration du pointeur de pile */
    l32i.n  sp,  sp, 1*4
.endm

.macro call_isr isr_name, level
    SaveCpuContext \level
    mov     a2, sp                  /* Passe SP en 1er argument (a2) */

    /* Long call sécurisé pour dépasser la limite des 128Ko de call0 */
    movi    a0, \isr_name
    callx0  a0                      /* Appel du handler C */

    /* Filtre de sécurité : Vérifie si a2 pointe vers une adresse DRAM valide (0x3FF.....) */
    extui   a4, a2, 20, 12
    movi    a3, 0x3FF
    bne     a4, a3, .L_skip_sp_\@   /* Si non valide, conserve l'ancien SP */
    mov     sp, a2                  /* Mise à jour du SP pour le changement de contexte */
.L_skip_sp_\@:
    RestoreCpuContext \level
.endm

/* =======================================================================================
   TABLE DES VECTEURS D'INTERRUPTION ESP32 LX6 (IRAM, Alignée sur 1024 octets)
   ======================================================================================= */

.section .iram0.vectors, "ax"
.global _vector_base
.type _vector_base, @function
.balign 1024

_vector_base:

    .org 0x000
    WindowVectors:
        rfwo

    .org 0x180
    Level2InterruptVector:
        j Level2InterruptVectorHandler

    .org 0x1C0
    Level3InterruptVector:
        j Level3InterruptVectorHandler

    .org 0x200
    Level4InterruptVector:
        j Level4InterruptVectorHandler

    .org 0x240
    Level5InterruptVector:
        j Level5InterruptVectorHandler

    .org 0x280
    DebugExceptionVector:
        1: j 1b

    .org 0x2C0
    NMIExceptionVector:
        1: j 1b

    .org 0x300
    Level1KernelInterruptVector:
        j Level1KernelInterruptVectorHandler

    .org 0x340
    Level1UserInterruptVector:
        j Level1UserInterruptVectorHandler

    .org 0x3C0
    DoubleExceptionVector:
        1: j 1b

    /* Obligatoire sur ESP32 LX6 en mode bare-metal (PS.UM = 0) */
    .org 0x400
    KernelExceptionVector:
        j KernelExceptionVectorHandler

.size _vector_base, .-_vector_base

/* =======================================================================================
   GESTIONNAIRES DE VECTEURS (HANDLERS)
   ======================================================================================= */

.section .iram0.text, "ax"
.global _vector_handlers
.type _vector_handlers, @function
.align 4

.extern c_interrupt_handler

_vector_handlers:

    Level2InterruptVectorHandler:
        call_isr c_interrupt_handler, 2
        rfi 2

    Level3InterruptVectorHandler:
        call_isr c_interrupt_handler, 3
        rfi 3

    Level4InterruptVectorHandler:
        call_isr c_interrupt_handler, 4
        rfi 4

    Level5InterruptVectorHandler:
        call_isr c_interrupt_handler, 5
        rfi 5

    Level1KernelInterruptVectorHandler:
        call_isr c_interrupt_handler, 1
        rfe

    Level1UserInterruptVectorHandler:
        call_isr c_interrupt_handler, 1
        rfe

    KernelExceptionVectorHandler:
        call_isr c_interrupt_handler, 1
        rfe

.size _vector_handlers, .-_vector_handlers

/* =======================================================================================
   FONCTIONS UTILITAIRES LOW-LEVEL
   ======================================================================================= */

.section .iram0.text, "ax"
.align 4
.global set_cpu_private_timer
.type set_cpu_private_timer, @function
set_cpu_private_timer:
    beqi    a2, 0, .L_timer0
    beqi    a2, 1, .L_timer1
    beqi    a2, 2, .L_timer2
    ret
.L_timer0:
    rsr     a2, ccount
    add     a2, a2, a3
    wsr     a2, ccompare0
    rsync
    ret
.L_timer1:
    rsr     a2, ccount
    add     a2, a2, a3
    wsr     a2, ccompare1
    rsync
    ret
.L_timer2:
    rsr     a2, ccount
    add     a2, a2, a3
    wsr     a2, ccompare2
    rsync
    ret
.size set_cpu_private_timer, .-set_cpu_private_timer
