#ifndef RAM_FS_H
#define RAM_FS_H

#include "src/types.h"
#include "src/memory_manager/memory.h"
#include "src/utils/utils.h"
#include "src/scheduler/spinlock.h"

#define RAM_FS_MAX_NAME 32

#define RAM_FS_O_READ   0x01
#define RAM_FS_O_WRITE  0x02
#define RAM_FS_O_CREATE 0x04
#define RAM_FS_O_TRUNC  0x08
#define RAM_FS_O_APPEND 0x10

#define RAM_FS_SEEK_SET 0
#define RAM_FS_SEEK_CUR 1
#define RAM_FS_SEEK_END 2

typedef enum {
    RAM_FS_TYPE_FILE   = 0,
    RAM_FS_TYPE_DIR    = 1,
    RAM_FS_TYPE_DEVICE = 2,
} ram_fs_type_t;

typedef struct {
    int (*read)(void *ctx, void *buf, uint32_t len);
    int (*write)(void *ctx, const void *buf, uint32_t len);
    int (*ioctl)(void *ctx, uint32_t request, void *arg);
} ram_fs_dev_ops_t;

typedef struct ram_fs_node {
    char name[RAM_FS_MAX_NAME];
    ram_fs_type_t type;
    struct ram_fs_node *parent;
    struct ram_fs_node *next_sibling;

    struct ram_fs_node *first_child;

    uint8_t *data;
    uint32_t size;
    uint32_t capacity;

    const ram_fs_dev_ops_t *dev_ops;
    void *dev_ctx;
} ram_fs_node_t;

struct ram_fs_file {
    ram_fs_node_t *node;
    uint32_t pos;
};

typedef struct ram_fs_file ram_fs_file_t;

extern void uart_print(const char *str);
extern void uart_print_hex(uint32_t val);

void ram_fs_init(void);
ram_fs_file_t *ram_fs_open(const char *path, int flags);

void ram_fs_close(ram_fs_file_t *file);
int  ram_fs_read(ram_fs_file_t *file, void *buf, uint32_t len);
int  ram_fs_write(ram_fs_file_t *file, const void *buf, uint32_t len);
int  ram_fs_seek(ram_fs_file_t *file, int32_t offset, int whence);
uint32_t ram_fs_size(ram_fs_file_t *file);

int ram_fs_unlink(const char *path);

int ram_fs_ioctl(ram_fs_file_t *file, uint32_t request, void *arg);
int ram_fs_mknod(const char *path, const ram_fs_dev_ops_t *ops, void *ctx);
int ram_fs_mkdir(const char *path);
int ram_fs_mkdir_p(const char *path);
int ram_fs_rmdir(const char *path);

int ram_fs_is_dir(const char *path);
int ram_fs_exists(const char *path);
int ram_fs_ls(const char *path);

void ram_fs_print_tree(void);

#endif /* RAM_FS_H */
