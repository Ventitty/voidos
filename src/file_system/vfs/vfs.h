#ifndef VFS_H
#define VFS_H

#include "src/types.h"
#include "src/file_system/ramfs/ramfs.h"
#include "src/file_system/fat32/fat32.h"
#include "arch/xtensa_lx6/drivers/esp32_sdcard/sdcard.h"
#include "src/memory_manager/memory.h"
#include "src/utils/utils.h"

#define VFS_O_READ   0x01
#define VFS_O_WRITE  0x02
#define VFS_O_CREATE 0x04
#define VFS_O_TRUNC  0x08
#define VFS_O_APPEND 0x10

#define VFS_SEEK_SET 0
#define VFS_SEEK_CUR 1
#define VFS_SEEK_END 2

#define VFS_SD_OK      0
#define VFS_SD_NO_CARD (-1)
#define VFS_SD_BAD_FS  (-2)

typedef struct vfs_file vfs_file_t;

typedef enum {
    VFS_BACKEND_RAM   = 0,
    VFS_BACKEND_FAT32 = 1,
} vfs_backend_t;

struct vfs_file {
    vfs_backend_t backend;
    void         *handle;
};

int  vfs_init(void);

int  vfs_mount_sd(uint8_t sclk, uint8_t mosi, uint8_t miso, uint8_t cs);
int  vfs_sd_mounted(void);

vfs_file_t *vfs_open(const char *path, int flags);
void     vfs_close(vfs_file_t *file);
int      vfs_read(vfs_file_t *file, void *buf, uint32_t len);
int      vfs_write(vfs_file_t *file, const void *buf, uint32_t len);
int      vfs_seek(vfs_file_t *file, int32_t offset, int whence);
uint32_t vfs_size(vfs_file_t *file);

int vfs_stat(const char *path, uint32_t *out_size, int *out_is_dir);
int vfs_exists(const char *path);
int vfs_is_dir(const char *path);
int vfs_mkdir(const char *path);
int vfs_mkdir_p(const char *path);
int vfs_rmdir(const char *path);
int vfs_unlink(const char *path);
int vfs_ls(const char *path);

extern void uart_print(const char *str);

#endif /* VFS_H */
