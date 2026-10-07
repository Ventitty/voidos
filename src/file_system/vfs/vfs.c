#include "src/file_system/vfs/vfs.h"

static int path_in(const char *path, const char *dir) {
    size_t n = strlen(dir);
    return strncmp(path, dir, n) == 0 && (path[n] == '\0' || path[n] == '/');
}

static int is_valid(const char *path) {
    return path != NULL && path[0] == '/';
}

static int is_root(const char *path) {
    return path[0] == '/' && path[1] == '\0';
}

static int is_ram_path(const char *path) {
    return path_in(path, "/dev") || path_in(path, "/tmp");
}

static int is_mount_point(const char *path) {
    return strcmp(path, "/dev") == 0 || strcmp(path, "/tmp") == 0;
}

int vfs_init(void) {
    ram_fs_init();
    if (ram_fs_mkdir("/dev") != 0) return -1;
    if (ram_fs_mkdir("/tmp") != 0) return -1;
    return 0;
}

int vfs_mount_sd(uint8_t sclk, uint8_t mosi, uint8_t miso, uint8_t cs) {
    if (sd_init(sclk, mosi, miso, cs) != 0) return VFS_SD_NO_CARD;
    if (fat32_mount() != 0) return VFS_SD_BAD_FS;
    return VFS_SD_OK;
}

int vfs_sd_mounted(void) {
    return fat32_is_mounted();
}

vfs_file_t *vfs_open(const char *path, int flags) {
    if (!is_valid(path) || is_root(path)) return NULL;

    void *handle;
    vfs_backend_t backend;

    if (is_ram_path(path)) {
        handle = ram_fs_open(path, flags);
        backend = VFS_BACKEND_RAM;
    } else {
        /* FAT32 en lecture seule. */
        if (flags & (VFS_O_WRITE | VFS_O_CREATE | VFS_O_TRUNC | VFS_O_APPEND)) return NULL;
        handle = fat32_open(path);
        backend = VFS_BACKEND_FAT32;
    }
    if (handle == NULL) return NULL;

    vfs_file_t *f = (vfs_file_t *)nmap(sizeof(vfs_file_t));
    if (f == NULL) {
        if (backend == VFS_BACKEND_RAM) ram_fs_close((ram_fs_file_t *)handle);
        else                            fat32_close((fat32_file_t *)handle);
        return NULL;
    }

    f->backend = backend;
    f->handle = handle;
    return f;
}

void vfs_close(vfs_file_t *file) {
    if (file == NULL) return;
    if (file->backend == VFS_BACKEND_RAM) ram_fs_close((ram_fs_file_t *)file->handle);
    else                                  fat32_close((fat32_file_t *)file->handle);
    unmap(file);
}

int vfs_read(vfs_file_t *file, void *buf, uint32_t len) {
    if (file == NULL || buf == NULL) return -1;
    if (file->backend == VFS_BACKEND_RAM) return ram_fs_read((ram_fs_file_t *)file->handle, buf, len);
    return fat32_read((fat32_file_t *)file->handle, buf, len);
}

int vfs_write(vfs_file_t *file, const void *buf, uint32_t len) {
    if (file == NULL || buf == NULL) return -1;
    if (file->backend == VFS_BACKEND_RAM) return ram_fs_write((ram_fs_file_t *)file->handle, buf, len);
    return -1;
}

int vfs_seek(vfs_file_t *file, int32_t offset, int whence) {
    if (file == NULL) return -1;
    if (file->backend == VFS_BACKEND_RAM) return ram_fs_seek((ram_fs_file_t *)file->handle, offset, whence);
    return fat32_seek((fat32_file_t *)file->handle, offset, whence);
}

uint32_t vfs_size(vfs_file_t *file) {
    if (file == NULL) return 0;
    if (file->backend == VFS_BACKEND_RAM) return ram_fs_size((ram_fs_file_t *)file->handle);
    return fat32_size((fat32_file_t *)file->handle);
}

int vfs_stat(const char *path, uint32_t *out_size, int *out_is_dir) {
    if (!is_valid(path)) return -1;

    if (is_root(path)) {
        if (out_size) *out_size = 0;
        if (out_is_dir) *out_is_dir = 1;
        return 0;
    }

    if (!is_ram_path(path)) return fat32_stat(path, out_size, out_is_dir);

    if (!ram_fs_exists(path)) return -1;
    int dir = ram_fs_is_dir(path);
    uint32_t size = 0;
    if (!dir) {
        ram_fs_file_t *f = ram_fs_open(path, RAM_FS_O_READ);
        if (f) { size = ram_fs_size(f); ram_fs_close(f); }
    }
    if (out_size) *out_size = size;
    if (out_is_dir) *out_is_dir = dir;
    return 0;
}

int vfs_exists(const char *path) {
    return vfs_stat(path, NULL, NULL) == 0;
}

int vfs_is_dir(const char *path) {
    int dir = 0;
    return vfs_stat(path, NULL, &dir) == 0 && dir;
}

int vfs_mkdir(const char *path) {
    if (!is_valid(path) || !is_ram_path(path)) return -1;
    return ram_fs_mkdir(path);
}

int vfs_mkdir_p(const char *path) {
    if (!is_valid(path) || !is_ram_path(path)) return -1;
    return ram_fs_mkdir_p(path);
}

int vfs_rmdir(const char *path) {
    if (!is_valid(path) || !is_ram_path(path) || is_mount_point(path)) return -1;
    return ram_fs_rmdir(path);
}

int vfs_unlink(const char *path) {
    if (!is_valid(path) || !is_ram_path(path) || is_mount_point(path)) return -1;
    return ram_fs_unlink(path);
}

int vfs_ls(const char *path) {
    if (!is_valid(path)) return -1;

    if (is_root(path)) {
        uart_print("D  dev/\n");
        uart_print("D  tmp/\n");
        if (!fat32_is_mounted()) {
            uart_print("   (carte SD non montee)\n");
            return 0;
        }
        return fat32_ls("/");
    }

    if (is_ram_path(path)) return ram_fs_ls(path);
    return fat32_ls(path);
}
