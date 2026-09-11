#ifndef FAT32_H
#define FAT32_H

#include "src/types.h"
#include "src/drivers/esp32_sdcard/sdcard.h"
#include "src/memory_manager/memory.h"
#include "src/utils/utils.h"
#include "src/scheduler/spinlock.h"

#define FAT32_EOC_MIN      0x0FFFFFF8u
#define FAT32_BAD_CLUSTER  0x0FFFFFF7u
#define FAT32_CLUSTER_MASK 0x0FFFFFFFu

#define DIRENT_SIZE     32u
#define DIRENT_FREE     0x00u
#define DIRENT_DELETED  0xE5u
#define ATTR_VOLUME_ID  0x08u
#define ATTR_DIRECTORY  0x10u
#define ATTR_LFN        0x0Fu

#define FAT32_SEEK_SET 0
#define FAT32_SEEK_CUR 1
#define FAT32_SEEK_END 2

struct volume {
    int mounted;
    uint32_t volume_start_lba;
    uint32_t bytes_per_sector;
    uint32_t sectors_per_cluster;
    uint32_t reserved_sectors;
    uint32_t num_fats;
    uint32_t fat_size_sectors;
    uint32_t root_cluster;
    uint32_t fat_start_lba;
    uint32_t data_start_lba;
    uint32_t total_clusters;
};

struct fat32_file {
    uint32_t first_cluster;
    uint32_t size;
    uint32_t pos;

    uint32_t cached_cluster_index;
    uint32_t cached_cluster;
};

typedef struct fat32_file fat32_file_t;

extern void uart_print(const char *str);
extern void uart_print_hex(uint32_t val);

int fat32_mount(void);
fat32_file_t *fat32_open(const char *path);
void fat32_close(fat32_file_t *file);
int  fat32_read(fat32_file_t *file, void *buf, uint32_t len);
int  fat32_seek(fat32_file_t *file, int32_t offset, int whence);
uint32_t fat32_size(fat32_file_t *file);
int fat32_ls(const char *path);
void fat32_print_info(void);

#endif /* FAT32_H */
