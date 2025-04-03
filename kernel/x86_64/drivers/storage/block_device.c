#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "kernel.h"   // for kprintf, kmalloc, etc.
#include <storage/block_device.h>
#include <mm/pmm.h>

#include <storage/block_device.h>
#include <kernel.h>
#include <string.h>
#include <mm/pmm.h>
#include <panic.h>

#define MAX_BLOCK_DEVICES 16

block_device_t *g_block_devices[MAX_BLOCK_DEVICES];
size_t g_block_device_count = 0;

block_device_t *block_device_register(device_type_t type,
                                        uint16_t io_base,
                                        uint16_t ctrl_base,
                                        uint8_t channel,
                                        uint8_t drive,
                                        uint64_t sectors,
                                        block_read_fn read_fn,
                                        block_write_fn write_fn,
                                        const char *name)
{
    if (g_block_device_count >= MAX_BLOCK_DEVICES) {
        kprintf("BlockDev: Cannot register more devices, max=%d reached.\n", MAX_BLOCK_DEVICES);
        return NULL;
    }

    block_device_t *dev = kmalloc(sizeof(block_device_t));
    if (!dev) {
        kprintf("BlockDev: Allocation failed for device struct\n");
        return NULL;
    }
    memset(dev, 0, sizeof(block_device_t));

    dev->type         = type;
    dev->sector_size  = 512;  // default; FS driver may override after probing.
    dev->sector_count = sectors;
    dev->read         = read_fn;
    dev->write        = write_fn;

    if (name) {
        strncpy(dev->name, name, sizeof(dev->name));
        dev->name[sizeof(dev->name)-1] = '\0';
    } else {
        dev->name[0] = '\0';
    }

    // Fill union fields based on device type.
    switch (type) {
    case DEV_ATA:
        dev->dev.ata.io_base   = io_base;
        dev->dev.ata.ctrl_base = ctrl_base;
        dev->dev.ata.channel   = channel;
        dev->dev.ata.drive     = drive;
        break;
    case DEV_AHCI:
        // For AHCI, we expect the AHCI driver to later assign dev.ahci.port.
        break;
    case DEV_FLOPPY:
        // Add floppy-specific assignments if needed.
        break;
    default:
        kprintf("BlockDev: Unknown device type %d\n", type);
        break;
    }

    // Store the device index in the device structure.
    dev->device_id = g_block_device_count;
    g_block_devices[g_block_device_count++] = dev;
    kprintf("BlockDev: Registered device '%s' (type=%d, sectors=%llu)\n",
            dev->name, dev->type, (unsigned long long)dev->sector_count);

    return dev;
}

int get_block_device_count(void) {
    return g_block_device_count;
}

block_device_t *get_block_device(int index) {
    if (index < 0 || index >= g_block_device_count)
        return NULL;
    return g_block_devices[index];
}

/** Generic block read: reads 'count' sectors starting at LBA 'lba' into 'buffer'. */
int block_read(block_device_t *dev, uint64_t lba, uint32_t count, void *buffer) {
    if (!dev || !dev->read) return -1;
    // Bounds check: don't read beyond end of device
    if (lba + count > dev->sector_count) {
        kprintf("BlockDev: Read beyond end (lba=%llu, count=%u)\n", lba, count);
        return -1;
    }
    // Delegate to the device-specific read implementation
    return dev->read(dev, lba, count, buffer);
}

/** Generic block write: writes 'count' sectors from 'buffer' to LBA 'lba'. */
int block_write(block_device_t *dev, uint64_t lba, uint32_t count, const void *buffer) {
    if (!dev || !dev->write) return -1;
    if (lba + count > dev->sector_count) {
        kprintf("BlockDev: Write beyond end (lba=%llu, count=%u)\n", lba, count);
        return -1;
    }
    return dev->write(dev, lba, count, buffer);
}

void block_list() {
    for (int i = 0; i < g_block_device_count; i++) {
        block_device_t *bdev = g_block_devices[i];

        switch (bdev->type) {
            case DEV_AHCI:
                kprintf("Device ID: %llu, Type: DEV_AHCI, Name: %s, Sector Count: %llu, Sector Size: %llu\n", bdev->device_id, bdev->name, bdev->sector_count, bdev->sector_size);
            break;
            case DEV_PART:
                kprintf("Device ID: %llu, Type: DEV_PART, Name: %s, Sector Count: %llu, Sector Size: %llu, Start LBA: %llu\n", bdev->device_id, bdev->name, bdev->sector_count, bdev->sector_size, bdev->dev.partition.start_lba);
            break;
            case DEV_ATA:
            case DEV_FLOPPY:
            default:
            panic("Undefined bdev->type: %llu", bdev->type);
        }
    }
}
