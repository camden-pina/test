#ifndef KERNEL_STORAGE_FS_FAT32_H
#define KERNEL_STORAGE_FS_FAT32_H

#include <stdint.h>
#include <stddef.h>
#include <storage/block_device.h>

// FAT32 on-disk constants
#define FAT32_EOC 0x0FFFFFF8  // Any value >= 0x0FFFFFF8 is considered end-of-chain marker
#define FAT32_FREE_CLUSTER 0x00000000

// FAT32 volume information structure
typedef struct fat32_volume {
    block_device_t *bdev;      // underlying block device
    uint32_t bytes_per_sector;
    uint32_t sectors_per_cluster;
    uint32_t reserved_sectors;
    uint32_t num_fats;
    uint32_t sectors_per_fat;
    uint32_t root_cluster;     // first cluster of root directory
    uint64_t total_sectors;
    // In-memory FAT buffer (optional caching):
    uint32_t *fat_table;       // if we choose to load FAT into memory
    bool     fat_dirty;
    // Spinlock for thread safety
    volatile uint8_t lock;
} fat32_volume_t;

// We can cache the mounted volume globally (assuming one volume mounted as root)
static fat32_volume_t *fat_vol = NULL;

// File object for an open file on FAT32
typedef struct file {
    fat32_volume_t *vol;
    uint32_t start_cluster;
    uint32_t curr_cluster;    // current cluster in file (for sequential access)
    uint32_t size;            // file size in bytes
    uint32_t pos;             // current file offset (0-based)
    uint32_t dir_cluster;     // cluster containing this file's directory entry
    uint32_t dir_offset;      // index of this file's entry within that cluster
    bool     write;           // opened for write?
} file_t;

file_t* fat32_open(const char *path, bool write);
int fat32_read(file_t *file, void *buffer, uint32_t count);
int fat32_write(file_t *file, const void *buffer, uint32_t count);
int fat32_close(file_t *file);
int fat32_mount(block_device_t *dev);

#endif
