#include <storage/ahci/ahci.h>
#include <descriptor_tables/isr.h>   // for register_interrupt_handler signature
#include <mm/vmem.h>                  // for vmap_phys, vmalloc, vfree, etc.
#include <spinlock.h>               // for spinlock_init, spinlock_lock/unlock
#include <printf.h>                 // kprintf
#include <string.h>                 // for memset
#include <mm/pmm.h>

#define MAX_AHCI_CONTROLLERS 8
static ahci_controller_t g_ahci_controllers[MAX_AHCI_CONTROLLERS];
static size_t g_ahci_controller_count = 0;

static void ahci_init_controller(ahci_controller_t *ctl);
static void ahci_reset_controller(ahci_controller_t *ctl);
static void pci_enable_bus_mastering(pci_device_t *dev);
static void pci_enable_memory_space(pci_device_t *dev);
static void pci_enable_msi(pci_device_t *dev);

extern void ahci_port_probe(ahci_port_t *port, uint32_t port_index);
extern void ahci_port_error_recovery(ahci_port_t *port);
extern void ahci_port_complete_command(ahci_port_t *port, int slot);

/*
 * The main entry point - typically called after pci_init() is done.
 */
void ahci_init(void) {
    kprintf("AHCI: Starting driver initialization...\n");
    ahci_discover_controllers();
    kprintf("AHCI: Discovered %zu controller(s).\n", g_ahci_controller_count);
}

/*
 * Scan the known PCI devices for class=1, subclass=6, prog_if=1 (AHCI).
 * For each found, set up an ahci_controller_t and initialize it.
 */
void ahci_discover_controllers(void) {
    if (g_ahci_controller_count > 0) {
        // Already discovered
        return;
    }

    int dev_count = pci_get_device_count();
    for (int i = 0; i < dev_count; i++) {
        pci_device_t *dev = pci_get_device(i);
        if (!dev) continue;

        // Check if mass-storage / SATA / AHCI
        if (dev->class_code == PCI_CLASS_MASS_STORAGE &&
            dev->subclass == PCI_SUBCLASS_SATA &&
            dev->prog_if == PCI_PROGIF_AHCI) 
        {
            if (g_ahci_controller_count >= MAX_AHCI_CONTROLLERS) {
                kprintf("AHCI: Max controllers exceeded.\n");
                break;
            }

            // Found an AHCI controller
            kprintf("AHCI: Found AHCI controller at %02x:%02x.%x\n",
                    dev->bus, dev->slot, dev->function);

            ahci_controller_t *ctl = &g_ahci_controllers[g_ahci_controller_count++];
            memset(ctl, 0, sizeof(*ctl));  // ensure clean

            ctl->pci_dev = dev;
            ctl->abar    = NULL;
            ctl->ports   = NULL;
            ctl->port_count = 0;
            ctl->irq     = dev->irq; // from PCI

            // Initialize the controller
            ahci_init_controller(ctl);
        }
    }
}

/*
 * We might need a trampoline if your 'register_interrupt_handler' doesn't allow a context pointer.
 * For example, if register_interrupt_handler's function signature is void (*handler)(uint64_t, uint32_t),
 * there's no direct place for 'context'. We can store a global that maps IRQ->controller pointer,
 * or a static array if we only support up to MAX_AHCI_CONTROLLERS, etc.
 *
 * This is a naive approach storing only one controller, if you have multiple, do a lookup by 'num'.
 */
static ahci_controller_t *g_current_ahci_ctl = NULL;

/* We'll register this as the actual IRQ handler. */
static void ahci_irq_handler_wrapper(uint64_t int_num, uint32_t err_code) {
    // The function we want to call is ahci_irq_handler(void *context).
    // We'll pass g_current_ahci_ctl if that's the one for this IRQ.
    (void)err_code; // unused
    if (g_current_ahci_ctl) {
        ahci_irq_handler(g_current_ahci_ctl);
    }
}

/*
 * Initialize a specific AHCI controller: map BAR, reset HBA, discover ports, etc.
 */
static void ahci_init_controller(ahci_controller_t *ctl) {
    pci_device_t *dev = ctl->pci_dev;

    // Enable bus mastering + memory access
    pci_enable_bus_mastering(dev);
    pci_enable_memory_space(dev);

    // Optionally enable MSI
    pci_enable_msi(dev);

    // ABAR is usually in BAR5, mask out lower bits
    uint64_t abar_phys = dev->bar[5] & ~0xFULL;
    if (!abar_phys) {
        kprintf("AHCI: No valid ABAR in BAR5, skipping.\n");
        return;
    }

    // Determine size from the PCI resource
    uint64_t abar_size = pci_resource_len(dev, 5);
    if (abar_size < 0x1000) {
        abar_size = 0x1000; // fallback minimum
    }

    // Map ABAR using your vmap_phys signature:
    //  uintptr_t vmap_phys(uintptr_t phys_addr, uintptr_t hint, size_t size,
    //                      uint32_t vm_flags, const char *name);
    // We'll pass 0 as 'hint' and some VM flags. Suppose your kernel has VM_FLAGS_KERNEL_RW?
    // Example:
    uint32_t vm_flags = VM_RDWR; // or whatever fits your design
    uintptr_t abar_virt = vmap_phys((uintptr_t)abar_phys,
                                    0,         // no particular hint
                                    (size_t)abar_size,
                                    vm_flags,
                                    "ahci_abar");
    if (!abar_virt) {
        kprintf("AHCI: Failed to map ABAR.\n");
        return;
    }

    volatile hba_mem_t *abar_map = (volatile hba_mem_t *)abar_virt;
    ctl->abar = abar_map;

    // Reset the controller
    ahci_reset_controller(ctl);

    // # of ports from CAP.NP
    uint32_t cap = abar_map->cap;
    uint32_t ports_supported = (cap & 0x1F) + 1;
    ctl->port_count = ports_supported;

    // Allocate array for ahci_port_t
    // We'll use vmalloc() with some flags:
    size_t ports_array_size = sizeof(ahci_port_t) * ports_supported;
    void *ptr = kmalloc(ports_array_size); // vmalloc(ports_array_size, vm_flags);
    if (!ptr) {
        kprintf("AHCI: Out of memory for ports.\n");
        return;
    }
    memset(ptr, 0, ports_array_size);
    ctl->ports = (ahci_port_t *)ptr;

    // Register an interrupt handler
    // your signature is register_interrupt_handler(unsigned int num, void (*handler)(uint64_t, uint32_t))
    // but we have ahci_irq_handler(void *context). So we need a small trampoline or modify the signature:
    // We'll define a local wrapper below if we must.
    register_interrupt_handler(ctl->irq, ahci_irq_handler_wrapper);

    g_current_ahci_ctl = ctl;

    // Actually store the pointer so the wrapper can find 'ctl'
    // We'll use a global or static array for context if we can't pass it directly.
    // For now let's assume we can pass an argument. If not, you do something else.

    // Probe each implemented port
    uint32_t impl = abar_map->pi;
    for (uint32_t p = 0; p < ports_supported; p++) {
        if (!(impl & (1u << p))) {
            continue; // port not implemented
        }
        ahci_port_t *port = &ctl->ports[p];
        port->hba  = ctl;
        port->regs = &abar_map->ports[p];
        spinlock_init(&port->lock);

        // The port-level initialization is done by ahci_port_probe
        ahci_port_probe(port, p);
    }
}



/*
 * Global HBA reset + enable AHCI mode
 */
static void ahci_reset_controller(ahci_controller_t *ctl) {
    volatile hba_mem_t *abar = ctl->abar;
    // BIOS-OS handoff if needed
    if (abar->cap2 & 1) {
        // set OOS
        abar->bohc |= (1 << 1);
        int timeout = 1000;
        while ((abar->bohc & 1) && timeout--) {
            // Instead of sleep(1), do a small busy wait or your kernel's internal usleep
            for (volatile int i = 0; i < 1000000; i++) {
                // no-op
            }
        }
        if (abar->bohc & 1) {
            kprintf("AHCI: BIOS handoff timed out.\n");
        } else {
            kprintf("AHCI: BIOS handoff OK.\n");
        }
    }
    // Reset
    abar->ghc |= HBA_GHC_HR;
    while (abar->ghc & HBA_GHC_HR) {
        // wait
    }
    // Enable AHCI + interrupts
    abar->ghc |= HBA_GHC_AE;
    abar->ghc |= HBA_GHC_IE;
}

/*
 * The AHCI global interrupt handler (PCI line or MSI).
 * We'll keep the same signature from older code, but we call it from a wrapper that your
 * register_interrupt_handler expects.
 */
void ahci_irq_handler(void *context) {
    ahci_controller_t *ctl = (ahci_controller_t*) context;
    if (!ctl || !ctl->abar) return;

    volatile hba_mem_t *abar = ctl->abar;
    uint32_t is = abar->is;
    if (!is) return; // spurious
    abar->is = is;   // clear global

    // For each port signaled
    for (uint32_t p = 0; p < ctl->port_count; p++) {
        if (!(is & (1u << p))) {
            continue;
        }
        ahci_port_t *port = &ctl->ports[p];
        volatile hba_port_t *pr = port->regs;

        uint32_t pis = pr->is;
        pr->is = pis; // clear port-level
        if (pis & HBA_PxIS_TFES) {
            kprintf("AHCI: port %u TFES error.\n", p);
            ahci_port_error_recovery(port);
            continue;
        }
        // Check completed commands
        uint32_t done = port->active_slots & ~pr->ci;
        while (done) {
            int slot = __builtin_ctz(done);
            done &= ~(1u << slot);

            spinlock_lock(&port->lock);
            port->active_slots &= ~(1u << slot);
            spinlock_unlock(&port->lock);

            ahci_port_complete_command(port, slot);
        }
    }
}

/*
 * Minimal stubs for enabling PCI bus mastering, memory, MSI
 */
static void pci_enable_bus_mastering(pci_device_t *dev) {
    uint32_t cmd = pci_config_read(dev->bus, dev->slot, dev->function, 0x04);
    cmd |= 0x4; // bus mastering
    pci_config_write(dev->bus, dev->slot, dev->function, 0x04, cmd);
}

static void pci_enable_memory_space(pci_device_t *dev) {
    uint32_t cmd = pci_config_read(dev->bus, dev->slot, dev->function, 0x04);
    cmd |= 0x2; // memory space
    pci_config_write(dev->bus, dev->slot, dev->function, 0x04, cmd);
}

static void pci_enable_msi(pci_device_t *dev) {
    int cap_ptr = pci_find_capability(dev->bus, dev->slot, dev->function, 0x05);
    if (cap_ptr) {
        kprintf("AHCI: Enabling minimal MSI for device %02x:%02x.%x\n", dev->bus, dev->slot, dev->function);
        // Real code would set up message address/data, etc. This is a stub.
    } else {
        // Not MSI-capable or no entry
    }
}

/*
 * The block-layer read/write calls remain the same. They rely on
 * bdev->dev.ahci.port => port pointer, then call your port-level
 * sector read/write. We assume you included "block.h" or had them here:
 */
int ahci_read(block_device_t *bdev, uint64_t lba, uint32_t count, void *buffer) {
    struct ahci_port *port = bdev->dev.ahci.port;
    return ahci_port_read_sectors(port, lba, count, buffer);
}

int ahci_write(block_device_t *bdev, uint64_t lba, uint32_t count, const void *buffer) {
    struct ahci_port *port = bdev->dev.ahci.port;
    return ahci_port_write_sectors(port, lba, count, buffer);
}
