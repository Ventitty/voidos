#ifndef ELF_LOADER_H
#define ELF_LOADER_H

#include "src/types.h"
#include "src/file_system/fat32/fat32.h"
#include "src/scheduler/scheduler.h"
#include "src/utils/utils.h"

#define EI_CLASS    4
#define EI_DATA     5
#define ELFCLASS32  1
#define ELFDATA2LSB 1
#define EM_XTENSA   94
#define PT_LOAD     1

#define EHDR_SIZE 52
#define PHDR_SIZE 32

extern void uart_print(const char *str);
extern void uart_print_hex(uint32_t val);
extern uint32_t _user_iram_base, _user_iram_size;
extern uint32_t _user_dram_base, _user_dram_size;

void (*elf_load(const char *path))(void);
int elf_exec(const char *path);
void elf_loader_print_layout(void);

#endif /* ELF_LOADER_H */
