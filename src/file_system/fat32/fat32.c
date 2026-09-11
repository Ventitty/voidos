#include "src/file_system/fat32/fat32.h"

static spinlock_t fat32_lock = SPINLOCK_INIT;
static struct volume vol;

static uint16_t rd16(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

int fat32_mount(void) {
    uint8_t sector[512];

    if (sd_read_block(0, sector) != 0) return -1;
    if (sector[510] != 0x55 || sector[511] != 0xAA) return -1;

    uint32_t volume_start = 0;
    uint8_t part_type = sector[0x1BE + 4];
    if (part_type == 0x0B || part_type == 0x0C) {
        volume_start = rd32(&sector[0x1BE + 8]);
    }

    if (sd_read_block(volume_start, sector) != 0) return -1;

    uint32_t bytes_per_sector = rd16(&sector[0x0B]);
    if (bytes_per_sector != 512) return -1;

    uint32_t fat_size_32 = rd32(&sector[0x24]);
    if (fat_size_32 == 0) return -1;

    vol.volume_start_lba = volume_start;
    vol.bytes_per_sector = bytes_per_sector;
    vol.sectors_per_cluster = sector[0x0D];
    vol.reserved_sectors = rd16(&sector[0x0E]);
    vol.num_fats = sector[0x10];
    vol.fat_size_sectors = fat_size_32;
    vol.root_cluster = rd32(&sector[0x2C]);

    if (vol.sectors_per_cluster == 0 || vol.num_fats == 0) return -1;

    vol.fat_start_lba = vol.volume_start_lba + vol.reserved_sectors;
    vol.data_start_lba = vol.fat_start_lba + vol.num_fats * vol.fat_size_sectors;

    uint32_t total_sectors = rd32(&sector[0x20]);
    if (total_sectors == 0) total_sectors = rd16(&sector[0x13]);
    uint32_t data_sectors = total_sectors - (vol.reserved_sectors + vol.num_fats * vol.fat_size_sectors);
    vol.total_clusters = data_sectors / vol.sectors_per_cluster;

    vol.mounted = 1;
    return 0;
}

void fat32_print_info(void) {
    if (!vol.mounted) { uart_print("[fat32] aucun volume monte\n"); return; }

    uart_print("[fat32] volume_start_lba="); uart_print_hex(vol.volume_start_lba); uart_print("\n");
    uart_print("[fat32] sectors_per_cluster="); uart_print_hex(vol.sectors_per_cluster); uart_print("\n");
    uart_print("[fat32] reserved_sectors="); uart_print_hex(vol.reserved_sectors); uart_print("\n");
    uart_print("[fat32] num_fats="); uart_print_hex(vol.num_fats); uart_print("\n");
    uart_print("[fat32] fat_size_sectors="); uart_print_hex(vol.fat_size_sectors); uart_print("\n");
    uart_print("[fat32] root_cluster="); uart_print_hex(vol.root_cluster); uart_print("\n");
    uart_print("[fat32] data_start_lba="); uart_print_hex(vol.data_start_lba); uart_print("\n");
    uart_print("[fat32] total_clusters="); uart_print_hex(vol.total_clusters); uart_print("\n");
}

static uint32_t cluster_to_lba(uint32_t cluster) {
    return vol.data_start_lba + (cluster - 2u) * vol.sectors_per_cluster;
}

static uint32_t fat_entry_get(uint32_t cluster) {
    uint32_t fat_offset = cluster * 4u;
    uint32_t fat_sector = vol.fat_start_lba + (fat_offset / 512u);
    uint32_t entry_offset = fat_offset % 512u;

    uint8_t sector[512];
    if (sd_read_block(fat_sector, sector) != 0) return FAT32_BAD_CLUSTER;

    return rd32(&sector[entry_offset]) & FAT32_CLUSTER_MASK;
}

static uint32_t chain_walk(uint32_t first_cluster, uint32_t index) {
    if (first_cluster < 2) return 0;

    uint32_t c = first_cluster;
    for (uint32_t i = 0; i < index; i++) {
        c = fat_entry_get(c);
        if (c < 2 || c >= FAT32_EOC_MIN) return 0;
    }
    return c;
}

static void to_83(const char *name, uint32_t len, char out[11]) {
    for (int i = 0; i < 11; i++) out[i] = ' ';

    uint32_t dot = len;
    for (uint32_t i = 0; i < len; i++) {
        if (name[i] == '.') { dot = i; break; }
    }

    uint32_t name_len = dot;
    if (name_len > 8) name_len = 8;
    for (uint32_t i = 0; i < name_len; i++) out[i] = (char)to_upper((unsigned char)name[i]);

    if (dot < len) {
        uint32_t ext_start = dot + 1;
        uint32_t ext_len = len - ext_start;
        if (ext_len > 3) ext_len = 3;
        for (uint32_t i = 0; i < ext_len; i++) out[8 + i] = (char)to_upper((unsigned char)name[ext_start + i]);
    }
}

static int name83_matches(const uint8_t *dirent, const char *component, uint32_t len) {
    char want[11];
    to_83(component, len, want);
    return memcmp(dirent, want, 11) == 0;
}

static int dir_find(uint32_t dir_cluster, const char *name, uint32_t name_len, uint32_t *out_cluster, uint32_t *out_size, uint8_t *out_attr) {
    if (dir_cluster == 0) dir_cluster = vol.root_cluster;

    uint32_t c = dir_cluster;
    while (c >= 2 && c < FAT32_EOC_MIN) {
        uint32_t base_lba = cluster_to_lba(c);
        for (uint32_t s = 0; s < vol.sectors_per_cluster; s++) {
            uint8_t sector[512];
            if (sd_read_block(base_lba + s, sector) != 0) return -1;

            for (uint32_t off = 0; off < 512; off += DIRENT_SIZE) {
                uint8_t first = sector[off];
                if (first == DIRENT_FREE) return 0;
                if (first == DIRENT_DELETED) continue;

                uint8_t attr = sector[off + 11];
                if (attr == ATTR_LFN) continue;
                if (attr & ATTR_VOLUME_ID) continue;

                if (name83_matches(&sector[off], name, name_len)) {
                    uint32_t clus_hi = rd16(&sector[off + 20]);
                    uint32_t clus_lo = rd16(&sector[off + 26]);
                    if (out_cluster) *out_cluster = (clus_hi << 16) | clus_lo;
                    if (out_size) *out_size = rd32(&sector[off + 28]);
                    if (out_attr) *out_attr = attr;
                    return 1;
                }
            }
        }
        c = fat_entry_get(c);
    }
    return 0;
}

static int resolve_parent(const char *path, uint32_t *parent_cluster, const char **name, uint32_t *name_len) {
    if (!path || path[0] != '/') return -1;

    uint32_t cur = vol.root_cluster;
    const char *p = path + 1;

    while (1) {
        const char *start = p;
        while (*p != '\0' && *p != '/') p++;
        uint32_t len = (uint32_t)(p - start);

        if (*p == '\0') {
            *parent_cluster = cur;
            *name = start;
            *name_len = len;
            return 0;
        }

        if (len == 0) { p++; continue; }

        uint32_t child_cluster, child_size;
        uint8_t child_attr;
        int found = dir_find(cur, start, len, &child_cluster, &child_size, &child_attr);
        if (found <= 0 || !(child_attr & ATTR_DIRECTORY)) return -1;

        cur = child_cluster;
        p++;
    }
}

fat32_file_t *fat32_open(const char *path) {
    if (!vol.mounted) return NULL;

    spinlock_acquire(&fat32_lock);

    uint32_t parent_cluster;
    const char *name;
    uint32_t name_len;
    if (resolve_parent(path, &parent_cluster, &name, &name_len) != 0 || name_len == 0) {
        spinlock_release(&fat32_lock);
        return NULL;
    }

    uint32_t cluster = 0, size = 0;
    uint8_t attr = 0;
    int found = dir_find(parent_cluster, name, name_len, &cluster, &size, &attr);

    spinlock_release(&fat32_lock);

    if (found <= 0 || (attr & ATTR_DIRECTORY)) return NULL;

    fat32_file_t *f = (fat32_file_t *)nmap(sizeof(fat32_file_t));
    if (!f) return NULL;

    f->first_cluster = cluster;
    f->size = size;
    f->pos = 0;
    f->cached_cluster_index = 0;
    f->cached_cluster = cluster;

    return f;
}

void fat32_close(fat32_file_t *file) {
    if (file) unmap(file);
}

uint32_t fat32_size(fat32_file_t *file) {
    return file ? file->size : 0;
}

int fat32_read(fat32_file_t *file, void *buf, uint32_t len) {
    if (!file || !vol.mounted) return -1;
    if (file->pos >= file->size) return 0;

    uint32_t avail = file->size - file->pos;
    if (len > avail) len = avail;

    uint32_t bytes_per_cluster = vol.bytes_per_sector * vol.sectors_per_cluster;
    uint8_t *out = (uint8_t *)buf;
    uint32_t done = 0;

    spinlock_acquire(&fat32_lock);

    while (done < len) {
        uint32_t abs_pos = file->pos + done;
        uint32_t cluster_index = abs_pos / bytes_per_cluster;
        uint32_t offset_in_cluster = abs_pos % bytes_per_cluster;
        uint32_t sector_in_cluster = offset_in_cluster / vol.bytes_per_sector;
        uint32_t offset_in_sector = offset_in_cluster % vol.bytes_per_sector;

        uint32_t cluster;
        if (cluster_index == file->cached_cluster_index) {
            cluster = file->cached_cluster;
        } else {
            cluster = chain_walk(file->first_cluster, cluster_index);
            if (cluster == 0) break;
            file->cached_cluster_index = cluster_index;
            file->cached_cluster = cluster;
        }

        uint8_t sector[512];
        if (sd_read_block(cluster_to_lba(cluster) + sector_in_cluster, sector) != 0) break;

        uint32_t chunk = 512 - offset_in_sector;
        if (chunk > len - done) chunk = len - done;
        memcpy(out + done, sector + offset_in_sector, chunk);
        done += chunk;
    }

    file->pos += done;
    spinlock_release(&fat32_lock);
    return (int)done;
}

int fat32_seek(fat32_file_t *file, int32_t offset, int whence) {
    if (!file) return -1;

    int32_t base;
    switch (whence) {
        case FAT32_SEEK_SET: base = 0; break;
        case FAT32_SEEK_CUR: base = (int32_t)file->pos; break;
        case FAT32_SEEK_END: base = (int32_t)file->size; break;
        default: return -1;
    }

    int32_t new_pos = base + offset;
    if (new_pos < 0) return -1;

    file->pos = (uint32_t)new_pos;
    return (int)file->pos;
}

int fat32_ls(const char *path) {
    if (!vol.mounted) return -1;

    spinlock_acquire(&fat32_lock);

    uint32_t dir_cluster = vol.root_cluster;
    if (path[0] != '\0' && !(path[0] == '/' && path[1] == '\0')) {
        uint32_t parent_cluster;
        const char *name;
        uint32_t name_len;
        if (resolve_parent(path, &parent_cluster, &name, &name_len) != 0) {
            spinlock_release(&fat32_lock);
            return -1;
        }
        uint32_t c, sz; uint8_t attr;
        int found = dir_find(parent_cluster, name, name_len, &c, &sz, &attr);
        if (found <= 0 || !(attr & ATTR_DIRECTORY)) {
            spinlock_release(&fat32_lock);
            return -1;
        }
        dir_cluster = c;
    }

    uint32_t c = (dir_cluster == 0) ? vol.root_cluster : dir_cluster;
    while (c >= 2 && c < FAT32_EOC_MIN) {
        uint32_t base_lba = cluster_to_lba(c);
        for (uint32_t s = 0; s < vol.sectors_per_cluster; s++) {
            uint8_t sector[512];
            if (sd_read_block(base_lba + s, sector) != 0) { spinlock_release(&fat32_lock); return -1; }

            for (uint32_t off = 0; off < 512; off += DIRENT_SIZE) {
                uint8_t first = sector[off];
                if (first == DIRENT_FREE) { spinlock_release(&fat32_lock); return 0; }
                if (first == DIRENT_DELETED) continue;
                uint8_t attr = sector[off + 11];
                if (attr == ATTR_LFN || (attr & ATTR_VOLUME_ID)) continue;

                char name[12];
                memcpy(name, &sector[off], 11);
                name[11] = '\0';

                for (int i = 10; i >= 0 && name[i] == ' '; i--) name[i] = '\0';

                uart_print((attr & ATTR_DIRECTORY) ? "D  " : "F  ");
                uart_print(name);
                if (!(attr & ATTR_DIRECTORY)) {
                    uart_print("  (");
                    uart_print_hex(rd32(&sector[off + 28]));
                    uart_print(" octets)");
                }
                uart_print("\n");
            }
        }
        c = fat_entry_get(c);
    }

    spinlock_release(&fat32_lock);
    return 0;
}
