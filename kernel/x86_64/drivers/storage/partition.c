#include <string.h>
#include <mm/pmm.h>
#include <printf.h>
#include "storage/partition.h"
#include "storage/block_device.h"
#include "panic.h"

// 0xAA55 in little-endian at offset 510
static const uint16_t MBR_SIGNATURE_LE = 0xAA55;

// -------------------------------------------------------------------------
// Partition read/write callbacks for DEV_PART
// -------------------------------------------------------------------------
static int partition_read(block_device_t *bdev, uint64_t lba, uint32_t count, void *buffer) {
    if (bdev->type != DEV_PART) return -1;

    block_device_t *parent = bdev->dev.partition.parent;
    uint64_t start_lba     = bdev->dev.partition.start_lba;

    // Bounds check
    if (lba + count > bdev->sector_count) {
        kprintf("Partition read OOB: lba=%llu count=%u (part size=%llu)\n",
                (unsigned long long)lba, count,
                (unsigned long long)bdev->sector_count);
        return -1;
    }

    // Delegate to parent
    // kprintf("start_lba: %llu, lba: %llu, count: %llu\n", start_lba, lba, count);
    return parent->read(parent, start_lba + lba, count, buffer);
}

static int partition_write(block_device_t *bdev, uint64_t lba, uint32_t count, const void *buffer) {
    if (bdev->type != DEV_PART) return -1;

    block_device_t *parent = bdev->dev.partition.parent;
    uint64_t start_lba     = bdev->dev.partition.start_lba;

    // Bounds check
    if (lba + count > bdev->sector_count) {
        kprintf("Partition write OOB: lba=%llu count=%u (part size=%llu)\n",
                (unsigned long long)lba, count,
                (unsigned long long)bdev->sector_count);
        return -1;
    }
    if (!parent->write) {
        kprintf("Partition: parent device has no write function.\n");
        return -1;
    }

    return parent->write(parent, start_lba + lba, count, buffer);
}

// -------------------------------------------------------------------------
// Utility: Create/register a new partition device. 
// Uses block_device_register(...) so it remains consistent with the 
// rest of your block devices.
// -------------------------------------------------------------------------
static int register_partition(block_device_t *disk, uint64_t start_lba, uint64_t sector_count) {
    char namebuf[16];

    // We'll name each partition "pN" where N = next device index
    int next_index = get_block_device_count();
    ksnprintf(namebuf, sizeof(namebuf), "p%d", next_index);

    block_device_t *pdev = block_device_register(
        DEV_PART,
        /*io_base=*/0, /*ctrl_base=*/0, /*channel=*/0, /*drive=*/0,
        sector_count,
        partition_read,
        partition_write,
        namebuf
    );
    if (!pdev) {
        kprintf("Partition: block_device_register failed\n");
        return -1;
    }
    // Set up partition fields
    pdev->dev.partition.parent    = disk;
    pdev->dev.partition.start_lba = start_lba;

    kprintf("Partition: Registered device #%d for LBA range [%llu..%llu]\n",
            next_index,
            (unsigned long long)start_lba,
            (unsigned long long)(start_lba + sector_count - 1));

    return next_index;
}


// -------------------------------------------------------------------------
// Forward declarations
// -------------------------------------------------------------------------
static int parse_mbr(block_device_t *disk, const uint8_t *mbr);
static int parse_gpt(block_device_t *disk);

// -------------------------------------------------------------------------
// partition_scan (main function): Reads MBR first, checks if GPT or MBR
// -------------------------------------------------------------------------
int partition_scan(block_device_t *disk) {
    kprintf("partition_scan: Entering, disk pointer = %p\n", disk);
    if (!disk) {
        kprintf("partition_scan: disk is NULL\n");
        return -1;
    }

    kprintf("partition_scan: Attempting to read LBA 0...\n");
    uint8_t sector0[512];
    int ret = block_read(disk, 0, 1, sector0);
    kprintf("partition_scan: block_read returned %d\n", ret);
    if (ret != 0) {
        kprintf("partition_scan: could not read LBA 0\n");
        return -1;
    }

    // Dump the first 16 bytes of sector0 for debugging purposes.
    kprintf("partition_scan: First 16 bytes of LBA 0: ");
    for (int i = 0; i < 16; i++) {
        kprintf("%02x ", sector0[i]);
    }
    kprintf("\n");

    // Verify the MBR signature at offset 510..511.
    uint16_t sig = *(uint16_t*)(sector0 + 510);
    kprintf("partition_scan: MBR signature = 0x%04x\n", sig);
    if (sig != MBR_SIGNATURE_LE) {
        kprintf("partition_scan: Missing 0x55AA signature, no partition table?\n");
        return -1;
    }

    // Distinguish GPT vs plain MBR.
    const mbr_partition_entry_t *parts = (const mbr_partition_entry_t*)(sector0 + 0x1BE);
    kprintf("partition_scan: Partition entry 0 type = 0x%02x\n", parts[0].type);
    if (parts[0].type == 0xEE) {
        kprintf("partition_scan: Detected GPT protective MBR\n");
        return parse_gpt(disk);
    } else {
        kprintf("partition_scan: Detected plain MBR, proceeding to parse MBR\n");
        return parse_mbr(disk, sector0);
    }
}

// -------------------------------------------------------------------------
// parse MBR
// -------------------------------------------------------------------------
static int parse_mbr(block_device_t *disk, const uint8_t *mbr) {
    const mbr_partition_entry_t *parts = (const mbr_partition_entry_t*)(mbr + 0x1BE);
    int found_count = 0;
    int extended_index = -1;
    uint64_t extended_base = 0;

    // primary partitions
    for (int i = 0; i < 4; i++) {
        uint8_t ptype = parts[i].type;
        if (!ptype) continue;  // skip empty
        uint64_t start  = parts[i].lba_start;
        uint64_t length = parts[i].lba_length;
        if (!length) continue;

        // check if extended
        if (ptype == 0x05 || ptype == 0x0F || ptype == 0x85) {
            extended_index = i;
            extended_base  = start;
        } else {
            // normal partition
            if (register_partition(disk, start, length) >= 0) {
                found_count++;
            }
        }
    }

    // handle extended/logical partitions
    if (extended_index != -1) {
        uint8_t ebr[512];
        uint64_t ebr_lba = extended_base;
        while (1) {
            if (block_read(disk, ebr_lba, 1, ebr) != 0) {
                kprintf("MBR: read error at EBR LBA %llu\n",(unsigned long long)ebr_lba);
                break;
            }
            uint16_t sig = *(uint16_t*)(ebr + 510);
            if (sig != MBR_SIGNATURE_LE) {
                kprintf("MBR: EBR lacks 0x55AA at LBA %llu\n",(unsigned long long)ebr_lba);
                break;
            }

            // in EBR, the partition entries are also at offset 0x1BE
            mbr_partition_entry_t *ebr_part = (mbr_partition_entry_t*)(ebr + 0x1BE);

            // 1st entry is the logical partition
            uint8_t lptype    = ebr_part[0].type;
            uint64_t lstart   = ebr_part[0].lba_start;
            uint64_t llength  = ebr_part[0].lba_length;
            if (lptype && llength) {
                uint64_t abs_start = ebr_lba + lstart;
                if (register_partition(disk, abs_start, llength) >= 0) {
                    found_count++;
                }
            }

            // 2nd entry points to the next EBR
            uint8_t next_type  = ebr_part[1].type;
            uint32_t next_start = ebr_part[1].lba_start;
            if (!next_type || !next_start) {
                break;  // no more
            }
            ebr_lba = extended_base + next_start;
        }
    }

    if (found_count == 0) {
        kprintf("parse_mbr: no partitions found.\n");
        return -1;
    }
    return found_count;
}

// -------------------------------------------------------------------------
// parse GPT
// -------------------------------------------------------------------------
static uint32_t gpt_crc32(const void *data, size_t len) {
    // standard polynomial 0xEDB88320
    uint32_t crc = 0xFFFFFFFF;
    const uint8_t *p = data;
    for (size_t i = 0; i < len; i++) {
        crc ^= p[i];
        for (int b = 0; b < 8; b++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc = (crc >> 1);
        }
    }
    return ~crc;
}

static int parse_gpt(block_device_t *disk) {
    uint8_t header_buf[512];
    // The primary GPT header is at LBA1
    if (block_read(disk, 1, 1, header_buf) != 0) {
        kprintf("parse_gpt: could not read GPT header at LBA1\n");
        return -1;
    }

    gpt_header_t *gpt = (gpt_header_t *)header_buf;
    if (memcmp(gpt->signature, "EFI PART", 8) != 0) {
        kprintf("parse_gpt: invalid GPT signature\n");
        return -1;
    }
    // Check header CRC
    uint32_t orig_crc = gpt->header_crc32;
    gpt->header_crc32 = 0; // zero before computing
    uint32_t calc_crc = gpt_crc32(gpt, gpt->header_size);
    gpt->header_crc32 = orig_crc; // restore
    if (calc_crc != orig_crc) {
        kprintf("parse_gpt: GPT header CRC mismatch!\n");
        // you can fail or proceed
        // return -1;
    }

    uint64_t entries_lba = gpt->partition_entries_lba;
    uint32_t count       = gpt->num_partition_entries;
    uint32_t esz         = gpt->partition_entry_size;
    if (esz < sizeof(gpt_partition_entry_t) || count == 0) {
        kprintf("parse_gpt: suspicious partition entry size/count\n");
        return -1;
    }

    // read the partition entries array
    uint64_t total_bytes = (uint64_t)count * esz;
    uint32_t sectors     = (uint32_t)((total_bytes + 511) / 512); // round up
    uint8_t *entries_buf = kmalloc(sectors * 512);
    if (!entries_buf) {
        kprintf("parse_gpt: out of memory\n");
        return -1;
    }
    if (block_read(disk, entries_lba, sectors, entries_buf) != 0) {
        kprintf("parse_gpt: could not read partition entries\n");
        kfree(entries_buf);
        return -1;
    }
    // check partition entries CRC
    uint32_t entries_crc = gpt_crc32(entries_buf, count * esz);
    if (entries_crc != gpt->partition_entries_crc32) {
        kprintf("parse_gpt: warning - partition entries CRC mismatch!\n");
        // can proceed or fail; your choice
    }

    // parse each partition entry
    int found_count = 0;
    for (uint32_t i = 0; i < count; i++) {
        gpt_partition_entry_t *pent = 
            (gpt_partition_entry_t *)(entries_buf + i * esz);

        // check if this entry is unused (type GUID all zero)
        int all_zero = 1;
        for (int j = 0; j < 16; j++) {
            if (pent->type_guid[j] != 0) {
                all_zero = 0;
                break;
            }
        }
        if (all_zero) continue; // skip

        uint64_t start = pent->start_lba;
        uint64_t end   = pent->end_lba;
        if (start == 0 || end < start) continue;

        uint64_t length = (end - start + 1);
        if (length == 0) continue;

        if (register_partition(disk, start, length) >= 0) {
            found_count++;
        }
    }
    kfree(entries_buf);

    if (found_count == 0) {
        kprintf("parse_gpt: no valid partitions found.\n");
        return -1;
    }
    return found_count;
}
