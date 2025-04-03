#include <storage/fs/vfs.h>
#include <storage/fs/fat32.h>
#include <storage/block_device.h>
#include <mm/pmm.h>
#include <string.h>
#include <printf.h>
#include <mm/pmm.h>

// Global mount table (linked list)
static mount_entry_t *mount_table = NULL;

// Helper: Create and initialize a filesystem from a block device.
static filesystem_t* create_filesystem(block_device_t *dev, const char *fs_type) {
    filesystem_t *fs = (filesystem_t*)kmalloc(sizeof(filesystem_t));
    if (!fs)
        return NULL;
    if (strcmp(fs_type, "fat32") == 0) {
    kprintf("\nCHECKPOINT 7\n");
        if (fat32_mount(dev) != 0) {
            kprintf("VFS: FAT32 mount failed\n");
            kfree(fs);
            return NULL;
        }
    kprintf("\nCHECKPOINT 8\n");
        strncpy(fs->fs_name, "fat32", sizeof(fs->fs_name));
        fs->ops.open  = fat32_open;
        fs->ops.read  = fat32_read;
        fs->ops.write = fat32_write;
        fs->ops.close = fat32_close;
        fs->fs_private = fat_vol;  // assuming fat_vol is defined in your FAT32 module
    } else {
        kprintf("VFS: Unknown filesystem type '%s'\n", fs_type);
        kfree(fs);
        return NULL;
    }
    return fs;
}

// Mount a filesystem at a given mount point.
int vfs_mount(const char *mount_point, block_device_t *dev, const char *fs_type) {
    if (!mount_point || !dev || !fs_type) {
        kprintf("VFS: NULL argument passed to vfs_mount\n");
        return -1;
    }
    
    // Debug: Log addresses of inputs.
    kprintf("vfs_mount: mount_point=%p, dev=%p, fs_type=%p\n", mount_point, dev, fs_type);

    // Check if mount_point is a valid, NUL-terminated string.
    if (strlen(mount_point) == 0) {
        kprintf("VFS: mount_point is empty\n");
        return -1;
    }
    
    // Ensure the mount point isn’t already in use.
    for (mount_entry_t *entry = mount_table; entry != NULL; entry = entry->next) {
        if (strcmp(entry->mount_point, mount_point) == 0) {
            kprintf("VFS: Mount point '%s' already in use.\n", mount_point);
            return -1;
        }
    }
    kprintf("\nCHECKPOINT 1\n");
    
    filesystem_t *fs = create_filesystem(dev, fs_type);
    kprintf("\nCHECKPOINT 2\n");
    if (!fs) {
        kprintf("VFS: create_filesystem() returned NULL\n");
        return -1;
    }
    kprintf("\nCHECKPOINT 3\n");
    
    mount_entry_t *new_entry = (mount_entry_t*)kmalloc(sizeof(mount_entry_t));
    kprintf("\nCHECKPOINT 4\n");
    if (!new_entry) {
        kprintf("VFS: kmalloc failed for new mount entry\n");
        kfree(fs);
        return -1;
    }
    kprintf("\nCHECKPOINT 5\n");
    
    // Use a safe string copy (ensure NUL termination)
    strncpy(new_entry->mount_point, mount_point, sizeof(new_entry->mount_point) - 1);
    new_entry->mount_point[sizeof(new_entry->mount_point) - 1] = '\0';
    new_entry->fs = fs;
    new_entry->next = mount_table;
    mount_table = new_entry;
    
    kprintf("VFS: Mounted filesystem '%s' at mount point '%s'\n", fs->fs_name, mount_point);
    return 0;
}

// Unmount a filesystem from a given mount point.
int vfs_unmount(const char *mount_point) {
    if (!mount_point)
        return -1;
    
    mount_entry_t *prev = NULL, *entry = mount_table;
    while (entry) {
        if (strcmp(entry->mount_point, mount_point) == 0) {
            // Remove entry from list.
            if (prev)
                prev->next = entry->next;
            else
                mount_table = entry->next;
            
            // In a complete implementation, call a proper unmount on fs.
            kfree(entry->fs);
            kfree(entry);
            kprintf("VFS: Unmounted filesystem at mount point '%s'\n", mount_point);
            return 0;
        }
        prev = entry;
        entry = entry->next;
    }
    kprintf("VFS: Mount point '%s' not found for unmounting\n", mount_point);
    return -1;
}

// Helper: Given a full path, find the best matching mount entry.
// It does a longest prefix match so that more specific mount points take precedence.
static mount_entry_t* find_mount_entry(const char *path, const char **relative_path_out) {
    mount_entry_t *result = NULL;
    size_t best_match_len = 0;
    for (mount_entry_t *entry = mount_table; entry != NULL; entry = entry->next) {
        size_t len = strlen(entry->mount_point);
        if (strncmp(path, entry->mount_point, len) == 0) {
            // Ensure the mount point is either an exact match or is followed by '/'.
            if (path[len] == '\0' || path[len] == '/') {
                if (len > best_match_len) {
                    best_match_len = len;
                    result = entry;
                }
            }
        }
    }
    if (result) {
        const char *rel = path + best_match_len;
        if (*rel == '/')
            rel++;
        *relative_path_out = rel;
    }
    return result;
}

// Open a file given its full VFS path (e.g., "/drive0/hello.txt").
vfs_file_t* vfs_open(const char *path, const char *mode) {
    if (!path || !mode)
        return NULL;
    
    const char *relative_path = NULL;
    mount_entry_t *entry = find_mount_entry(path, &relative_path);
    if (!entry) {
        kprintf("VFS: No mount entry found for path '%s'\n", path);
        return NULL;
    }
    
    bool write = (mode[0] == 'w');
    file_t *f = entry->fs->ops.open(relative_path, write);
    if (!f) {
        kprintf("VFS: Failed to open file '%s' on mount '%s'\n", relative_path, entry->mount_point);
        return NULL;
    }
    
    vfs_file_t *vf = (vfs_file_t*)kmalloc(sizeof(vfs_file_t));
    if (!vf) {
        entry->fs->ops.close(f);
        return NULL;
    }
    vf->fs = entry->fs;
    vf->file = f;
    return vf;
}

int vfs_read(vfs_file_t *file, void *buf, uint32_t count) {
    if (!file || !file->fs || !file->file)
        return -1;
    return file->fs->ops.read(file->file, buf, count);
}

int vfs_write(vfs_file_t *file, const void *buf, uint32_t count) {
    if (!file || !file->fs || !file->file)
        return -1;
    return file->fs->ops.write(file->file, buf, count);
}

int vfs_close(vfs_file_t *file) {
    if (!file || !file->fs || !file->file)
        return -1;
    int ret = file->fs->ops.close(file->file);
    kfree(file);
    return ret;
}

uint32_t vfs_file_size(vfs_file_t *file) {
    if (!file || !file->file) {
        kprintf("VFS: Invalid file pointer in vfs_file_size\n");
        return 0;
    }
    // Assuming file->file (of type file_t*) contains a member 'size'
    return file->file->size;
}