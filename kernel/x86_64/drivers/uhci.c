#include <drivers/uhci.h>
#include <drivers/uhci_td.h>
#include <printf.h>
#include <panic.h>
#include <io.h>
#include <mm/pmm.h>
#include <string.h>
#include <interrupts/ioapic.h>

/* Assume these kernel functions are provided */
extern uint32_t get_time_ms(void);  // Returns current time in milliseconds
extern void yield(void);            // Yields CPU so that interrupts can run

#define UHCI_TRANSFER_TIMEOUT_MS 1000

/* For our simplified UHCI TD token:
 * - Bits [16:30] encode the total bytes to transfer.
 * - Bit 31 is the Active bit.
 */
#define UHCI_TD_ACTIVE   (1u << 31)

/*
 * Register UHCI interrupt by checking the IRQ reported by the device
 * and mapping it to an IDT vector using the I/O APIC.
 */
void uhci_register_interrupt(pci_device_t *pci_dev) {
    static int uhci_irq_registered = 0;
    if (uhci_irq_registered) {
        kprintf("UHCI: IRQ already registered, skipping registration.\n");
        return;
    }

    // Retrieve IRQ from the PCI device structure.
    uint8_t irq = pci_dev->irq;
    
    // Validate the IRQ. Some systems might report 0xFF or 0 for an unconfigured IRQ.
    if (irq == 0xFF || irq == 0) {
        kprintf("UHCI: Invalid IRQ (%d) reported; defaulting to IRQ 11\n", irq);
        irq = 11;
    }
    
    // For legacy UHCI, IRQ 11 is common, but if the hardware reports a different IRQ, log it.
    if (irq != 11) {
        kprintf("UHCI: Expected IRQ 11 but device reports IRQ %d; using IRQ %d\n", irq, irq);
    } else {
        kprintf("UHCI: Using default IRQ %d\n", irq);
    }
    
    // Map the IRQ to an IDT vector. Here, we choose vector 0x50.
    ioapic_map_irq(irq, 0x50);
    kprintf("UHCI: Mapped IRQ %d to IDT vector 0x50\n", irq);

    uhci_irq_registered = 1;
}

/*
 * Wait for a UHCI TD to complete using interrupt signaling.
 *
 * This function waits until the TD's 'done' flag is set (by the interrupt handler)
 * or until a timeout occurs. It returns 0 on success or -1 on timeout.
 */
static int wait_for_td_completion_interrupt(uhci_td *td) {
    uint32_t start = get_time_ms();
    while (!td->done) {
        if ((get_time_ms() - start) > UHCI_TRANSFER_TIMEOUT_MS) {
            kprintf("UHCI: TD timeout, token = 0x%08x\n", td->token);
            return -1;
        }
        yield();
    }
    return 0;
}

/*
 * UHCI Interrupt Handler (to be registered for the UHCI controller).
 *
 * This handler should be invoked when a UHCI interrupt occurs.
 * It scans the TD chain from the controller's QH and marks any TD that has
 * completed (i.e. whose Active bit is cleared) by setting td->done = 1.
 *
 * NOTE: In a production driver, this handler would be part of a larger interrupt
 *       routine that services multiple TD chains. For simplicity, we assume here
 *       that the control QH's element pointer gives us the head of the TD chain.
 */
void uhci_interrupt_handler(usb_host_controller_t *hc) {
    uhci_td *td = (uhci_td *)(uintptr_t)(hc->uhci.control_qh->element);
    while (td && td->link != 0x00000001) {
        if (!(td->token & UHCI_TD_ACTIVE)) {
            td->done = 1;
        }
        td = (uhci_td *)(uintptr_t)(td->link);
    }
}

/*
 * UHCI interrupt handler main function.
 * This function is called from the assembly ISR stub (uhci_isr_common_stub)
 * and should process UHCI interrupts.
 */
void uhci_interrupt_handler_main(uint64_t vector, uint32_t error) {
    kprintf("UHCI interrupt received: vector = %llu, error = 0x%08x\n", vector, error);
    
    // For each UHCI controller, call its handler.
    extern usb_host_controller_t uhci_controllers[];
    extern int num_uhci_controllers;
    for (int i = 0; i < num_uhci_controllers; i++) {
        uhci_interrupt_handler(&uhci_controllers[i]);
    }
    
    // Signal end-of-interrupt to the local APIC, if needed.
    // For example: lapic_write(APIC_EOI, 0);
}

/*
 * UHCI Control Transfer Implementation (Interrupt-Driven)
 *
 * This function performs a control transfer over a UHCI controller using three TDs:
 *   - Setup stage (8 bytes)
 *   - Data stage (if any)
 *   - Status stage (zero-length)
 *
 * The TDs are linked together and inserted into the controller's schedule via the
 * control Queue Head (assumed to be pre-allocated in hc->uhci.control_qh).
 *
 * The function then waits (using interrupts) for each TD to complete.
 */
int uhci_control_transfer(usb_host_controller_t *hc, uint8_t device_address,
                          usb_setup_packet_t *setup, void *buffer, int length)
{
    // Allocate TDs.
    uhci_td *setup_td  = kmalloca(sizeof(uhci_td), 16);
    uhci_td *data_td   = NULL;
    uhci_td *status_td = kmalloca(sizeof(uhci_td), 16);
    memset(setup_td, 0, sizeof(uhci_td));
    memset(status_td, 0, sizeof(uhci_td));

    // Initialize the 'done' flag.
    setup_td->done = 0;
    status_td->done = 0;

    // Allocate buffer for Setup stage (8 bytes) and copy the setup packet.
    void *setup_buffer = kmalloca(8, 4);
    memcpy(setup_buffer, setup, sizeof(usb_setup_packet_t));

    // Fill in the Setup TD (for control transfers, SETUP PID would be used).
    setup_td->token = ((8 & 0x7FFF) << 16) | UHCI_TD_ACTIVE;
    setup_td->link = 0x00000001;  // Terminate pointer.
    setup_td->buffer = (uint32_t)(uintptr_t)setup_buffer;

    // Data TD (if length > 0).
    if (length > 0) {
        data_td = kmalloca(sizeof(uhci_td), 16);
        memset(data_td, 0, sizeof(uhci_td));
        data_td->done = 0;
        data_td->token = ((length & 0x7FFF) << 16) | UHCI_TD_ACTIVE;
        data_td->link = 0x00000001;
        data_td->buffer = (uint32_t)(uintptr_t)buffer;
    }

    // Fill in the Status TD (zero-length transfer).
    status_td->token = (0 << 16) | UHCI_TD_ACTIVE;
    status_td->link = 0x00000001;
    status_td->buffer = 0;

    // Link the TDs: Setup -> (Data if present) -> Status.
    if (data_td) {
        setup_td->link = (uint32_t)(uintptr_t)data_td;
        data_td->link = (uint32_t)(uintptr_t)status_td;
    } else {
        setup_td->link = (uint32_t)(uintptr_t)status_td;
    }

    // Insert the TD chain into the schedule.
    // We assume that hc->uhci.control_qh is dedicated to control transfers.
    // Set its element pointer to our Setup TD.
    hc->uhci.control_qh->element = (uint32_t)(uintptr_t)setup_td;

    int ret = 0;

    // Wait for Setup stage to complete using interrupt-driven wait.
    ret = wait_for_td_completion_interrupt(setup_td);
    if (ret != 0) {
        kprintf("UHCI: Setup stage failed\n");
        goto cleanup;
    }

    // If Data stage exists, wait for its completion.
    if (data_td) {
        ret = wait_for_td_completion_interrupt(data_td);
        if (ret != 0) {
            kprintf("UHCI: Data stage failed\n");
            goto cleanup;
        }
    }

    // Wait for Status stage to complete.
    ret = wait_for_td_completion_interrupt(status_td);
    if (ret != 0) {
        kprintf("UHCI: Status stage failed\n");
        goto cleanup;
    }

cleanup:
    // Remove the TD chain from the schedule.
    hc->uhci.control_qh->element = 0x00000001;  // Terminate pointer.
    // In a production system, free or reuse allocated TDs and buffers appropriately.
    return ret;
}
