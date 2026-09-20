#include "src/loader/elf_loader.h"

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p) { return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24)); }

void elf_loader_print_layout(void) {
    uart_print("[elf] user_iram_base="); uart_print_hex((uint32_t)&_user_iram_base);
    uart_print(" size="); uart_print_hex((uint32_t)&_user_iram_size); uart_print("\n");
    uart_print("[elf] user_dram_base="); uart_print_hex((uint32_t)&_user_dram_base);
    uart_print(" size="); uart_print_hex((uint32_t)&_user_dram_size); uart_print("\n");
}

static int load_segment(fat32_file_t *f, uint8_t *dst, uint32_t filesz, uint32_t memsz) {
    if (((uint32_t)dst & 3u) != 0) {
        uart_print("[elf] REFUS : adresse de segment non alignee sur 4\n");
        return -1;
    }

    uint32_t offset = 0;
    uint32_t file_remaining = filesz;

    while (offset < memsz) {
        uint8_t buf[256];
        memset(buf, 0, sizeof(buf));

        uint32_t chunk = memsz - offset;
        if (chunk > sizeof(buf)) chunk = sizeof(buf);

        uint32_t from_file = (file_remaining < chunk) ? file_remaining : chunk;
        if (from_file > 0) {
            int n = fat32_read(f, buf, from_file);
            if (n != (int)from_file) return -1;
            file_remaining -= from_file;
        }

        uint32_t words = (chunk + 3u) / 4u;
        uint32_t *dst32 = (uint32_t *)(dst + offset);
        const uint32_t *buf32 = (const uint32_t *)buf;
        for (uint32_t w = 0; w < words; w++) dst32[w] = buf32[w];

        offset += chunk;
    }

    return 0;
}

void (*elf_load(const char *path))(void) {
    fat32_file_t *f = fat32_open(path);
    if (!f) {
        uart_print("[elf] fichier introuvable sur la carte : "); uart_print(path); uart_print("\n");
        return NULL;
    }

    uint8_t ehdr[EHDR_SIZE];
    if (fat32_read(f, ehdr, EHDR_SIZE) != EHDR_SIZE) {
        uart_print("[elf] en-tete illisible (fichier trop court)\n");
        fat32_close(f);
        return NULL;
    }

    if (ehdr[0] != 0x7Fu || ehdr[1] != 'E' || ehdr[2] != 'L' || ehdr[3] != 'F') {
        uart_print("[elf] pas un fichier ELF\n");
        fat32_close(f);
        return NULL;
    }
    if (ehdr[EI_CLASS] != ELFCLASS32 || ehdr[EI_DATA] != ELFDATA2LSB) {
        uart_print("[elf] doit etre ELF32 little-endian\n");
        fat32_close(f);
        return NULL;
    }
    if (rd16(&ehdr[18]) != EM_XTENSA) {
        uart_print("[elf] architecture incorrecte (attendu Xtensa)\n");
        fat32_close(f);
        return NULL;
    }

    uint32_t e_entry     = rd32(&ehdr[24]);
    uint32_t e_phoff     = rd32(&ehdr[28]);
    uint16_t e_phentsize = rd16(&ehdr[42]);
    uint16_t e_phnum     = rd16(&ehdr[44]);

    if (e_phentsize != PHDR_SIZE || e_phnum == 0) {
        uart_print("[elf] table des program headers invalide\n");
        fat32_close(f);
        return NULL;
    }

    uint32_t iram_base = (uint32_t)&_user_iram_base, iram_size = (uint32_t)&_user_iram_size;
    uint32_t dram_base = (uint32_t)&_user_dram_base, dram_size = (uint32_t)&_user_dram_size;

    int segments_loaded = 0;

    for (uint16_t i = 0; i < e_phnum; i++) {
        if (fat32_seek(f, (int32_t)(e_phoff + (uint32_t)i * e_phentsize), FAT32_SEEK_SET) < 0) {
            uart_print("[elf] seek program header echoue\n");
            fat32_close(f);
            return NULL;
        }

        uint8_t phdr[PHDR_SIZE];
        if (fat32_read(f, phdr, PHDR_SIZE) != PHDR_SIZE) {
            uart_print("[elf] program header illisible\n");
            fat32_close(f);
            return NULL;
        }

        if (rd32(&phdr[0]) != PT_LOAD) continue;

        uint32_t p_offset = rd32(&phdr[4]);
        uint32_t p_vaddr  = rd32(&phdr[8]);
        uint32_t p_filesz = rd32(&phdr[16]);
        uint32_t p_memsz  = rd32(&phdr[20]);

        int in_iram = (p_vaddr >= iram_base) && (p_vaddr + p_memsz <= iram_base + iram_size);
        int in_dram = (p_vaddr >= dram_base) && (p_vaddr + p_memsz <= dram_base + dram_size);
        if (!in_iram && !in_dram) {
            uart_print("[elf] REFUS : segment hors zone reservee, vaddr=");
            uart_print_hex(p_vaddr);
            uart_print(" memsz="); uart_print_hex(p_memsz);
            uart_print(" (compile avec user_program.ld ?)\n");
            fat32_close(f);
            return NULL;
        }
        if (p_filesz > p_memsz) {
            uart_print("[elf] REFUS : p_filesz > p_memsz (ELF incoherent)\n");
            fat32_close(f);
            return NULL;
        }

        if (fat32_seek(f, (int32_t)p_offset, FAT32_SEEK_SET) < 0) {
            uart_print("[elf] seek donnees segment echoue\n");
            fat32_close(f);
            return NULL;
        }

        if (load_segment(f, (uint8_t *)p_vaddr, p_filesz, p_memsz) != 0) {
            uart_print("[elf] chargement du segment echoue\n");
            fat32_close(f);
            return NULL;
        }

        segments_loaded++;
    }

    fat32_close(f);

    if (segments_loaded == 0) {
        uart_print("[elf] aucun segment PT_LOAD\n");
        return NULL;
    }

    int entry_ok = (e_entry >= iram_base && e_entry < iram_base + iram_size) ||
    (e_entry >= dram_base && e_entry < dram_base + dram_size);
    if (!entry_ok) {
        uart_print("[elf] REFUS : point d'entree hors zone reservee, e_entry=");
        uart_print_hex(e_entry); uart_print("\n");
        return NULL;
    }

    __asm__ volatile ("memw\n\tisync" ::: "memory");

    uart_print("[elf] charge, entree="); uart_print_hex(e_entry); uart_print("\n");
    return (void (*)(void))e_entry;
}

int elf_exec(const char *path) {
    void (*entry)(void) = elf_load(path);
    if (!entry) return -1;

    int tid = task_create(entry);
    if (tid < 0) {
        uart_print("[elf] task_create a echoue (table de taches pleine ?)\n");
        return -1;
    }

    uart_print("[elf] lance comme tache id="); uart_print_hex((uint32_t)tid); uart_print("\n");
    return tid;
}
