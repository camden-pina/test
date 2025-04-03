#ifndef KERNEL_STORAGE_BLOCK_DEVICE_H
#define KERNEL_STORAGE_BLOCK_DEVICE_H

#include <stdint.h>

// Maximum number of block devices we can register
#define MAX_BLOCK_DEVICES  8

// Supported device types
typedef enum {
    DEV_ATA,    // ATA/ATAPI disks (PIO or later maybe DMA/AHCI)
    DEV_AHCI,
    DEV_FLOPPY, // Floppy drive (to be implemented similarly)
    DEV_PART,
    // ... other types (USB, NVMe, etc.) can be added in future
} device_type_t;

// Forward declaration of block_device struct
struct block_device;

// Function pointer types for device I/O operations
typedef int (*block_read_fn)(struct block_device *dev, uint64_t lba, uint32_t count, void *buffer);
typedef int (*block_write_fn)(struct block_device *dev, uint64_t lba, uint32_t count, const void *buffer);

typedef struct block_device {
    device_type_t type;
    uint32_t sector_size;
    uint64_t sector_count;
    char name[16];
    int device_id;

    block_read_fn  read;
    block_write_fn write;

    // Instead of dedicated fields for ATA, unify them in a union:
    union {
        struct {
            uint16_t io_base;
            uint16_t ctrl_base;
            uint8_t  channel;
            uint8_t  drive;
        } ata;

        struct {
            struct ahci_port *port;
        } ahci;

        struct {
            struct block_device *parent;
            uint64_t start_lba;  // offset of this partition on parent
        } partition;

        // You could have more if you wish (NVMe, USB, etc.)
    } dev;
} block_device_t;

block_device_t *block_device_register(device_type_t type,
    uint16_t io_base,
    uint16_t ctrl_base,
    uint8_t channel,
    uint8_t drive,
    uint64_t sectors,
    block_read_fn read_fn,
    block_write_fn write_fn,
    const char *name);

int block_read(block_device_t *dev, uint64_t lba, uint32_t count, void *buffer);
int block_write(block_device_t *dev, uint64_t lba, uint32_t count, const void *buffer);

block_device_t *get_block_device(int index);
int get_block_device_count(void);

void block_list();

#endif
