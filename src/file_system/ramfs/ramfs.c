#include "src/file_system/ramfs/ramfs.h"

static ram_fs_node_t root;
static spinlock_t ram_fs_lock = SPINLOCK_INIT;

static ram_fs_node_t *find_child(ram_fs_node_t *dir, const char *name, uint32_t len) {
    for (ram_fs_node_t *c = dir->first_child; c; c = c->next_sibling) {
        if (strlen(c->name) == len && strncmp(c->name, name, len) == 0) return c;
    }
    return NULL;
}

static ram_fs_node_t *alloc_node(const char *name, uint32_t len, ram_fs_type_t type, ram_fs_node_t *parent) {
    ram_fs_node_t *n = (ram_fs_node_t *)nmap(sizeof(ram_fs_node_t));
    if (!n) return NULL;

    memset(n, 0, sizeof(ram_fs_node_t));
    strncpy(n->name, name, len < RAM_FS_MAX_NAME - 1 ? len : RAM_FS_MAX_NAME - 1);
    n->name[len < RAM_FS_MAX_NAME - 1 ? len : RAM_FS_MAX_NAME - 1] = '\0';
    n->type = type;
    n->parent = parent;

    n->next_sibling = parent->first_child;
    parent->first_child = n;

    return n;
}

static void detach_node(ram_fs_node_t *node) {
    ram_fs_node_t *parent = node->parent;
    if (!parent) return;

    if (parent->first_child == node) {
        parent->first_child = node->next_sibling;
        return;
    }
    for (ram_fs_node_t *c = parent->first_child; c; c = c->next_sibling) {
        if (c->next_sibling == node) {
            c->next_sibling = node->next_sibling;
            return;
        }
    }
}

static ram_fs_node_t *resolve_parent(const char *path, const char **out_name, uint32_t *out_len) {
    if (!path || path[0] != '/') return NULL;

    ram_fs_node_t *cur = &root;
    const char *p = path + 1;

    while (1) {
        const char *start = p;
        while (*p != '\0' && *p != '/') p++;
        uint32_t len = (uint32_t)(p - start);

        if (*p == '\0') {
            *out_name = start;
            *out_len = len;
            return cur;
        }

        if (len == 0) { p++; continue; }

            ram_fs_node_t *child = find_child(cur, start, len);
            if (!child || child->type != RAM_FS_TYPE_DIR) return NULL;
            cur = child;
        p++;
    }
}

static ram_fs_node_t *find_node(const char *path) {
    if (path && path[0] == '/' && path[1] == '\0') return &root;

    const char *name;
    uint32_t len;
    ram_fs_node_t *parent = resolve_parent(path, &name, &len);
    if (!parent || len == 0) return NULL;

    return find_child(parent, name, len);
}

static int ensure_capacity(ram_fs_node_t *n, uint32_t needed) {
    if (needed <= n->capacity) return 0;

    uint8_t *new_buf = (uint8_t *)nmap(needed);
    if (!new_buf) return -1;

    if (n->data) {
        memcpy(new_buf, n->data, n->size);
        unmap(n->data);
    }
    n->data = new_buf;
    n->capacity = needed;
    return 0;
}

void ram_fs_init(void) {
    spinlock_acquire(&ram_fs_lock);
    memset(&root, 0, sizeof(root));
    root.type = RAM_FS_TYPE_DIR;
    root.name[0] = '\0';
    spinlock_release(&ram_fs_lock);
}

ram_fs_file_t *ram_fs_open(const char *path, int flags) {
    spinlock_acquire(&ram_fs_lock);

    const char *name;
    uint32_t len;
    ram_fs_node_t *parent = resolve_parent(path, &name, &len);
    if (!parent || len == 0 || len >= RAM_FS_MAX_NAME) {
        spinlock_release(&ram_fs_lock);
        return NULL;
    }

    ram_fs_node_t *node = find_child(parent, name, len);
    if (!node) {
        if (!(flags & RAM_FS_O_CREATE)) {
            spinlock_release(&ram_fs_lock);
            return NULL;
        }
        node = alloc_node(name, len, RAM_FS_TYPE_FILE, parent);
        if (!node) {
            spinlock_release(&ram_fs_lock);
            return NULL;
        }
    } else if (node->type == RAM_FS_TYPE_DIR) {
        spinlock_release(&ram_fs_lock);
        return NULL;   /* c'est un répertoire, pas un fichier/device */
    } else if (node->type == RAM_FS_TYPE_FILE && (flags & RAM_FS_O_TRUNC)) {
        if (node->data) { unmap(node->data); node->data = NULL; }
        node->size = 0;
        node->capacity = 0;
    }

    spinlock_release(&ram_fs_lock);

    ram_fs_file_t *file = (ram_fs_file_t *)nmap(sizeof(ram_fs_file_t));
    if (!file) return NULL;

    file->node = node;
    file->pos = (flags & RAM_FS_O_APPEND) ? node->size : 0;
    return file;
}

void ram_fs_close(ram_fs_file_t *file) {
    if (file) unmap(file);
}

int ram_fs_read(ram_fs_file_t *file, void *buf, uint32_t len) {
    if (!file) return -1;
    ram_fs_node_t *n = file->node;

    if (n->type == RAM_FS_TYPE_DEVICE) {
        if (!n->dev_ops->read) return -1;
        return n->dev_ops->read(n->dev_ctx, buf, len);
    }

    spinlock_acquire(&ram_fs_lock);

    if (file->pos >= n->size) {
        spinlock_release(&ram_fs_lock);
        return 0;
    }

    uint32_t avail = n->size - file->pos;
    uint32_t k = (len < avail) ? len : avail;
    memcpy(buf, n->data + file->pos, k);
    file->pos += k;

    spinlock_release(&ram_fs_lock);
    return (int)k;
}

int ram_fs_write(ram_fs_file_t *file, const void *buf, uint32_t len) {
    if (!file) return -1;
    ram_fs_node_t *n = file->node;

    if (n->type == RAM_FS_TYPE_DEVICE) {
        if (!n->dev_ops->write) return -1;
        return n->dev_ops->write(n->dev_ctx, buf, len);
    }

    uint32_t end = file->pos + len;

    spinlock_acquire(&ram_fs_lock);

    if (ensure_capacity(n, end) != 0) {
        spinlock_release(&ram_fs_lock);
        return -1;
    }

    memcpy(n->data + file->pos, buf, len);
    if (end > n->size) n->size = end;
    file->pos = end;

    spinlock_release(&ram_fs_lock);
    return (int)len;
}

int ram_fs_seek(ram_fs_file_t *file, int32_t offset, int whence) {
    if (!file) return -1;
    ram_fs_node_t *n = file->node;

    int32_t base;
    switch (whence) {
        case RAM_FS_SEEK_SET: base = 0; break;
        case RAM_FS_SEEK_CUR: base = (int32_t)file->pos; break;
        case RAM_FS_SEEK_END: base = (int32_t)n->size; break;
        default: return -1;
    }

    int32_t new_pos = base + offset;
    if (new_pos < 0) return -1;

    file->pos = (uint32_t)new_pos;
    return (int)file->pos;
}

uint32_t ram_fs_size(ram_fs_file_t *file) {
    return file ? file->node->size : 0;
}

int ram_fs_ioctl(ram_fs_file_t *file, uint32_t request, void *arg) {
    if (!file || file->node->type != RAM_FS_TYPE_DEVICE || !file->node->dev_ops->ioctl) return -1;
    return file->node->dev_ops->ioctl(file->node->dev_ctx, request, arg);
}

int ram_fs_mknod(const char *path, const ram_fs_dev_ops_t *ops, void *ctx) {
    if (!ops) return -1;

    spinlock_acquire(&ram_fs_lock);

    const char *name;
    uint32_t len;
    ram_fs_node_t *parent = resolve_parent(path, &name, &len);
    if (!parent || len == 0 || len >= RAM_FS_MAX_NAME || find_child(parent, name, len)) {
        spinlock_release(&ram_fs_lock);
        return -1;
    }

    ram_fs_node_t *n = alloc_node(name, len, RAM_FS_TYPE_DEVICE, parent);
    if (n) {
        n->dev_ops = ops;
        n->dev_ctx = ctx;
    }

    spinlock_release(&ram_fs_lock);
    return n ? 0 : -1;
}

int ram_fs_unlink(const char *path) {
    spinlock_acquire(&ram_fs_lock);

    ram_fs_node_t *n = find_node(path);
    if (!n || n == &root || n->type == RAM_FS_TYPE_DIR) {
        spinlock_release(&ram_fs_lock);
        return -1;
    }

    detach_node(n);
    if (n->data) unmap(n->data);
    unmap(n);

    spinlock_release(&ram_fs_lock);
    return 0;
}

int ram_fs_mkdir(const char *path) {
    spinlock_acquire(&ram_fs_lock);

    const char *name;
    uint32_t len;
    ram_fs_node_t *parent = resolve_parent(path, &name, &len);
    if (!parent || len == 0 || len >= RAM_FS_MAX_NAME) {
        spinlock_release(&ram_fs_lock);
        return -1;
    }

    ram_fs_node_t *existing = find_child(parent, name, len);
    if (existing) {
        int ok = (existing->type == RAM_FS_TYPE_DIR);
        spinlock_release(&ram_fs_lock);
        return ok ? 0 : -1;
    }

    ram_fs_node_t *n = alloc_node(name, len, RAM_FS_TYPE_DIR, parent);
    spinlock_release(&ram_fs_lock);
    return n ? 0 : -1;
}

int ram_fs_mkdir_p(const char *path) {
    if (!path || path[0] != '/') return -1;

    spinlock_acquire(&ram_fs_lock);

    ram_fs_node_t *cur = &root;
    const char *p = path + 1;

    while (*p != '\0') {
        const char *start = p;
        while (*p != '\0' && *p != '/') p++;
        uint32_t len = (uint32_t)(p - start);

        if (len == 0) { if (*p == '/') p++; continue; }
        if (len >= RAM_FS_MAX_NAME) { spinlock_release(&ram_fs_lock); return -1; }

        ram_fs_node_t *child = find_child(cur, start, len);
        if (!child) {
            child = alloc_node(start, len, RAM_FS_TYPE_DIR, cur);
            if (!child) { spinlock_release(&ram_fs_lock); return -1; }
        } else if (child->type != RAM_FS_TYPE_DIR) {
            spinlock_release(&ram_fs_lock);
            return -1;
        }

        cur = child;
        if (*p == '/') p++;
    }

    spinlock_release(&ram_fs_lock);
    return 0;
}

int ram_fs_rmdir(const char *path) {
    spinlock_acquire(&ram_fs_lock);

    ram_fs_node_t *n = find_node(path);
    if (!n || n == &root || n->type != RAM_FS_TYPE_DIR) {
        spinlock_release(&ram_fs_lock);
        return -1;
    }
    if (n->first_child != NULL) {
        spinlock_release(&ram_fs_lock);
        return -1;
    }

    detach_node(n);
    unmap(n);

    spinlock_release(&ram_fs_lock);
    return 0;
}

int ram_fs_is_dir(const char *path) {
    spinlock_acquire(&ram_fs_lock);
    ram_fs_node_t *n = find_node(path);
    int ok = n && n->type == RAM_FS_TYPE_DIR;
    spinlock_release(&ram_fs_lock);
    return ok;
}

int ram_fs_exists(const char *path) {
    spinlock_acquire(&ram_fs_lock);
    int ok = find_node(path) != NULL;
    spinlock_release(&ram_fs_lock);
    return ok;
}

int ram_fs_ls(const char *path) {
    spinlock_acquire(&ram_fs_lock);

    ram_fs_node_t *dir = find_node(path);
    if (!dir || dir->type != RAM_FS_TYPE_DIR) {
        spinlock_release(&ram_fs_lock);
        return -1;
    }

    for (ram_fs_node_t *c = dir->first_child; c; c = c->next_sibling) {
        if (c->type == RAM_FS_TYPE_DIR) {
            uart_print("D  ");
            uart_print(c->name);
            uart_print("/\n");
        } else if (c->type == RAM_FS_TYPE_DEVICE) {
            uart_print("C  ");
            uart_print(c->name);
            uart_print("\n");
        } else {
            uart_print("F  ");
            uart_print(c->name);
            uart_print("  (");
            uart_print_hex(c->size);
            uart_print(" octets)\n");
        }
    }

    spinlock_release(&ram_fs_lock);
    return 0;
}

static void print_node_recursive(ram_fs_node_t *node, int depth) {
    for (int i = 0; i < depth; i++) uart_print("  ");

    if (node == &root) {
        uart_print("/\n");
    } else if (node->type == RAM_FS_TYPE_DIR) {
        uart_print(node->name);
        uart_print("/\n");
    } else if (node->type == RAM_FS_TYPE_DEVICE) {
        uart_print(node->name);
        uart_print("  [device]\n");
    } else {
        uart_print(node->name);
        uart_print("  (");
        uart_print_hex(node->size);
        uart_print(" octets)\n");
    }

    if (node->type == RAM_FS_TYPE_DIR) {
        for (ram_fs_node_t *c = node->first_child; c; c = c->next_sibling) {
            print_node_recursive(c, depth + 1);
        }
    }
}

void ram_fs_print_tree(void) {
    spinlock_acquire(&ram_fs_lock);
    print_node_recursive(&root, 0);
    spinlock_release(&ram_fs_lock);
}
