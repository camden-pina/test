#ifndef KERNEL_VFS_H
#define KERNEL_VFS_H

#include <stdint.h>
#include <stdbool.h>
#include <storage/block_device.h>
#include <storage/fs/fat32.h>
#include <stddef.h>

// Filesystem operations table
typedef struct filesystem_ops {
    file_t* (*open)(const char *path, bool write);
    int     (*read)(file_t *file, void *buf, uint32_t count);
    int     (*write)(file_t *file, const void *buf, uint32_t count);
    int     (*close)(file_t *file);
} filesystem_ops_t;

// Filesystem descriptor
typedef struct filesystem {
    char fs_name[8];
    filesystem_ops_t ops;
    void *fs_private;  // e.g., pointer to FAT32 volume data
} filesystem_t;

// Mount table entry structure
typedef struct mount_entry {
    char mount_point[256];  // e.g., "/drive0", "/drive1"
    filesystem_t *fs;
    struct mount_entry *next;
} mount_entry_t;

// VFS file abstraction: wraps the underlying filesystem's file handle.
typedef struct vfs_file {
    filesystem_t *fs;
    file_t *file;
} vfs_file_t;

// VFS API functions
int vfs_mount(const char *mount_point, block_device_t *dev, const char *fs_type);
int vfs_unmount(const char *mount_point);
vfs_file_t* vfs_open(const char *path, const char *mode);
int vfs_read(vfs_file_t *file, void *buf, uint32_t count);
int vfs_write(vfs_file_t *file, const void *buf, uint32_t count);
int vfs_close(vfs_file_t *file);

uint32_t vfs_file_size(vfs_file_t *file);

#endif
