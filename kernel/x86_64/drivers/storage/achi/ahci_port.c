#include <storage/ahci/ahci.h>
#include <mm/pgtable.h>
#include <mm/pmm.h>
#include <string.h>
#include <printf.h>
#include <panic.h>

// Forward prototypes of local helpers
static ahci_dev_type_t ahci_check_port_device(volatile hba_port_t *port);
static void ahci_stop_port(volatile hba_port_t *port);
static void ahci_start_port(volatile hba_port_t *port);
static int ahci_port_initialize(ahci_port_t *port);
static int ahci_find_free_slot(ahci_port_t *port);

static int ahci_port_identify_cmd(ahci_port_t *port, uint16_t *id_buf, bool atapi);
static void ahci_identify_device(ahci_port_t *port, bool atapi);

// Called from ahci_init_controller() for each implemented port
void ahci_port_probe(ahci_port_t *port, uint32_t port_index) {
    volatile hba_port_t *r = port->regs;
    ahci_dev_type_t type = ahci_check_port_device(r);
    port->dev_type = type;
    if (type == AHCI_DEV_NONE) {
        return; // No device present
    }

    // Initialize port memory structures (command list, FIS, command tables, etc.)
    if (!ahci_port_initialize(port)) {
        kprintf("AHCI: Port %u init failed.\n", port_index);
        port->dev_type = AHCI_DEV_NONE;
        return;
    }

    // Identify the connected device.
    if (type == AHCI_DEV_SATA) {
        ahci_identify_device(port, false); // ATA Identify (0xEC)
    } else if (type == AHCI_DEV_SATAPI) {
        ahci_identify_device(port, true);  // ATAPI Identify Packet (0xA1)
    } else {
        kprintf("AHCI: Port %u device type not fully supported.\n", port_index);
        return;
    }

    // Register block device for this AHCI port.
    // For AHCI, we pass zeros for io_base, ctrl_base, channel, drive.
    char name[16];
    ksnprintf(name, sizeof(name), "ahci%u", port_index);
    block_device_t *bdev = block_device_register(DEV_AHCI,
                                                 0, 0, 0, 0,
                                                 port->sector_count,
                                                 ahci_read, ahci_write,
                                                 name);
    if (!bdev) {
        kprintf("AHCI: Failed to register block device for port %u\n", port_index);
        return;
    }
    // Set the AHCI-specific union field.
    bdev->dev.ahci.port = port;

    // Store the pointer in the port structure for future reference.
    port->bdev = bdev;

    kprintf("AHCI: Port %u => device registered: %s, %llu sectors, sector=%u\n",
            port_index, bdev->name,
            (unsigned long long)port->sector_count,
            port->sector_size);
}

// Quickly see if a device is present by reading ssts
static ahci_dev_type_t ahci_check_port_device(volatile hba_port_t *port) {
    uint32_t ssts = port->ssts;
    uint8_t det = ssts & 0xF; 
    uint8_t ipm = (ssts >> 8) & 0xF;

    if (det != 0x3 || ipm != 0x1) {
        return AHCI_DEV_NONE;
    }
    uint32_t sig = port->sig;
    switch (sig) {
    case SATA_SIG_ATAPI: return AHCI_DEV_SATAPI;
    case SATA_SIG_SEMB:  return AHCI_DEV_SEMB;
    case SATA_SIG_PM:    return AHCI_DEV_PM;
    default:             return AHCI_DEV_SATA;
    }
}

// For safety, stop the port
static void ahci_stop_port(volatile hba_port_t *port) {
    // Clear ST, FRE
    port->cmd &= ~HBA_PxCMD_ST;
    port->cmd &= ~HBA_PxCMD_FRE;
    // Wait for CR=0, FR=0
    while (port->cmd & (HBA_PxCMD_CR | HBA_PxCMD_FR)) {
        // spin
    }
}

// Start the port
static void ahci_start_port(volatile hba_port_t *port) {
    // wait for CR=0
    while (port->cmd & HBA_PxCMD_CR) { /* spin */ }
    // set FRE, ST
    port->cmd |= HBA_PxCMD_FRE;
    port->cmd |= HBA_PxCMD_ST;
}

/*
 * Allocate command list (1 page), FIS (1 page), command tables (8KB), etc.
 */
static int ahci_port_initialize(ahci_port_t *port) {
    volatile hba_port_t *pr = port->regs;

    // Stop the port so we can reconfigure
    ahci_stop_port(pr);

    // 1) Command List Buffer (4 KiB)
    uint64_t clb_dma = 0; // will store the physical DMA address
    uint8_t *clb_v = dma_alloc_coherent(4096, &clb_dma);
    if (!clb_v) {
        kprintf("AHCI: Failed to allocate command list buffer.\n");
        return 0;
    }
    memset(clb_v, 0, 4096);

    // Program the HBA registers with the lower/upper 32 bits
    pr->clb  = (uint32_t)(clb_dma & 0xFFFFFFFF);
    pr->clbu = (uint32_t)(clb_dma >> 32);

    // Save these in the port structure for later
    port->clb_virtual = clb_v;
    port->clb_phys    = clb_dma;

    // 2) FIS Receive Buffer (4 KiB)
    uint64_t fis_dma = 0;
    uint8_t *fis_v = dma_alloc_coherent(4096, &fis_dma);
    if (!fis_v) {
        kprintf("AHCI: Failed to allocate FIS buffer.\n");
        return 0;
    }
    memset(fis_v, 0, 4096);

    pr->fb  = (uint32_t)(fis_dma & 0xFFFFFFFF);
    pr->fbu = (uint32_t)(fis_dma >> 32);

    port->fis_virtual = fis_v;
    port->fis_phys    = fis_dma;

    // 3) Command Table Region (8 KiB total for 32 command tables, each 256 bytes)
    uint64_t ctbl_dma = 0;
    uint8_t *ctbl_v = dma_alloc_coherent(8192, &ctbl_dma);
    if (!ctbl_v) {
        kprintf("AHCI: Failed to allocate command table region.\n");
        return 0;
    }
    memset(ctbl_v, 0, 8192);

    // We also need arrays to track each command table pointer & physical address
    port->cmd_tables = (hba_cmd_tbl_t **)dma_alloc_coherent(sizeof(hba_cmd_tbl_t *) * 32, NULL);
    port->ctba_phys  = (uint64_t *)      dma_alloc_coherent(sizeof(uint64_t) * 32,      NULL);

    // If your kernel doesn't require these to be DMA-coherent themselves, you could
    // just kmalloc them. We'll assume it's safe either way.
    if (!port->cmd_tables || !port->ctba_phys) {
        kprintf("AHCI: Failed to allocate cmd_tables/ctba_phys arrays.\n");
        return 0;
    }

    // Initialize each slot in the 8KB command table region
    for (int i = 0; i < 32; i++) {
        port->cmd_tables[i] = (hba_cmd_tbl_t*)(ctbl_v + (i * 256));
        port->ctba_phys[i]  = ctbl_dma + (i * 256);
    }

    // 4) Fill the command list entries
    hba_cmd_header_t *cmd_header = (hba_cmd_header_t *)port->clb_virtual;
    for (int i = 0; i < 32; i++) {
        cmd_header[i].cfl   = sizeof(fis_reg_h2d_t) / 4;
        cmd_header[i].atapi = 0;   // assume SATA for now
        cmd_header[i].write = 0;   // default read
        cmd_header[i].prdtl = 8;   // up to 8 PRD entries
        cmd_header[i].prdbc = 0;
        uint64_t ctba = port->ctba_phys[i];
        cmd_header[i].ctba  = (uint32_t)(ctba & 0xFFFFFFFF);
        cmd_header[i].ctbau = (uint32_t)(ctba >> 32);
    }

    // 5) Clear interrupts, enable
    pr->is = 0xFFFFFFFF;
    pr->ie = 0xFFFFFFFF;

    // 6) Start port
    ahci_start_port(pr);

    return 1;
}

#include <errno.h>

// Issue an IDENTIFY (ATA=0xEC, ATAPI=0xA1)
static void ahci_identify_device(ahci_port_t *port, bool atapi) {
    // uint16_t id_buf[256];
    uint16_t *id_buf = dma_alloc_coherent(256, NULL);
    memset(id_buf, 0, 256);

    if (!ahci_port_identify_cmd(port, id_buf, atapi)) {
        kprintf("AHCI: Identify command failed.\n");
    }

    // Parse identify data for geometry
    port->sector_size = 512;
    bool lba48 = (id_buf[83] & (1<<10)) != 0;
    uint64_t sectors = 0;
    if (lba48) {
        sectors = ((uint64_t)id_buf[103]<<48) | ((uint64_t)id_buf[102]<<32)
                | ((uint64_t)id_buf[101]<<16) | (uint64_t)id_buf[100];
    } else {
        sectors = ((uint64_t)id_buf[61]<<16) | id_buf[60];
    }
    port->sector_count = sectors;
    // Check word106 bit12 for >512 sector
    if (id_buf[106] & (1<<12)) {
        uint32_t lss = ((uint32_t)id_buf[118]<<16) | id_buf[117];
        if (lss) {
            port->sector_size = lss;
        }
    }

    // Print model
    char model[41];
    for (int i=0; i<20; i++) {
        uint16_t w = id_buf[27 + i];
        model[2*i]   = (char)(w >> 8);
        model[2*i+1] = (char)(w & 0xFF);
    }
    model[40] = 0;
    for (int i=39; i>=0 && model[i]==' '; i--) {
        model[i] = 0;
    }
    kprintf("AHCI: Identify => Model='%s', sectors=%llu, sector_size=%u\n",
            model, (unsigned long long)port->sector_count, port->sector_size);
}

/*
 * Perform a single IDENTIFY command using slot=0, poll for completion.
 */
static int ahci_port_identify_cmd(ahci_port_t *port, uint16_t *id_buf, bool atapi) {
    spinlock_lock(&port->lock);
    int slot = ahci_find_free_slot(port);
    if (slot < 0) {
        spinlock_unlock(&port->lock);
        return 0;
    }
    port->active_slots |= (1u<<slot);

    hba_cmd_header_t *hdr = (hba_cmd_header_t*)(port->clb_virtual) + slot;
    hdr->cfl   = sizeof(fis_reg_h2d_t)/4;
    hdr->atapi = atapi ? 1 : 0;
    hdr->write = 0;
    hdr->prdtl = 1; // one PRD
    hdr->prdbc = 0;

    // Prepare command table
    hba_cmd_tbl_t *tbl = port->cmd_tables[slot];
    memset(tbl, 0, 256);
    uint64_t phys_idbuf = virt_to_phys(id_buf);
    tbl->prdt_entry[0].dba  = (uint32_t)(phys_idbuf & 0xFFFFFFFF);
    tbl->prdt_entry[0].dbau = (uint32_t)(phys_idbuf >> 32);
    tbl->prdt_entry[0].dbc  = 512 - 1;
    tbl->prdt_entry[0].i    = 1;

    // Construct FIS
    fis_reg_h2d_t *cfis = (fis_reg_h2d_t*)tbl->cfis;
    memset(cfis, 0, sizeof(fis_reg_h2d_t));
    cfis->fis_type = FIS_TYPE_REG_H2D;
    cfis->c = 1;
    cfis->command = atapi ? 0xA1 : 0xEC;
    cfis->device = 1<<6; // LBA bit
    cfis->countl = 1;

    // Issue
    port->regs->ci |= (1u<<slot);
    spinlock_unlock(&port->lock);

    // Poll
    int spin = 0;
    while ((port->regs->ci & (1u<<slot)) && spin<1000000) {
        if (port->regs->is & HBA_PxIS_TFES) {
            port->regs->is = HBA_PxIS_TFES;
            kprintf("AHCI: Identify: TFES error.\n");
            spinlock_lock(&port->lock);
            port->active_slots &= ~(1u<<slot);
            spinlock_unlock(&port->lock);
            return 0;
        }
        spin++;
    }
    // Mark slot free
    spinlock_lock(&port->lock);
    port->active_slots &= ~(1u<<slot);
    spinlock_unlock(&port->lock);
    if (spin==1000000) {
        kprintf("AHCI: Identify timed out.\n");
        return 0;
    }
    return 1;
}

/*
 * Return an unused command slot index, or -1 if full. 
 * We also check the hardware's "ci" to skip in-flight commands.
 */
static int ahci_find_free_slot(ahci_port_t *port) {
    uint32_t slots = port->active_slots | port->regs->ci;
    for (int i=0; i<32; i++) {
        if (!(slots & (1u<<i))) {
            return i;
        }
    }
    return -1;
}

/*
 * Provide port read/write sector functions for the block device callbacks.
 * This is a standard chunking approach if the request is large.
 */
#include <storage/ahci/ahci.h>
#include <mm/pmm.h>
#include <spinlock.h>
#include <printf.h>
#include <string.h>

/**
 * Helper to issue either a READ DMA EXT (0x25) or WRITE DMA EXT (0x35)
 * command for up to 'sector_count' sectors from/to 'buf'.
 *
 * Returns 0 on success, -1 on error.
 */
static int ahci_port_issue_rw_cmd(ahci_port_t *port, 
                                  bool write,
                                  uint64_t lba,
                                  uint32_t sector_count,
                                  void *buf)
{
    // 1) Lock the port so we don't collide with other commands.
    spinlock_lock(&port->lock);

    // 2) Find a free command slot.
    int slot = ahci_find_free_slot(port);
    if (slot < 0) {
        kprintf("AHCI: No free command slots for port read/write.\n");
        spinlock_unlock(&port->lock);
        return -1;
    }

    // Mark that slot as in use
    port->active_slots |= (1u << slot);

    // 3) Prepare the command header
    hba_cmd_header_t *cmd_header = (hba_cmd_header_t *)port->clb_virtual;
    hba_cmd_header_t *hdr = &cmd_header[slot];
    memset(hdr, 0, sizeof(hba_cmd_header_t));

    // Command FIS size = 5 DWORDs for Register H2D (20 bytes).
    hdr->cfl = sizeof(fis_reg_h2d_t) / 4;  // 20 bytes -> 5 DWORDS
    hdr->write = (write ? 1 : 0);          // 1 = write, 0 = read
    hdr->prdtl = 1;                       // We'll use 1 PRD entry for the entire chunk if physically contiguous
                                          // If you must scatter/gather, set prdtl to # of PRDs you fill in below.

    // 4) Get the command table for this slot
    hba_cmd_tbl_t *cmd_tbl = port->cmd_tables[slot];
    memset(cmd_tbl, 0, sizeof(hba_cmd_tbl_t));

    // 5) Fill the PRDT entry
    //    For simplicity, let's assume 'buf' is physically contiguous. 
    //    In a real OS, you'd map each segment or have a contiguous DMA buffer, etc.
    uint64_t phys_buf = virt_to_phys(buf);
    uint32_t byte_count = sector_count * port->sector_size;

    cmd_tbl->prdt_entry[0].dba  = (uint32_t)(phys_buf & 0xFFFFFFFF);
    cmd_tbl->prdt_entry[0].dbau = (uint32_t)((phys_buf >> 32) & 0xFFFFFFFF);
    // dbc is the byte count - 1
    cmd_tbl->prdt_entry[0].dbc  = (byte_count - 1) & 0x3FFFFF;  // 22 bits
    cmd_tbl->prdt_entry[0].i    = 1;   // interrupt on completion

    // 6) Fill in the CFIS for READ DMA EXT or WRITE DMA EXT
    fis_reg_h2d_t *cfis = (fis_reg_h2d_t *)cmd_tbl->cfis;
    memset(cfis, 0, sizeof(fis_reg_h2d_t));
    cfis->fis_type = FIS_TYPE_REG_H2D;
    cfis->c = 1;  // Command
    cfis->command = (write ? 0x35 : 0x25); // WRITE DMA EXT or READ DMA EXT

    // The SATA "device" register bit 6 must be set to enable LBA mode
    cfis->device = (1 << 6);

    // Load LBA (48-bit) into CFIS
    // LBA bits go into cfis->lba0..lba5
    cfis->lba0 = (uint8_t)((lba      ) & 0xFF);
    cfis->lba1 = (uint8_t)((lba >>  8) & 0xFF);
    cfis->lba2 = (uint8_t)((lba >> 16) & 0xFF);
    cfis->lba3 = (uint8_t)((lba >> 24) & 0xFF);
    cfis->lba4 = (uint8_t)((lba >> 32) & 0xFF);
    cfis->lba5 = (uint8_t)((lba >> 40) & 0xFF);

    // Load sector count (16-bit) into countl/count h
    // If sector_count == 65536, actually program 0x0000 to represent 65536.
    uint16_t sec_count = (sector_count == 65536) ? 0 : (uint16_t)sector_count;
    cfis->countl = (uint8_t)(sec_count & 0xFF);
    cfis->counth = (uint8_t)((sec_count >> 8) & 0xFF);

    // 7) Set up the cmd_header’s CTBA
    uint64_t ctba = port->ctba_phys[slot];
    hdr->ctba  = (uint32_t)(ctba & 0xFFFFFFFF);
    hdr->ctbau = (uint32_t)((ctba >> 32) & 0xFFFFFFFF);

    // 8) Kick off the command by setting bit=slot in port->regs->ci
    port->regs->ci |= (1u << slot);

    // 9) Poll for completion or error
    int spin_count = 0;
    while ((port->regs->ci & (1u << slot)) != 0) {
        // Check for task-file error
        if (port->regs->is & HBA_PxIS_TFES) {
            kprintf("AHCI: RW command TFES error (slot %d)\n", slot);
            // Clear TFES from port->regs->is
            port->regs->is = HBA_PxIS_TFES;
            // Mark slot free
            port->active_slots &= ~(1u << slot);
            spinlock_unlock(&port->lock);
            return -1;
        }
        if (spin_count++ > 10000000) {
            kprintf("AHCI: RW command timed out (slot %d)\n", slot);
            // Mark slot free
            port->active_slots &= ~(1u << slot);
            spinlock_unlock(&port->lock);
            return -1;
        }
    }

    // Mark slot free, unlock
    port->active_slots &= ~(1u << slot);
    spinlock_unlock(&port->lock);
    return 0;
}

/**
 * Reads `count` sectors from `port` at LBA `lba` into `buf`.
 */
int ahci_port_read_sectors(ahci_port_t *port, uint64_t lba, uint32_t count, void *buf) {
    // kprintf("AHCI: port_read_sectors: Entering, port=%p, LBA=%llu, count=%u, buf=%p\n",
            // port, (unsigned long long)lba, count, buf);
    
    if (!port) {
        kprintf("AHCI: port_read_sectors: port is NULL\n");
        return -1;
    }
    if (!buf) {
        kprintf("AHCI: port_read_sectors: buf is NULL\n");
        return -1;
    }
    // kprintf("AHCI: port sector_size = %u\n", port->sector_size);

    uint32_t sectors_done = 0;
    uint8_t *dst = (uint8_t *)buf;
    while (sectors_done < count) {
        uint32_t sectors_todo = count - sectors_done;
        if (sectors_todo > 65535) {
            sectors_todo = 65535;  // Limit per command.
        }
        // kprintf("AHCI: Reading sectors: current LBA=%llu, sectors_todo=%u\n",
                // (unsigned long long)(lba + sectors_done), sectors_todo);
        int rc = ahci_port_issue_rw_cmd(port, /*write=*/false, lba + sectors_done, sectors_todo, dst);
        if (rc != 0) {
            kprintf("AHCI: read error at LBA=%llu, rc=%d\n", (unsigned long long)(lba + sectors_done), rc);
            return -1;
        }
        sectors_done += sectors_todo;
        dst += (sectors_todo * port->sector_size);
    }
    // kprintf("AHCI: port_read_sectors: Completed reading %u sectors\n", count);
    return 0;
}

/**
 * Writes `count` sectors to `port` at LBA `lba` from `buf`.
 */
int ahci_port_write_sectors(ahci_port_t *port, uint64_t lba, uint32_t count, const void *buf)
{
    kprintf("AHCI: port write: LBA=%llu count=%u\n",(unsigned long long)lba, count);

    uint32_t sectors_done = 0;
    const uint8_t *src = (const uint8_t *)buf;
    while (sectors_done < count) {
        uint32_t sectors_todo = count - sectors_done;
        if (sectors_todo > 65535) {
            sectors_todo = 65535;
        }
        int rc = ahci_port_issue_rw_cmd(port,
                                        /*write=*/true,
                                        lba + sectors_done,
                                        sectors_todo,
                                        (void*)src);
        if (rc != 0) {
            kprintf("AHCI: write error at LBA=%llu\n",(unsigned long long)(lba + sectors_done));
            return -1;
        }
        sectors_done += sectors_todo;
        src += (sectors_todo * port->sector_size);
    }
    return 0;
}

/*
 * Called by the ISR to notify a completed command. 
 * If you do synchronous polling, there's often nothing to do. 
 * If asynchronous, you might signal a waiting thread.
 */
void ahci_port_complete_command(ahci_port_t *port, int slot) {
    // No-op for synchronous approach
}

/*
 * Called by the ISR if TFES (task file error) is set. 
 * We do a basic port reset to recover.
 */
void ahci_port_error_recovery(ahci_port_t *port) {
    volatile hba_port_t *pr = port->regs;
    ahci_stop_port(pr);
    // COMRESET: set DET=1
    pr->sctl |= 0x1;
    // sleep(1); 
    for (int i = 0; i < 1000000; i++);
    pr->sctl &= ~0x1;
    // sleep(1);
    for (int i = 0; i < 1000000; i++);
    pr->serr = 0xFFFFFFFF; 
    ahci_start_port(pr);
    pr->is = 0xFFFFFFFF;
    kprintf("AHCI: Port error recovery done.\n");
}
