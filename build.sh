#!/usr/bin/env bash
set -e

MODE="${1:-flash}"
PORT="${2:-}"

CHIP="esp32"
TOOLS_DIR="build/bin"
CC="${TOOLS_DIR}/bin/xtensa-${CHIP}-elf-gcc"
QEMU="${TOOLS_DIR}/bin/qemu-system-xtensa"

GCC_URL="https://github.com/espressif/crosstool-NG/releases/download/esp-16.1.0_20260609/xtensa-esp-elf-16.1.0_20260609-x86_64-linux-gnu.tar.gz"
QEMU_URL="https://github.com/espressif/qemu/releases/download/esp-develop-9.2.2-20260417/qemu-xtensa-softmmu-esp_develop_9.2.2_20260417-x86_64-linux-gnu.tar.xz"

INCLUDES="-I."
SRC_DIR="src"
OUT_DIR="build"
ARCH_DIR="arch/xtensa_lx6"

LINKER_SCRIPT="arch/xtensa_lx6/linker.ld"
KERNEL_ELF="${OUT_DIR}/kernel.elf"
KERNEL_BIN="${OUT_DIR}/kernel.bin"

mkdir -p "${TOOLS_DIR}"

if [ ! -f "${CC}" ]; then
    echo "[0/4] Toolchain GCC introuvable. Téléchargement via wget..."
    mkdir -p /tmp/xtensa-gcc
    wget -q --show-progress -O /tmp/xtensa-gcc.tar.gz "${GCC_URL}"
    echo "Extraction de la toolchain GCC dans ${TOOLS_DIR}..."
    tar -xzf /tmp/xtensa-gcc.tar.gz --strip-components=1 -C "${TOOLS_DIR}"
    rm -rf /tmp/xtensa-gcc.tar.gz
fi

if [ "$MODE" = "qemu" ] && [ ! -f "${QEMU}" ]; then
    echo "[0/4] QEMU Xtensa introuvable. Téléchargement via wget..."
    wget -q --show-progress -O /tmp/qemu-xtensa.tar.gz "${QEMU_URL}"
    echo "Extraction de QEMU dans ${TOOLS_DIR}..."
    tar -xJf /tmp/qemu-xtensa.tar.gz --strip-components=1 -C "${TOOLS_DIR}"
    rm -rf /tmp/qemu-xtensa.tar.gz
fi

echo "[1/4] Nettoyage et préparation du dossier de build"
mkdir -p ${OUT_DIR}
rm -f ${KERNEL_ELF} ${KERNEL_BIN}

echo "[2/4] Compilation du kernel"
${CC} -nostdlib -T ${LINKER_SCRIPT} -g -fno-tree-loop-distribute-patterns -mtext-section-literals -mlongcalls -fno-builtin -fno-stack-protector -fno-pic -fno-pie -Wall -Wextra -Werror -std=c99 -pedantic -ffreestanding \
    -mabi=call0 \
    ${INCLUDES} \
    -o ${KERNEL_ELF} \
    ${ARCH_DIR}/boot.S ${ARCH_DIR}/vector.S ${SRC_DIR}/kernel/kernel.c ${SRC_DIR}/memory_manager/memory.c ${SRC_DIR}/interrupts/interrupts.c ${SRC_DIR}/watchdog/watchdog.c ${SRC_DIR}/utils/utils.c \
    -lgcc

echo "Succès : ${KERNEL_ELF} généré."

if [ "$MODE" = "qemu" ]; then
    echo "[3/3] Démarrage de QEMU ESP32"
    ${QEMU} \
        -machine ${CHIP} \
        -smp 2 \
        -display none \
        -serial mon:stdio \
        -kernel ${KERNEL_ELF} \
        -d int,guest_errors -D /tmp/int-trace.log

elif [ "$MODE" = "flash" ]; then
    if [ -z "$PORT" ]; then
        if [ -e /dev/ttyUSB0 ]; then
            PORT="/dev/ttyUSB0"
        elif [ -e /dev/ttyACM0 ]; then
            PORT="/dev/ttyACM0"
        else
            echo "Erreur : aucun périphérique /dev/ttyUSB0 ou /dev/ttyACM0 trouvé."
            exit 1
        fi
    fi

    echo "[3/4] Conversion du binaire (ELF -> BIN)"
    esptool --chip ${CHIP} elf2image ${KERNEL_ELF} -o ${KERNEL_BIN}

    echo "[4/4] Flashage sur l'ESP32 (${PORT})"
    esptool --chip ${CHIP} --port "${PORT}" write_flash -fm dio 0x1000 ${KERNEL_BIN}

    echo "Flash terminé ! Ouverture du moniteur série sur ${PORT}..."
    picocom -b 115200 "${PORT}"
else
    echo "Usage : $0 [flash|qemu] [PORT]"
    exit 1
fi
