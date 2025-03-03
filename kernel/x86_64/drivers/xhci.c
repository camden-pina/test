#include <drivers/xhci.h>
#include <drivers/usb.h>
#include <mm/pmm.h>
#include <string.h>
#include <printf.h>
#include <panic.h>
#include <interrupts/ioapic.h>
#include <descriptor_tables/idt.h>
#include <interrupts/lapic.h>
#include <stdbool.h>
#include <drivers/pci.h>
#include <mm/pgtable.h>

/*
 * We store ephemeral state for command completion, transfer completion, etc.
 * In a more advanced driver, you'd have a queue or multiple concurrency contexts.
 */
static volatile bool awaiting_cmd = false;
static volatile uint8_t last_cmd_type = 0;
static volatile uint8_t last_cmd_comp_code = 0;
static volatile uint8_t last_cmd_slot = 0;

static volatile bool awaiting_transfer = false;
static volatile uint8_t pending_slot = 0;
static volatile uint8_t pending_ep = 0;
static volatile uint8_t transfer_comp_code = 0;
static volatile uint32_t transfer_remain = 0;
static volatile uint32_t pending_transfer_length = 0;

static void xhci_interrupt_handler_main(uint64_t vector, uint32_t error);
static int xhci_issue_command(usb_host_controller_t *hc, xhci_trb_t *cmd_trb);

/*
 * Initialize xHCI controller
 */
int init_xhci_controller(pci_device_t *pci_dev) {
    kprintf("xHCI: Initializing controller at bus %u slot %u func %u\n",
            pci_dev->bus, pci_dev->slot, pci_dev->function);

    // **Find the first valid MMIO BAR**
    uintptr_t base_phys = 0;
    bool is_64bit = false;

       // Look for the first valid memory BAR using the full 64-bit value.
       for (int i = 0; i < 6; i++) {
        uint64_t bar_val = pci_dev->bar[i];
        kprintf("BARID[0x%016lx]\n", bar_val);

        // Check if BAR is valid (note: 64-bit BARs will have a nonzero upper 32 bits even if lower 32 bits are 0).
        if (bar_val != 0 && bar_val != 0xFFFFFFFFFFFFFFFFULL) {
            base_phys = bar_val;
            break;
        }
    }

    if (!base_phys) {
        kprintf("xHCI: No valid memory BAR found in [BAR0..5]\n");
        return -1;
    } 

    // Ensure Bus Mastering & MMIO is enabled
    uint16_t cmd = pci_read16(pci_dev->bus, pci_dev->slot, pci_dev->function, 0x04);
    cmd |= (1 << 2) | (1 << 1); // Enable Bus Mastering & Memory Space
    pci_write16(pci_dev->bus, pci_dev->slot, pci_dev->function, 0x04, cmd);

    uint16_t verify_cmd = pci_read16(pci_dev->bus, pci_dev->slot, pci_dev->function, 0x04);
    if ((verify_cmd & 0x6) != 0x6) {
        kprintf("xHCI: PCI command register update failed! (0x%04x)\n", verify_cmd);
        return -1;
    }

    // Map MMIO region: Compute virtual address mapping.
    uintptr_t virt_addr = 0xFFFFFF8001800000 + (base_phys & 0xFFFFFFF);
    early_map_entries(virt_addr, base_phys, 1, VM_WRITE | VM_NOCACHE);
    base_phys = virt_addr;

    kprintf("xHCI: MMIO base mapped to virtual address: 0x%016lx\n", base_phys);

    // **Ensure MMIO is accessible**
    volatile uint8_t *cap_regs = (volatile uint8_t *)base_phys;
    volatile uint32_t *cap_test = (volatile uint32_t *)base_phys;

    // **TEST READ: Ensure MMIO is accessible**
    uint32_t test_val = cap_test[0];
    uint32_t hcsp_params1 = *(volatile uint32_t *)(cap_regs + 0x04);

    if (test_val == 0xFFFFFFFF || test_val == 0x0 || hcsp_params1 == 0xFFFFFFFF) {
        kprintf("xHCI: MMIO read failure! Test read returned 0x%08x, HCSPARAMS1 = 0x%08x\n", 
                test_val, hcsp_params1);
        return -1;
    }

    uint8_t cap_length = cap_regs[0]; // CAPLENGTH
    if (cap_length < 0x20) {
        kprintf("xHCI: Invalid CAPLENGTH (0x%x), HCSPARAMS1=0x%08x\n", cap_length, hcsp_params1);
        return -1;
    }

    // **Operational Registers Mapping**
    usb_host_controller_t *hc = &usb_controllers[usb_hc_count];
    memset(hc, 0, sizeof(*hc));
    hc->type    = USB_HC_XHCI;
    hc->pci_dev = pci_dev;

    // Operational registers
    hc->op_base = (volatile uint32_t *)(cap_regs + cap_length);

    // **Fill in xHCI-specific structure**
    xhci_state_t *x = &hc->x.xhci;

    // Read dboff, rtsoff from capability registers
    uint32_t dboff  = *(volatile uint32_t *)(cap_regs + 0x14);
    uint32_t rtsoff = *(volatile uint32_t *)(cap_regs + 0x18);
    dboff  &= ~0x3;
    rtsoff &= ~0x3;

    x->db_regs  = (volatile uint32_t *)(cap_regs + dboff);
    x->run_regs = (volatile uint32_t *)(cap_regs + rtsoff);

    // **Halt & Reset**
    hc->op_base[1] &= ~0x1; // Clear Run/Stop
    while (!(hc->op_base[2] & 0x1)) { /* wait for HCHalted=1 */ }

    // Reset
    hc->op_base[1] |= (1 << 1);
    while (hc->op_base[1] & (1 << 1)) { /* wait */ }

    // **Initialize Device Context Base Address Array (DCBAA)**
    void *dcbaa_page = pmm_alloc();
    if (!dcbaa_page) {
        PANIC("xHCI: DCBAA allocation failed");
    }
    memset(dcbaa_page, 0, 4096);
    x->dcbaa = (uint64_t*)dcbaa_page;
    uint64_t dcbaa_phys = (uint64_t)(uintptr_t)dcbaa_page;
    hc->op_base[0x30/4] = (uint32_t)dcbaa_phys;
    hc->op_base[0x34/4] = (uint32_t)(dcbaa_phys >> 32);

    // **Set Max Slots**
    uint32_t hcsparams1 = *(volatile uint32_t *)(cap_regs + 0x04);
    uint8_t max_slots = (uint8_t)(hcsparams1 & 0xFF);
    hc->op_base[0x38/4] = max_slots;
    x->max_slots = max_slots;

    // **Enable Controller**
    hc->op_base[1] |= 0x1;

    usb_hc_count++;
    kprintf("xHCI: Controller initialized successfully (maxSlots=%d)\n", max_slots);
    return 0;
}

static void xhci_ring_doorbell(usb_host_controller_t *hc, uint8_t dbIndex, uint8_t value) {
    xhci_state_t *x = &hc->x.xhci;
    x->db_regs[dbIndex] = value;
}

static int xhci_issue_command(usb_host_controller_t *hc, xhci_trb_t *cmd_trb) {
    xhci_state_t *x = &hc->x.xhci;
    uint16_t idx = x->cmd_ring_index;
    x->cmd_ring[idx] = *cmd_trb;
    // Set cycle bit
    x->cmd_ring[idx].field[3] |= x->cmd_cycle & 1;

    idx = (idx + 1) % XHCI_CMD_RING_SIZE;
    if (idx == 0) {
        x->cmd_cycle ^= 1;
    }
    x->cmd_ring_index = idx;

    // Ring doorbell 0 (command)
    xhci_ring_doorbell(hc, 0, 0);

    // Wait
    awaiting_cmd    = true;
    last_cmd_type   = (cmd_trb->field[3] >> 10) & 0x3F;
    last_cmd_comp_code = 0;

    extern uint32_t get_time_ms(void);
    extern void yield(void);

    uint32_t start = get_time_ms();
    while (awaiting_cmd) {
        if (get_time_ms() - start > 1000) {
            kprintf("xHCI: Command 0x%x timed out.\n", last_cmd_type);
            awaiting_cmd = false;
            return -1;
        }
        yield();
    }
    if (last_cmd_comp_code != XHCI_COMP_SUCCESS) {
        kprintf("xHCI: Command 0x%x failed, compcode=%d\n", last_cmd_type, last_cmd_comp_code);
        return -1;
    }
    if (last_cmd_type == TRB_TYPE_ENABLE_SLOT) {
        return last_cmd_slot;
    }
    return 0;
}

/*
 * xHCI ISR
 */
static void xhci_interrupt_handler_main(uint64_t vector, uint32_t error) {
    // Find the xHCI controller
    usb_host_controller_t *hc = NULL;
    for (int i = 0; i < usb_hc_count; i++) {
        if (usb_controllers[i].type == USB_HC_XHCI) {
            hc = &usb_controllers[i];
            break;
        }
    }
    if (!hc) {
        apic_send_eoi_if_necessary(vector);
        return;
    }

    xhci_state_t *x = &hc->x.xhci;

    while (true) {
        xhci_trb_t *evt = &x->event_ring[x->evt_ring_index];
        uint8_t cycle = evt->field[3] & 0x1;
        if (cycle != x->evt_cycle) {
            // no more new events
            break;
        }
        uint8_t evt_type = (evt->field[3] >> 10) & 0x3F;
        uint8_t comp_code = (evt->field[2] >> 24) & 0xFF;
        uint32_t remainder = evt->field[2] & 0xFFFFFF;
        uint8_t slot_id = (evt->field[3] >> 24) & 0xFF;
        uint8_t ep_id   = (evt->field[3] >> 16) & 0x1F;

        if (evt_type == TRB_TYPE_CMD_COMPLETION) {
            last_cmd_comp_code = comp_code;
            if (last_cmd_type == TRB_TYPE_ENABLE_SLOT && comp_code == XHCI_COMP_SUCCESS) {
                last_cmd_slot = slot_id;
            }
            awaiting_cmd = false;
        } else if (evt_type == TRB_TYPE_TRANSFER_EVENT) {
            transfer_comp_code = comp_code;
            transfer_remain    = remainder;
            if (awaiting_transfer && slot_id == pending_slot && ep_id == pending_ep) {
                awaiting_transfer = false;
            }
        } else if (evt_type == TRB_TYPE_PORT_STATUS_CHANGE) {
            // handle new device connect
            uint8_t port_id = evt->field[0] & 0xFF;
            kprintf("xHCI: Port %u status change\n", port_id);
            // We'll do a port reset + enable slot, etc. For brevity, skipping here
            // Could call usb_process_device_connect(...).
        }

        // Advance ring index
        x->evt_ring_index = (x->evt_ring_index + 1) % XHCI_EVENT_RING_SIZE;
        if (x->evt_ring_index == 0) {
            x->evt_cycle ^= 1;
        }
        // Update ERDP
        uint64_t erdp = (uint64_t)(uintptr_t)&x->event_ring[x->evt_ring_index];
        x->run_regs[0x18/4] = (uint32_t)erdp;
        x->run_regs[0x1C/4] = (uint32_t)(erdp >> 32);
    }

    apic_send_eoi_if_necessary(vector);
}

/*
 * Control transfer
 */
int xhci_control_transfer(usb_host_controller_t *hc, uint8_t dev_addr,
                          usb_setup_packet_t *setup, void *buffer, int length) {
    xhci_state_t *x = &hc->x.xhci;

    xhci_trb_t setup_trb;
    memset(&setup_trb, 0, sizeof(setup_trb));
    // copy the 8-byte setup packet
    memcpy(&setup_trb.field[0], setup, 8);

    bool has_data = (setup->wLength != 0);
    bool in_dir   = (setup->bmRequestType & 0x80) != 0;
    uint8_t trt   = 0;
    if (has_data) {
        trt = in_dir ? 2 : 1;  // 2=IN,1=OUT
    }
    setup_trb.field[2] = 8; // length of setup stage
    setup_trb.field[3] = (TRB_TYPE_SETUP_STAGE << 10) | (1 << 6) | (trt << 16);

    // We'll reuse the command ring for simplicity
    if (xhci_issue_command(hc, &setup_trb)) {
        return -1;
    }

    if (has_data) {
        xhci_trb_t data_trb;
        memset(&data_trb, 0, sizeof(data_trb));
        uint64_t buf_addr = (uint64_t)(uintptr_t)buffer;
        data_trb.field[0] = (uint32_t)buf_addr;
        data_trb.field[1] = (uint32_t)(buf_addr >> 32);
        data_trb.field[2] = length;
        uint8_t dir_bit = in_dir ? 1 : 0;
        data_trb.field[3] = (TRB_TYPE_DATA_STAGE << 10) | (dir_bit << 16);
        if (xhci_issue_command(hc, &data_trb)) {
            return -1;
        }
    }

    xhci_trb_t status_trb;
    memset(&status_trb, 0, sizeof(status_trb));
    // direction is opposite of data stage
    uint8_t status_dir = (has_data && in_dir) ? 0 : 1;
    status_trb.field[3] = (TRB_TYPE_STATUS_STAGE << 10) | (status_dir << 16) | (1 << 5);

    // Mark transfer pending
    uint8_t slot = (dev_addr == 0) ? x->slot_for_address[0] : x->slot_for_address[dev_addr];
    pending_slot   = slot;
    pending_ep     = 0; // EP0
    awaiting_transfer = true;
    pending_transfer_length = length;

    if (xhci_issue_command(hc, &status_trb)) {
        awaiting_transfer = false;
        return -1;
    }

    extern uint32_t get_time_ms(void);
    extern void yield(void);
    uint32_t start = get_time_ms();
    while (awaiting_transfer) {
        if (get_time_ms() - start > 1000) {
            kprintf("xHCI: Control transfer timed out\n");
            awaiting_transfer = false;
            return -1;
        }
        yield();
    }
    if (transfer_comp_code != XHCI_COMP_SUCCESS && transfer_comp_code != XHCI_COMP_SHORT_PACKET) {
        kprintf("xHCI: Control transfer error, code=%d\n", transfer_comp_code);
        return -1;
    }
    return 0;
}

/*
 * Bulk, interrupt, iso transfers can be implemented similarly.
 * For now, we stub them or do partial implementations.
 */

int xhci_bulk_transfer(usb_host_controller_t *hc, uint8_t dev_addr,
                       uint8_t endpoint, void *buffer, int length) {
    kprintf("xHCI: Bulk transfer not fully implemented\n");
    return -1;
}

int xhci_interrupt_transfer(usb_host_controller_t *hc, uint8_t dev_addr,
                            uint8_t endpoint, void *buffer, int length) {
    kprintf("xHCI: Interrupt transfer not fully implemented\n");
    return -1;
}

int xhci_iso_transfer(usb_host_controller_t *hc, uint8_t dev_addr,
                      uint8_t endpoint, void *buffer, int length, uint32_t frame) {
    kprintf("xHCI: Iso transfer not implemented\n");
    return -1;
}
