#ifndef KERNEL_STORAGE_ATA_H
#define KERNEL_STORAGE_ATA_H

// ATA register ports for primary and secondary channels
#define PRI_IO_BASE    0x1F0  // Data register base for primary
#define PRI_CTRL_BASE  0x3F6  // Control (alternate status) for primary
#define SEC_IO_BASE    0x170  // Data register base for secondary
#define SEC_CTRL_BASE  0x376  // Control for secondary

// ATA registers offsets from IO base
#define REG_DATA       0x00
#define REG_ERROR      0x01  // (read) also features (write)
#define REG_SECCOUNT   0x02
#define REG_LBA_LO     0x03
#define REG_LBA_MID    0x04
#define REG_LBA_HI     0x05
#define REG_DRIVE      0x06
#define REG_STATUS     0x07  // (read) status, (write) command

// Status bits
#define ATA_SR_BSY     0x80  // Busy 
#define ATA_SR_DRDY    0x40  // Drive ready
#define ATA_SR_DF      0x20  // Device fault
#define ATA_SR_DRQ     0x08  // Data request (ready to transfer)
#define ATA_SR_ERR     0x01  // Error

// Commands
#define ATA_CMD_IDENTIFY    0xEC
#define ATA_CMD_READ_SECT   0x20
#define ATA_CMD_WRITE_SECT  0x30
#define ATA_CMD_CACHE_FLUSH 0xE7

#endif
