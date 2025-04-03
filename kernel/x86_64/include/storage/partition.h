#ifndef PARTITION_H
#define PARTITION_H

#include <stdint.h>
#include <stddef.h>
#include "storage/block_device.h"  // your existing code: typedef struct block_device ...

// -------------------------------------------------------------------------
// MBR structures (16 bytes each) and GPT structures
// -------------------------------------------------------------------------
#pragma pack(push, 1)
typedef struct {
    uint8_t  status;
    uint8_t  chs_start[3];
    uint8_t  type;
    uint8_t  chs_end[3];
    uint32_t lba_start;
    uint32_t lba_length;
} mbr_partition_entry_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint8_t  signature[8];  // "EFI PART"
    uint32_t revision;
    uint32_t header_size;
    uint32_t header_crc32;
    uint32_t reserved;
    uint64_t current_lba;
    uint64_t backup_lba;
    uint64_t first_usable_lba;
    uint64_t last_usable_lba;
    uint8_t  disk_guid[16];
    uint64_t partition_entries_lba;
    uint32_t num_partition_entries;
    uint32_t partition_entry_size;
    uint32_t partition_entries_crc32;
    // remainder up to sector size is reserved
} gpt_header_t;

#pragma pack(push, 1)
typedef struct {
    uint8_t  type_guid[16];
    uint8_t  unique_guid[16];
    uint64_t start_lba;
    uint64_t end_lba; // inclusive
    uint64_t attributes;
    uint16_t name[36]; // UTF-16
} gpt_partition_entry_t;
#pragma pack(pop)

/*
 * Partition scanning interface
 * - Scans the given 'disk' (which presumably is DEV_ATA or DEV_AHCI) for MBR or GPT
 * - Registers each found partition as a DEV_PART block_device
 * - Returns count of partitions found, or -1 on error
 */
int partition_scan(block_device_t *disk);

#endif // PARTITION_H
