#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "storage/block_device.h"
#include <kernel.h>
#include <storage/fs/fat32.h>
#include <mm/pmm.h>
#include <string.h>



/** Spinlock functions for fat32_volume (simple test-and-set). */
static inline void fat32_lock(fat32_volume_t *vol) {
    while (__sync_lock_test_and_set(&vol->lock, 1)) { }
}
static inline void fat32_unlock(fat32_volume_t *vol) {
    __sync_lock_release(&vol->lock);
}

/** Read a single sector from the volume into buffer. Returns 0 on success. */
static int fat32_read_sector(fat32_volume_t *vol, uint32_t sector, uint8_t *buffer) {
    if (block_read(vol->bdev, sector, 1, buffer) != 0) {
        kprintf("FAT32: Error reading sector %u\n", sector);
        return -1;
    }
    return 0;
}

/** Write a single sector from buffer to the volume. Returns 0 on success. */
static int fat32_write_sector(fat32_volume_t *vol, uint32_t sector, const uint8_t *buffer) {
    if (block_write(vol->bdev, sector, 1, buffer) != 0) {
        kprintf("FAT32: Error writing sector %u\n", sector);
        return -1;
    }
    return 0;
}

/** Convert a cluster number to the LBA (sector number) of the first sector of that cluster. */
static uint64_t cluster_to_lba(fat32_volume_t *vol, uint32_t cluster) {
    // Cluster numbering starts at 2 for the first data cluster.
    // LBA of cluster N = start of data region + (N - 2) * sectors_per_cluster.
    uint64_t first_data_sector = vol->reserved_sectors + vol->num_fats * vol->sectors_per_fat;
    return first_data_sector + (uint64_t)(cluster - 2) * vol->sectors_per_cluster;
}

/** Read a cluster (all sectors in it) into buffer. Buffer must be at least cluster_size bytes. */
static int fat32_read_cluster(fat32_volume_t *vol, uint32_t cluster, uint8_t *buffer) {
    uint64_t start_sector = cluster_to_lba(vol, cluster);
    for (uint32_t i = 0; i < vol->sectors_per_cluster; ++i) {
        if (fat32_read_sector(vol, start_sector + i, buffer + i * vol->bytes_per_sector) != 0) {
            return -1;
        }
    }
    return 0;
}

/** Write a cluster from buffer to disk. */
static int fat32_write_cluster(fat32_volume_t *vol, uint32_t cluster, const uint8_t *buffer) {
    uint64_t start_sector = cluster_to_lba(vol, cluster);
    for (uint32_t i = 0; i < vol->sectors_per_cluster; ++i) {
        if (fat32_write_sector(vol, start_sector + i, buffer + i * vol->bytes_per_sector) != 0) {
            return -1;
        }
    }
    return 0;
}

/** Get the value of the FAT entry for a given cluster (i.e., the next cluster in chain). */
static uint32_t fat32_get_fat_entry(fat32_volume_t *vol, uint32_t cluster) {
    // Each FAT32 entry is 4 bytes. We might read sector(s) containing the entry.
    uint32_t fat_offset = cluster * 4;
    uint32_t sector_num = vol->reserved_sectors + (fat_offset / vol->bytes_per_sector);
    uint32_t offset_in_sector = fat_offset % vol->bytes_per_sector;
    uint8_t sector_buffer[512];
    if (fat32_read_sector(vol, sector_num, sector_buffer) != 0) {
        return FAT32_EOC; // on error, return end-of-chain to avoid further damage
    }
    uint32_t entry_val = *(uint32_t*)(sector_buffer + offset_in_sector);
    entry_val &= 0x0FFFFFFF;  // FAT32 entries are 28-bit (top 4 bits reserved)
    return entry_val;
}

/** Set the FAT entry for a given cluster to a new value (next cluster or EOC). */
static int fat32_set_fat_entry(fat32_volume_t *vol, uint32_t cluster, uint32_t value) {
    uint32_t fat_offset = cluster * 4;
    uint32_t sector_num = vol->reserved_sectors + (fat_offset / vol->bytes_per_sector);
    uint32_t offset_in_sector = fat_offset % vol->bytes_per_sector;
    uint8_t sector_buffer[512];
    if (fat32_read_sector(vol, sector_num, sector_buffer) != 0) {
        return -1;
    }
    // Mask value to 28 bits and write it
    uint32_t newval = value & 0x0FFFFFFF;
    *(uint32_t*)(sector_buffer + offset_in_sector) = (*(uint32_t*)(sector_buffer + offset_in_sector) & 0xF0000000) | newval;
    // Write the sector back
    if (fat32_write_sector(vol, sector_num, sector_buffer) != 0) {
        return -1;
    }
    return 0;
}

/** Find a free cluster by scanning the FAT. Returns cluster number or 0 if none free. */
static uint32_t fat32_find_free_cluster(fat32_volume_t *vol) {
    // We scan FAT sector by sector for a 0 entry
    uint8_t sector_buffer[512];
    uint32_t fat_sector_start = vol->reserved_sectors;
    for (uint32_t i = 0; i < vol->sectors_per_fat; ++i) {
        if (fat32_read_sector(vol, fat_sector_start + i, sector_buffer) != 0) {
            return 0;
        }
        // Examine this FAT sector
        for (uint32_t offset = 0; offset < vol->bytes_per_sector; offset += 4) {
            uint32_t entry_val = *(uint32_t*)(sector_buffer + offset) & 0x0FFFFFFF;
            if (entry_val == FAT32_FREE_CLUSTER) {
                // Calculate cluster index corresponding to this entry.
                uint32_t cluster_index = (i * vol->bytes_per_sector + offset) / 4;
                // We skip clusters 0 and 1 (not used in FAT indexing)
                if (cluster_index >= 2) {
                    return cluster_index;
                }
            }
        }
    }
    return 0; // no free cluster found
}

/** Allocate a new cluster (mark it as end-of-chain in FAT) and return its number. */
static uint32_t fat32_alloc_cluster(fat32_volume_t *vol) {
    uint32_t free_cluster = fat32_find_free_cluster(vol);
    if (free_cluster == 0) {
        kprintf("FAT32: No free cluster available (disk full)\n");
        return 0;
    }
    // Mark this cluster as end-of-chain in FAT
    if (fat32_set_fat_entry(vol, free_cluster, 0x0FFFFFFF) != 0) {
        kprintf("FAT32: Failed to mark cluster %u as EOC\n", free_cluster);
        return 0;
    }
    return free_cluster;
}

/** Compare a filename (8.3 format) with a directory entry name. Returns true if match. */
static bool fat32_match_filename(const char *name83, const uint8_t dirName[11]) {
    // name83 is assumed to be uppercase and formatted as 11 char (8 name + 3 ext, space-padded).
    return (memcmp(name83, dirName, 11) == 0);
}

/** Convert a normal filename (e.g., "FILE.TXT") to FAT 8.3 format (11-byte array). */
static void fat32_make_83_name(const char *name, char out[11]) {
    // Initialize output with spaces
    memset(out, ' ', 11);
    // Split name and extension
    const char *dot = strrchr(name, '.');
    size_t name_len = dot ? (size_t)(dot - name) : strlen(name);
    size_t ext_len  = dot ? strlen(dot+1) : 0;
    if (name_len > 8) name_len = 8;
    if (ext_len > 3) ext_len = 3;
    // Copy and uppercase
    for (size_t i = 0; i < name_len; ++i) {
        out[i] = toupper((unsigned char)name[i]);
    }
    for (size_t j = 0; j < ext_len; ++j) {
        out[8 + j] = toupper((unsigned char)dot[1 + j]);
    }
}

/** Search a directory (by starting cluster) for an entry with the given 8.3 name. 
    Outputs the directory entry (32 bytes) in out_entry and its cluster/offset if found. */
/** Search a directory (by starting cluster) for an entry with the given 8.3 name.
    Outputs the directory entry (32 bytes) in out_entry and its cluster/offset if found. */
    static int fat32_dir_find_entry(fat32_volume_t *vol, uint32_t dir_cluster, const char *name83,
        uint8_t out_entry[32], uint32_t *out_entry_cluster, uint32_t *out_entry_offset) {
uint8_t *cluster_buf = kmalloc(vol->bytes_per_sector * vol->sectors_per_cluster);
if (!cluster_buf) return -1;
uint32_t current_cluster = dir_cluster;
while (current_cluster >= 2 && current_cluster < FAT32_EOC) {
if (fat32_read_cluster(vol, current_cluster, cluster_buf) != 0) {
kfree(cluster_buf);
return -1;
}
size_t entries_per_cluster = (vol->bytes_per_sector * vol->sectors_per_cluster) / 32;
for (size_t i = 0; i < entries_per_cluster; ++i) {
uint8_t *entry = cluster_buf + i * 32;
uint8_t firstChar = entry[0];
if (firstChar == 0x00) {
// No more entries in this cluster; directory entries are terminated here.
kprintf("FAT32: End of entries reached at index %zu in cluster %u\n", i, current_cluster);
kfree(cluster_buf);
return -1;
}
if (firstChar == 0xE5) {
// Deleted entry, skip it.
continue;
}
uint8_t attr = entry[11];
if (attr == 0x0F) {
// Long file name entry, skip it.
continue;
}
// Build a temporary string for logging.
char entryName[12];
memcpy(entryName, entry, 11);
entryName[11] = '\0';
kprintf("FAT32: Scanned entry at index %zu in cluster %u: '%s', attr: 0x%02X\n", i, current_cluster, entryName, attr);
// If this entry matches the requested 8.3 name, return it.
if (fat32_match_filename(name83, entry)) {
memcpy(out_entry, entry, 32);
*out_entry_cluster = current_cluster;
*out_entry_offset = i;
kprintf("FAT32: Matching entry found: '%s' at index %zu in cluster %u\n", entryName, i, current_cluster);
kfree(cluster_buf);
return 0;
}
}
// Move to the next cluster in the directory chain.
uint32_t next = fat32_get_fat_entry(vol, current_cluster);
if (next >= FAT32_EOC) {
kprintf("FAT32: End of directory chain reached after cluster %u\n", current_cluster);
break;
}
current_cluster = next;
}
kfree(cluster_buf);
return -1; // Entry not found.
}

/** Open a file or directory by path. Returns a file_t* or NULL on error. */
file_t* fat32_open(const char *path, bool write) {
    if (!fat_vol || !path) return NULL;
    fat32_lock(fat_vol);

    // Start at root directory
    uint32_t current_cluster = fat_vol->root_cluster;

    // If path begins with '/', skip leading '/'
    if (path[0] == '/') {
        path++;
    }

    // Duplicate path string for tokenization
    char *path_copy = kmalloc(strlen(path) + 1);
    if (!path_copy) {
        fat32_unlock(fat_vol);
        return NULL;
    }
    strcpy(path_copy, path);

    char *token, *save;
    uint8_t dir_entry[32];
    uint32_t dir_entry_cluster = 0, dir_entry_offset = 0;
    bool found = true;

    token = strtok_r(path_copy, "/", &save);
    while (token) {
        char name83[11];
        fat32_make_83_name(token, name83);

        kprintf("FAT32: Looking for component '%s' (83 format: '%.11s') in cluster %u\n", token, name83, current_cluster);

        if (fat32_dir_find_entry(fat_vol, current_cluster, name83, dir_entry,
                                 &dir_entry_cluster, &dir_entry_offset) != 0) {
            kprintf("FAT32: Component '%s' not found in cluster %u\n", token, current_cluster);
            found = false;
            break;
        }

        kprintf("FAT32: Found component '%s' in cluster %u (entry cluster %u, offset %u)\n",
                token, current_cluster, dir_entry_cluster, dir_entry_offset);

        token = strtok_r(NULL, "/", &save);
        if (token != NULL) {
            // Ensure the found entry is a directory
            if (!(dir_entry[11] & 0x10)) {
                kprintf("FAT32: '%s' is not a directory (attribute: 0x%02X)\n", token, dir_entry[11]);
                fat32_unlock(fat_vol);
                kfree(path_copy);
                return NULL;
            }

            // Compute starting cluster of the subdirectory
            uint32_t high = *(uint16_t*)(dir_entry + 20);
            uint32_t low  = *(uint16_t*)(dir_entry + 26);
            current_cluster = ((high << 16) | low);

            if (current_cluster == 0) {
                // In FAT32, cluster 0 implies root dir
                current_cluster = fat_vol->root_cluster;
            }

            kprintf("FAT32: Descending into subdirectory cluster %u\n", current_cluster);
        } else {
            // Last component found
            kprintf("FAT32: Target file or directory '%s' resolved at cluster %u\n", path, current_cluster);
            break;
        }
    }

    kfree(path_copy);

    if (!found) {
        kprintf("FAT32: Path '%s' not found\n", path);
        fat32_unlock(fat_vol);
        return NULL;
    }

    // We have the directory entry of the target file/dir in dir_entry
    file_t *file = kmalloc(sizeof(file_t));
    if (!file) {
        fat32_unlock(fat_vol);
        return NULL;
    }

    file->vol = fat_vol;
    uint32_t high = *(uint16_t*)(dir_entry + 20);
    uint32_t low  = *(uint16_t*)(dir_entry + 26);
    file->start_cluster = ((high << 16) | low);
    file->curr_cluster = (file->start_cluster == 0 ? fat_vol->root_cluster : file->start_cluster);
    file->size = *(uint32_t*)(dir_entry + 28);
    file->pos = 0;
    file->dir_cluster = dir_entry_cluster;
    file->dir_offset = dir_entry_offset;
    file->write = write;

    kprintf("FAT32: Open successful. Start cluster: %u, Size: %u bytes\n", file->start_cluster, file->size);

    fat32_unlock(fat_vol);
    return file;
}

/** Read from an open file. Returns number of bytes read (could be less than count at EOF) or -1 on error. */
int fat32_read(file_t *file, void *buffer, uint32_t count) {
    if (!file || !file->vol) return -1;
    fat32_lock(file->vol);
    if (file->pos >= file->size) {
        fat32_unlock(file->vol);
        return 0; // EOF reached
    }
    // Adjust count if it goes beyond EOF
    if (file->pos + count > file->size) {
        count = file->size - file->pos;
    }
    uint32_t bytes_per_cluster = file->vol->bytes_per_sector * file->vol->sectors_per_cluster;
    uint8_t *cluster_buf = kmalloc(bytes_per_cluster);
    if (!cluster_buf) {
        fat32_unlock(file->vol);
        return -1;
    }
    uint32_t bytes_read = 0;
    uint8_t *dest = (uint8_t*)buffer;
    while (bytes_read < count) {
        // If current cluster is 0 (file has no data clusters yet), break (nothing to read).
        if (file->curr_cluster == 0) {
            break;
        }
        // Load the current cluster from disk
        if (fat32_read_cluster(file->vol, file->curr_cluster, cluster_buf) != 0) {
            kprintf("FAT32: Error reading cluster %u\n", file->curr_cluster);
            break;
        }
        // Calculate offset within cluster and how many bytes to read from this cluster
        uint32_t cluster_offset = file->pos % bytes_per_cluster;
        uint32_t bytes_in_cluster = bytes_per_cluster - cluster_offset;
        uint32_t to_copy = (count - bytes_read < bytes_in_cluster) ? (count - bytes_read) : bytes_in_cluster;
        memcpy(dest + bytes_read, cluster_buf + cluster_offset, to_copy);
        bytes_read += to_copy;
        file->pos += to_copy;
        // If we reached end of cluster, move to next
        if (file->pos % bytes_per_cluster == 0) {
            // Advance to next cluster in chain
            uint32_t next_cluster = fat32_get_fat_entry(file->vol, file->curr_cluster);
            file->curr_cluster = (next_cluster >= FAT32_EOC) ? 0 : next_cluster;
        }
    }
    kfree(cluster_buf);
    fat32_unlock(file->vol);
    return bytes_read;
}

/** Write to an open file. Returns number of bytes written, or -1 on error. */
int fat32_write(file_t *file, const void *buffer, uint32_t count) {
    if (!file || !file->vol || !file->write) return -1;
    fat32_lock(file->vol);
    uint32_t bytes_per_cluster = file->vol->bytes_per_sector * file->vol->sectors_per_cluster;
    uint8_t *cluster_buf = kmalloc(bytes_per_cluster);
    if (!cluster_buf) {
        fat32_unlock(file->vol);
        return -1;
    }
    uint32_t bytes_written = 0;
    const uint8_t *src = (const uint8_t*)buffer;
    while (bytes_written < count) {
        // If current cluster is 0 (file has no cluster chain yet), allocate first cluster
        if (file->curr_cluster == 0) {
            uint32_t newclus = fat32_alloc_cluster(file->vol);
            if (newclus == 0) {
                break; // disk full or error
            }
            file->start_cluster = newclus;
            file->curr_cluster = newclus;
            // Update directory entry's start cluster (we'll do this after writing content and determining final size)
        }
        // Load cluster (if partially filled or we'll modify part of it)
        uint32_t cluster_offset = file->pos % bytes_per_cluster;
        uint32_t bytes_in_cluster = bytes_per_cluster - cluster_offset;
        uint32_t to_copy = (count - bytes_written < bytes_in_cluster) ? (count - bytes_written) : bytes_in_cluster;
        if (to_copy != bytes_per_cluster) {
            // We're not writing a full cluster, so read the cluster first to merge data
            if (fat32_read_cluster(file->vol, file->curr_cluster, cluster_buf) != 0) {
                kprintf("FAT32: Error reading cluster %u (for write)\n", file->curr_cluster);
                break;
            }
        } else {
            // Full cluster write; we can skip reading and just overwrite cluster_buf completely
            // (but cluster_buf is allocated, we'll use it directly)
        }
        // Copy data into cluster buffer at the appropriate offset
        memcpy(cluster_buf + cluster_offset, src + bytes_written, to_copy);
        // Write cluster back to disk
        if (fat32_write_cluster(file->vol, file->curr_cluster, cluster_buf) != 0) {
            kprintf("FAT32: Error writing cluster %u\n", file->curr_cluster);
            break;
        }
        file->pos += to_copy;
        bytes_written += to_copy;
        if (file->pos > file->size) {
            file->size = file->pos; // extend file size if we wrote past the old end
        }
        // If we filled the cluster and still have data, need to allocate next cluster
        if (file->pos % bytes_per_cluster == 0 && bytes_written < count) {
            uint32_t next_cluster = fat32_get_fat_entry(file->vol, file->curr_cluster);
            if (next_cluster >= FAT32_EOC) {
                // End of chain reached, allocate a new cluster
                uint32_t newclus = fat32_alloc_cluster(file->vol);
                if (newclus == 0) {
                    break; // disk full
                }
                // Link new cluster to current chain
                fat32_set_fat_entry(file->vol, file->curr_cluster, newclus);
                next_cluster = newclus;
            }
            file->curr_cluster = next_cluster;
        }
    }
    kfree(cluster_buf);
    // Update directory entry metadata (file size, first cluster) on disk
    if (bytes_written > 0) {
        // Compute sector containing the directory entry
        uint64_t dir_cluster_lba = cluster_to_lba(file->vol, file->dir_cluster);
        uint32_t entry_sector_offset = (file->dir_offset * 32) / file->vol->bytes_per_sector;
        uint32_t entry_offset_in_sector = (file->dir_offset * 32) % file->vol->bytes_per_sector;
        uint32_t entry_sector = dir_cluster_lba + entry_sector_offset;
        uint8_t sector_data[512];
        if (fat32_read_sector(file->vol, entry_sector, sector_data) == 0) {
            // Update file size
            *(uint32_t*)(sector_data + entry_offset_in_sector + 28) = file->size;
            // Update first cluster (if it was zero originally)
            uint16_t high = (uint16_t)((file->start_cluster >> 16) & 0xFFFF);
            uint16_t low  = (uint16_t)(file->start_cluster & 0xFFFF);
            *(uint16_t*)(sector_data + entry_offset_in_sector + 20) = high;
            *(uint16_t*)(sector_data + entry_offset_in_sector + 26) = low;
            fat32_write_sector(file->vol, entry_sector, sector_data);
        }
    }
    fat32_unlock(file->vol);
    return bytes_written;
}

/** Close an open file, releasing resources. */
int fat32_close(file_t *file) {
    if (!file) return -1;
    // (If file metadata needed flushing, we would do it here, but we handled it in write.)
    kfree(file);
    return 0;
}

/** Mount a FAT32 volume from a given block device. Reads the boot sector and initializes fat_vol. */
int fat32_mount(block_device_t *dev) {
    uint8_t sector0[512];
    if (block_read(dev, 0, 1, sector0) != 0) {
        kprintf("FAT32: Failed to read boot sector\n");
        return -1;
    }
    // Check FAT32 signature (bytes 0x52-0x59 should be "FAT32   ")
    if (memcmp(sector0 + 0x52, "FAT32", 5) != 0) {
        kprintf("FAT32: Boot sector does not indicate FAT32\n");
        return -1;
    }
    fat_vol = kmalloc(sizeof(fat32_volume_t));
    if (!fat_vol) {
        return -1;
    }
    fat_vol->bdev = dev;
    fat_vol->bytes_per_sector   = *(uint16_t*)(sector0 + 0x0B);
    fat_vol->sectors_per_cluster = sector0[0x0D];
    fat_vol->reserved_sectors   = *(uint16_t*)(sector0 + 0x0E);
    fat_vol->num_fats           = sector0[0x10];
    fat_vol->sectors_per_fat    = *(uint32_t*)(sector0 + 0x24);
    fat_vol->root_cluster       = *(uint32_t*)(sector0 + 0x2C);
    uint16_t total_sectors_16   = *(uint16_t*)(sector0 + 0x13);
    uint32_t total_sectors_32   = *(uint32_t*)(sector0 + 0x20);
    fat_vol->total_sectors = (total_sectors_16 != 0) ? total_sectors_16 : total_sectors_32;
    fat_vol->fat_table = NULL;
    fat_vol->fat_dirty = false;
    fat_vol->lock = 0;
    kprintf("FAT32: Mounted volume – %u bytes/sector, %u sec/cluster, %u FATs, root_cluster=%u\n",
            fat_vol->bytes_per_sector, fat_vol->sectors_per_cluster,
            fat_vol->num_fats, fat_vol->root_cluster);
    return 0;
}
