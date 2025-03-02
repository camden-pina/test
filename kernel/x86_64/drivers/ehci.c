#include <drivers/ehci.h>
#include <drivers/ehci_qtd.h>
#include <drivers/usb.h>
#include <drivers/pci.h>
#include <printf.h>
#include <panic.h>
#include <io.h>
#include <mm/pmm.h>
#include <string.h>

/* Assume these functions are provided by your kernel */
extern uint32_t get_time_ms(void);
extern void yield(void);

/* Timeout for each transfer stage in milliseconds */
#define EHCI_TRANSFER_TIMEOUT_MS 1000

/*
 * Wait for a qTD to complete.
 *
 * Waits until the Active bit (bit 31 of token) clears or until a timeout occurs.
 * Returns 0 on success; otherwise, returns -1 on timeout or a negative error code if error bits are set.
 */
static int wait_for_qtd_completion(ehci_qtd *qtd) {
    uint32_t start_time = get_time_ms();
    while (qtd->token & (1u << 31)) {
        if ((get_time_ms() - start_time) > EHCI_TRANSFER_TIMEOUT_MS) {
            kprintf("EHCI: qTD timeout\n");
            return -1;
        }
        yield(); // Allow interrupts to be processed
    }
    // Check for error bits in the token (this is simplified; real drivers check specific error bits)
    if (qtd->token & 0xFF) {  // Assume lower byte holds error status (for example purposes)
        kprintf("EHCI: qTD error: token=0x%08x\n", qtd->token);
        return -2;
    }
    return 0;
}

/*
 * EHCI Control Transfer Implementation
 *
 * This function creates three qTDs for the Setup, Data (if any), and Status stages,
 * links them into the asynchronous schedule, waits (with timeout) for each to complete,
 * and then cleans up the schedule.
 *
 * This version uses interrupt-driven completion via yield() and get_time_ms().
 */
int ehci_control_transfer(usb_host_controller_t *hc, uint8_t device_address,
                          usb_setup_packet_t *setup, void *buffer, int length)
{
    /* Allocate qTDs for Setup and Status stages.
       Allocate a Data stage qTD only if length > 0. */
    ehci_qtd *setup_qtd = kmalloca(sizeof(ehci_qtd), 32);
    ehci_qtd *data_qtd = NULL;
    ehci_qtd *status_qtd = kmalloca(sizeof(ehci_qtd), 32);
    memset(setup_qtd, 0, sizeof(ehci_qtd));
    memset(status_qtd, 0, sizeof(ehci_qtd));

    /* Allocate a buffer for the Setup stage (8 bytes) and copy the setup packet. */
    void *setup_buffer = kmalloca(8, 4);
    memcpy(setup_buffer, setup, sizeof(usb_setup_packet_t));

    /* Setup Stage: 8 bytes transfer */
    // Bits 16-30 encode total bytes; bit 31 is Active.
    setup_qtd->token = ((8 & 0x7FFF) << 16) | (1u << 31);
    setup_qtd->next_qtd = 0x00000001;           // Terminate
    setup_qtd->alternate_next_qtd = 0x00000001;   // Terminate alternate
    setup_qtd->buffer[0] = (uint32_t)(uintptr_t)setup_buffer;

    /* Data Stage (if any) */
    if (length > 0) {
        data_qtd = kmalloca(sizeof(ehci_qtd), 32);
        memset(data_qtd, 0, sizeof(ehci_qtd));
        data_qtd->token = ((length & 0x7FFF) << 16) | (1u << 31);
        data_qtd->next_qtd = 0x00000001;
        data_qtd->alternate_next_qtd = 0x00000001;
        data_qtd->buffer[0] = (uint32_t)(uintptr_t)buffer;
    }

    /* Status Stage: Zero-length transfer */
    status_qtd->token = (0 << 16) | (1u << 31);
    status_qtd->next_qtd = 0x00000001;
    status_qtd->alternate_next_qtd = 0x00000001;

    /* Link the qTDs: Setup -> (Data, if any) -> Status */
    if (data_qtd) {
        setup_qtd->next_qtd = (uint32_t)(uintptr_t)data_qtd;
        data_qtd->next_qtd = (uint32_t)(uintptr_t)status_qtd;
    } else {
        setup_qtd->next_qtd = (uint32_t)(uintptr_t)status_qtd;
    }

    /* Insert the chain into the asynchronous schedule.
       For simplicity, we assume the async list pointer is used exclusively for this transfer.
       In a full implementation, you would link this chain into an existing schedule list. */
    volatile uint32_t *asyncListAddr = hc->op_base + (EHCI_ASYNCLISTADDR_OFFSET / 4);
    *asyncListAddr = (uint32_t)(uintptr_t)setup_qtd;

    int ret = 0;

    /* Wait for Setup Stage completion */
    ret = wait_for_qtd_completion(setup_qtd);
    if (ret != 0) {
        kprintf("EHCI: Setup stage failed\n");
        goto cleanup;
    }

    /* If there is a Data Stage, wait for its completion */
    if (data_qtd) {
        ret = wait_for_qtd_completion(data_qtd);
        if (ret != 0) {
            kprintf("EHCI: Data stage failed\n");
            goto cleanup;
        }
    }

    /* Wait for Status Stage completion */
    ret = wait_for_qtd_completion(status_qtd);
    if (ret != 0) {
        kprintf("EHCI: Status stage failed\n");
        goto cleanup;
    }

cleanup:
    /* Remove the transfer chain from the asynchronous schedule */
    *asyncListAddr = 0x00000001;  // Terminate async list

    /* In a full implementation, free or reuse the allocated qTDs and buffers appropriately. */
    return ret;
}

/*
 * EHCI Controller Initialization (Enhanced)
 *
 * This function initializes the EHCI controller by reading the capability registers,
 * resetting the controller, allocating the periodic frame list and an initial asynchronous queue head,
 * setting CONFIGFLAG, and then enumerating each root hub port.
 */
int ehci_init_controller(pci_device_t *pci_dev, usb_host_controller_t *hc) {
    /* Get EHCI MMIO base from BAR0 (using uintptr_t for 64-bit addresses) */
    uintptr_t base = pci_dev->bar[0] & ~0xF;
    if (!base)
        return -1;

    volatile uint8_t *mmio_base = (volatile uint8_t *)base;
    /* Read EHCI capability registers */
    ehci_cap_regs_t cap_regs;
    memcpy(&cap_regs, (void *)mmio_base, sizeof(ehci_cap_regs_t));
    uint8_t caplength = cap_regs.caplength;
    kprintf("EHCI: Capability length = %d, HCIVersion = 0x%04x\n", caplength, cap_regs.hciversion);

    /* Operational registers start at mmio_base + caplength */
    volatile uint32_t *op_base = (volatile uint32_t *)(mmio_base + caplength);
    hc->op_base = op_base;

    /* Reset EHCI controller */
    volatile uint32_t *usbcmd = op_base + (EHCI_USBCMD_OFFSET / 4);
    *usbcmd |= EHCI_USBCMD_RESET;
    for (int i = 0; i < 1000000; i++) {
         if (!(*usbcmd & EHCI_USBCMD_RESET))
             break;
    }

    /* Allocate periodic frame list (4KB aligned) */
    uint32_t *periodic_list = usb_alloc_page();
    volatile uint32_t *periodicListBase = op_base + (EHCI_PERIODICLISTBASE_OFFSET / 4);
    *periodicListBase = (uint32_t)(uintptr_t)periodic_list;
    hc->ehci.periodic_list = periodic_list;

    /* Allocate an initial asynchronous queue head for control transfers */
    ehci_qh *async_head = kmalloca(sizeof(ehci_qh), 32);
    memset(async_head, 0, sizeof(ehci_qh));
    async_head->horizontalLink = 0x00000001;  // Terminate list
    volatile uint32_t *asyncListAddr = op_base + (EHCI_ASYNCLISTADDR_OFFSET / 4);
    *asyncListAddr = (uint32_t)(uintptr_t)async_head;
    hc->ehci.async_head = async_head;

    /* Set CONFIGFLAG (assumed at offset 0x40) to hand over ports to EHCI */
    volatile uint32_t *configFlag = op_base + (0x40 / 4);
    *configFlag = EHCI_CONFIGFLAG_FLAG;

    /* Start the EHCI controller (set RUN bit) */
    *usbcmd |= EHCI_USBCMD_RUN;

    /* Determine number of root hub ports from HCSPARAMS in cap_regs */
    uint32_t num_ports = cap_regs.hcsparams & 0xF;  // Lower nibble holds port count
    hc->num_ports = (uint8_t)num_ports;

    kprintf("EHCI: Initialized controller on bus %d slot %d with %d ports\n",
            pci_dev->bus, pci_dev->slot, num_ports);

    /* Enumerate each port */
    for (uint32_t p = 0; p < num_ports; p++) {
        volatile uint32_t *portsc = op_base + ((EHCI_PORTSC_OFFSET(p)) / 4);
        uint32_t status = *portsc;
        kprintf("EHCI: Port %u status = 0x%08x\n", p + 1, status);
        if (status & EHCI_PORTSC_CONNECT) {
            if (status & EHCI_PORTSC_LOW_SPEED) {
                /* Hand off low-speed devices to companion controller */
                *portsc |= EHCI_PORT_OWNER;
                kprintf("EHCI: Port %u assigned to companion controller (low/full-speed device)\n", p + 1);
            } else {
                /* High-speed device: perform port reset */
                *portsc |= EHCI_PORTSC_RESET_BIT;
                for (volatile int i = 0; i < 100000; i++);
                *portsc &= ~EHCI_PORTSC_RESET_BIT;
                for (volatile int i = 0; i < 50000; i++);
                kprintf("EHCI: Device detected on port %u\n", p + 1);
                /* Enumerate the device using the common enumeration function.
                   This will use our new EHCI control transfer function. */
                usb_process_device_connect(hc, (uint8_t)p, 0);
            }
        }
    }

    return 0;
}
