// xhci.c
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
#include <msi.h>

// Forward declarations for external functions.
extern void yield(void);
extern uint32_t get_time_ms(void);
extern void usb_process_device_connect(usb_host_controller_t *hc, uint8_t port, uint8_t low_speed);
extern void *pmm_alloc(void);

// Forward declarations of internal functions.
void xhci_interrupt_handler_main(uint64_t vector, uint32_t error);
static int  xhci_issue_command(usb_host_controller_t *hc, xhci_trb_t *cmd_trb);
static int  xhci_configure_event_ring(usb_host_controller_t *hc);
static int  xhci_register_interrupt(usb_host_controller_t *hc);

// Global volatile state for command and transfer completions.
static volatile bool     awaiting_cmd            = false;
static volatile uint8_t  last_cmd_type           = 0;
static volatile uint8_t  last_cmd_comp_code      = 0;
static volatile uint8_t  last_cmd_slot           = 0;
static volatile bool     awaiting_transfer       = false;
static volatile uint8_t  pending_slot            = 0;
static volatile uint8_t  pending_ep              = 0;
static volatile uint8_t  transfer_comp_code      = 0;
static volatile uint32_t transfer_remain         = 0;
static volatile uint32_t pending_transfer_length = 0;

#define FIXED_VIRT_BASE      0xFFFFFF8000D00000ULL

/*
 * test_xhci_enable_slot:
 * Issues an ENABLE_SLOT command on the command ring and waits for its completion.
 */
int test_xhci_enable_slot(usb_host_controller_t *hc) {
    kprintf("xHCI Test: Issuing ENABLE_SLOT command...\n");
    kprintf("xHCI Test: cmd_ring pointer = 0x%lx, doorbell pointer = 0x%lx\n",
            (uintptr_t)hc->x.xhci.cmd_ring, (uintptr_t)hc->x.xhci.db_regs);

    // Prepare the TRB for an Enable Slot command.
    xhci_trb_t cmd;
    memset(&cmd, 0, sizeof(cmd));

    // TRB Type = ENABLE_SLOT (9)
    cmd.field[3] = (TRB_TYPE_ENABLE_SLOT << 10);
    cmd.field[3] |= (hc->x.xhci.cmd_cycle & 1);

    // Issue the command on the command ring.
    int ret = xhci_issue_command(hc, &cmd);
    if (ret < 0) {
        kprintf("xHCI Test: ENABLE_SLOT command failed (ret=%d)\n", ret);
        return ret;
    }

    // Success; 'ret' is actually the allocated slot ID.
    kprintf("xHCI Test: ENABLE_SLOT command succeeded. Allocated slot = %d\n", ret);
    return 0;
}

/*
 * xhci_ring_doorbell:
 * Writes a value to the doorbell register and logs the operation.
 */
static void xhci_ring_doorbell(usb_host_controller_t *hc, uint8_t dbIndex, uint8_t value) {
    xhci_state_t *x = &hc->x.xhci;

    volatile uint32_t *op_regs = hc->op_base;
    uint32_t usbsts = op_regs[1];
    if (usbsts & (1 << 0)) {
        kprintf("xhci_ring_doorbell: WARNING: xHCI is halted!\n");
    }
    if (usbsts & (1 << 2)) {
        kprintf("xhci_ring_doorbell: WARNING: Host System Error!\n");
    }

    // Write the doorbell.
    kprintf("xhci_ring_doorbell: Writing value 0x%02x to doorbell[%d]\n", value, dbIndex);
    x->db_regs[dbIndex] = value;

    uint32_t db_val = x->db_regs[dbIndex];
    kprintf("xhci_ring_doorbell: readback = 0x%08x\n", db_val);
}

/*
 * test_xhci_noop:
 * Test by issuing a NOOP command.
 */
int test_xhci_noop(usb_host_controller_t *hc) {
    kprintf("xHCI Test: Issuing NOOP command...\n");
    xhci_trb_t noop;
    memset(&noop, 0, sizeof(noop));
    noop.field[3] = (TRB_TYPE_NOOP << 10) | (hc->x.xhci.cmd_cycle & 1);

    int ret = xhci_issue_command(hc, &noop);
    if (ret < 0) {
        kprintf("xHCI Test: NOOP command failed (ret=%d)\n", ret);
        return ret;
    }
    kprintf("xHCI Test: NOOP command completed successfully.\n");
    return 0;
}

/*
 * xhci_configure_event_ring:
 * Allocates/configures the event ring + ERST.
 */
static int xhci_configure_event_ring(usb_host_controller_t *hc) {
    kprintf("xhci_configure_event_ring: begin\n");
    xhci_state_t *x = &hc->x.xhci;

    // Allocate event ring.
    x->event_ring = (xhci_trb_t *) usb_alloc_page();
    if (!x->event_ring) {
        kprintf("xhci_configure_event_ring: failed to alloc event ring\n");
        return -1;
    }
    memset(x->event_ring, 0, PAGE_SIZE);
    x->evt_ring_index = 0;
    x->evt_cycle = 1;

    // Allocate ERST.
    xhci_erst_entry_t *erst = (xhci_erst_entry_t *) usb_alloc_page();
    if (!erst) {
        kprintf("xhci_configure_event_ring: failed to allocate ERST\n");
        return -1;
    }
    memset(erst, 0, PAGE_SIZE);
    x->erst = erst;
    kprintf("xhci_configure_event_ring: ERST allocated & cleared\n");

    // Convert addresses to physical
    uint64_t ring_phys = virt_to_phys(x->event_ring);
    uint64_t erst_phys = virt_to_phys(erst);
    kprintf("xhci_configure_event_ring: event_ring virt=0x%lx, phys=0x%lx\n",
            (uintptr_t)x->event_ring, ring_phys);
    kprintf("xhci_configure_event_ring: ERST virt=0x%lx, phys=0x%lx\n",
            (uintptr_t)erst, erst_phys);

    // Single ERST entry
    erst[0].seg_addr = ring_phys;
    erst[0].seg_size = XHCI_EVENT_RING_SIZE;
    erst[0].reserved = 0;

    volatile uint32_t *ir_base = x->run_regs;
    if (!ir_base) {
        kprintf("xhci_configure_event_ring: run_regs is NULL\n");
        return -1;
    }

    // ERSTSZ=1
    ir_base[0x20/4 + 0x08/4] = 1;

    // ERSTBA = erst_phys
    ir_base[0x20/4 + 0x10/4] = (uint32_t)erst_phys;
    ir_base[0x20/4 + 0x14/4] = (uint32_t)(erst_phys >> 32);

    // ERDP = ring_phys | EHB
    ir_base[0x20/4 + 0x18/4] = (uint32_t)ring_phys;
    ir_base[0x20/4 + 0x1C/4] = (uint32_t)(ring_phys >> 32);
    ir_base[0x20/4 + 0x1C/4] |= (1 << 3);

    // IMAN|=IE
    uint32_t iman = ir_base[0x20/4 + 0x00/4];
    iman |= (1 << 1);
    ir_base[0x20/4 + 0x00/4] = iman;

    kprintf("xhci_configure_event_ring: done, ring_phys=0x%lx\n", ring_phys);
    return 0;
}

static inline uint16_t bswap_16(uint16_t x) {
    return (x >> 8) | (x << 8);
}

// -----------------------------------------------------------------------------
// init_xhci_controller
// -----------------------------------------------------------------------------

#define XHCI_CMD_RING_SIZE 64
#define TRB_TYPE_LINK      6

int init_xhci_controller(pci_device_t *pci_dev) {
    usb_host_controller_t *hc = &usb_controllers[usb_hc_count];
    memset(hc, 0, sizeof(*hc));
    hc->type    = USB_HC_XHCI;
    hc->pci_dev = pci_dev;

    // 1) Enable PCI device (I/O + Memory + Bus Master).
    uint16_t cmd_reg = pci_read16(pci_dev->bus, pci_dev->slot, pci_dev->function, 0x04);
    cmd_reg |= 0x0007;
    pci_write16(pci_dev->bus, pci_dev->slot, pci_dev->function, 0x04, cmd_reg);

    // 2) Find a valid MMIO BAR
    int bar_index = -1;
    for (int i = 0; i < 6; i++) {
        uint64_t bar_val = pci_dev->bar[i];
        if (bar_val != 0 && bar_val != 0xFFFFFFFFFFFFFFFFULL) {
            bar_index = i;
            break;
        }
    }
    if (bar_index < 0) {
        kprintf("xHCI: No valid BAR found.\n");
        return -1;
    }

    // 3) Map the xHCI registers
    uintptr_t base_phys = pci_resource_start(pci_dev, bar_index);
    size_t mmio_size    = pci_resource_len(pci_dev, bar_index);
    if (!mmio_size) {
        kprintf("xHCI: BAR[%d] size=0?\n", bar_index);
        return -1;
    }
    kprintf("xHCI: Using BAR[%d], phys=0x%lx size=0x%lx\n", bar_index, base_phys, mmio_size);

    size_t num_pages = (mmio_size + PAGE_SIZE - 1) / PAGE_SIZE;
    uintptr_t virt_addr = FIXED_VIRT_BASE;
    early_map_entries(virt_addr, base_phys, num_pages, VM_WRITE | VM_NOCACHE);

    volatile uint32_t *_cap_regs = (volatile uint32_t *) virt_addr;
    uint32_t reg0        = _cap_regs[0];
    uint8_t  cap_length  = (uint8_t)(reg0 & 0xFF);
    uint16_t hci_version = (uint16_t)((reg0 >> 16) & 0xFFFF);

    kprintf("xHCI: CAPLENGTH=0x%x, HCIVERSION=0x%x\n", cap_length, hci_version);
    hc->op_base = (volatile uint32_t *)((uintptr_t)_cap_regs + cap_length);
    volatile uint32_t *op_regs = hc->op_base;

    // Check HCCPARAMS1 for context size requirement.
    // HCCPARAMS1 is at offset 0x10 in the capability registers (index 4).
    uint32_t hccparams1 = *(_cap_regs + 4);
    if (hccparams1 & (1 << 2)) {
        kprintf("xHCI: 64-byte device context required (HCCPARAMS1.CSZ=1). Setting USBCMD.CSZ bit.\n");
        uint32_t usbcmd = op_regs[0];
        usbcmd |= (1 << 23); // Set CSZ bit for 64-byte contexts.
        op_regs[0] = usbcmd;
    } else {
        kprintf("xHCI: 32-byte device context used (HCCPARAMS1.CSZ=0).\n");
    }

    // 4) Runtime & Doorbell
    uint32_t dboff = _cap_regs[0x14/4] & ~0x3;
    hc->x.xhci.db_regs  = (volatile uint32_t *)((uintptr_t)_cap_regs + dboff);

    uint32_t rtsoff = _cap_regs[0x18/4] & ~0x1F;
    hc->x.xhci.run_regs = (volatile uint32_t *)((uintptr_t)_cap_regs + rtsoff);

    kprintf("xHCI: opBase=0x%p, runRegs=0x%p, dbRegs=0x%p\n",
            hc->op_base, hc->x.xhci.run_regs, hc->x.xhci.db_regs);

    // 5) IRQ → either MSI-X or legacy
    hc->irq = pci_dev->irq;
    if (hc->irq == 0xFF) {
        kprintf("xHCI: PCI IRQ=0xFF => fallback to 10\n");
        hc->irq = 10;
    }
    kprintf("xHCI: Using IRQ %u\n", hc->irq);

    // Attempt MSI-X on vector 0x50
    if (pci_enable_msix(pci_dev, 0x50, 1) != 0) {
        kprintf("xHCI: MSI-X failed => using legacy.\n");
        if (hc->irq != 0xFF && hc->irq != 0) {
            ioapic_map_irq(hc->irq, 0x50);
        }
    } else {
        kprintf("xHCI: MSI-X enabled.\n");
    }

    // 6) Allocate command ring – do NOT set CRCR yet
    void *cmd_ring_page = pmm_alloc();
    memset(cmd_ring_page, 0, PAGE_SIZE);
    hc->x.xhci.cmd_ring       = (xhci_trb_t *)cmd_ring_page;
    hc->x.xhci.cmd_ring_index = 0;
    hc->x.xhci.cmd_cycle      = 1;
    uint64_t cmd_ring_phys    = virt_to_phys(cmd_ring_page);

    // Link TRB at ring end
    unsigned last_trb = XHCI_CMD_RING_SIZE - 1;
    xhci_trb_t *link_trb = &hc->x.xhci.cmd_ring[last_trb];
    memset(link_trb, 0, sizeof(*link_trb));
    link_trb->field[0] = (uint32_t)(cmd_ring_phys & 0xFFFFFFFF);
    link_trb->field[1] = (uint32_t)(cmd_ring_phys >> 32);
    link_trb->field[3] = (TRB_TYPE_LINK << 10) | (1 << 1) | (hc->x.xhci.cmd_cycle & 1);

    kprintf("xHCI: CMD ring at %p, Link TRB -> 0x%lx\n", cmd_ring_page, cmd_ring_phys);

    // 7) Configure event ring now (ERST, etc.)
    int ret = xhci_configure_event_ring(hc);
    if (ret) {
        kprintf("xHCI: event ring config fail (ret=%d)\n", ret);
        return ret;
    }
    // Clear pending interrupt bits
    op_regs[1] |= (1 << 3);

    // 8) Register the ISR on vector=0x50
    ret = xhci_register_interrupt(hc);
    if (ret) {
        kprintf("xHCI: register_interrupt fail (ret=%d)\n", ret);
        return ret;
    }

    // 9) Controller reset => RunStop=0, then HCRST=1
    op_regs[0] &= ~0x1; // Clear RunStop
    op_regs[0] |= (1 << 1); // HCRST=1

    uint32_t timeout = 1000000;
    while ((op_regs[0] & (1 << 1)) && timeout--) {
        // spin
    }
    if (!timeout) {
        kprintf("TIMEOUT: waiting for HC Reset=0\n");
        return -1;
    }
    // Wait until HCHalted=1
    timeout = 1000000;
    while (!(op_regs[1] & (1 << 0)) && timeout--) {
        // spin
    }
    if (!timeout) {
        kprintf("TIMEOUT: waiting for HCHalted=1\n");
        return -1;
    }
    kprintf("xHCI: Controller halted after reset.\n");

    // 10) (Re)-write CRCR
    op_regs[0x18/4] = (uint32_t)cmd_ring_phys | 1; // RCS=1
    op_regs[0x1C/4] = (uint32_t)(cmd_ring_phys >> 32);
    kprintf("xHCI: CRCR re-written => 0x%08x\n", op_regs[0x18/4]);

    // 11) Allocate DCBAA first, set DCBAAP
    void *dcbaa_page = pmm_alloc();
    if (!dcbaa_page) {
        PANIC("xHCI: DCBAA allocation failed!");
    }
    memset(dcbaa_page, 0, PAGE_SIZE);
    hc->x.xhci.dcbaa = (uint64_t *)dcbaa_page;
    uint64_t dcbaa_phys = virt_to_phys(dcbaa_page);

    op_regs[0x30/4] = (uint32_t)dcbaa_phys;
    op_regs[0x34/4] = (uint32_t)(dcbaa_phys >> 32);
    kprintf("DCBAA: virt=0x%lx, phys=0x%lx\n",
            (uintptr_t)dcbaa_page, dcbaa_phys);

    // If scratchpads are needed, set DCBAA[0]
    uint32_t hcs_params2 = _cap_regs[2];
    uint8_t spb_max = (hcs_params2 >> 27) & 0x1F;
    if (spb_max) {
        uint32_t num_sp = (1U << spb_max);
        kprintf("xHCI: Need %u scratchpad(s)\n", num_sp);

        uint64_t *sp_array = (uint64_t *)pmm_alloc();
        memset(sp_array, 0, PAGE_SIZE);

        for (uint32_t i = 0; i < num_sp; i++) {
            void *sp_page = pmm_alloc();
            memset(sp_page, 0, PAGE_SIZE);
            uint64_t sp_phys = virt_to_phys(sp_page);
            sp_array[i] = sp_phys;
            kprintf(" Scratchpad[%u] => 0x%lx\n", i, sp_phys);
        }
        hc->x.xhci.dcbaa[0] = virt_to_phys(sp_array);
        kprintf("DCBAA[0] => 0x%lx\n", hc->x.xhci.dcbaa[0]);
    }

    // 12) Read # of MaxSlots from HCSPARAMS1 and set CONFIG
    uint32_t hcsp1 = _cap_regs[1];
    uint8_t max_slots = (hcsp1 & 0xFF);
    op_regs[0x38/4] = max_slots;
    kprintf("xHCI: MaxSlotsEn => %u\n", max_slots);

    // 13) Set PAGESIZE to 4KB
    op_regs[2] = 0x1;
    kprintf("xHCI: PAGESIZE => 0x%x\n", op_regs[2]);

    // 14) Global interrupt enable in USBCMD (bit2)
    op_regs[0] |= (1 << 2);

    // 15) Set RunStop=1, wait for HCHalted=0, then check CNR=0
    op_regs[0] |= 0x1; // RunStop=1
    timeout = 1000000;
    while ((op_regs[1] & 0x1) && timeout--) {
        // wait HCHalted=0
    }
    if (!timeout) {
        kprintf("TIMEOUT: HCHalted didn't clear => USBSTS=0x%08x\n", op_regs[1]);
        return -1;
    }

    // Wait for CNR=0
    uint32_t status;
    timeout = 1000000;
    do {
        status = op_regs[1];
    } while ((status & (1 << 12)) && --timeout);

    if (!timeout) {
        kprintf("TIMEOUT: CNR didn't clear => USBSTS=0x%08x\n", status);
        return -1;
    }

    kprintf("xHCI: Controller running. USBSTS=0x%08x\n", op_regs[1]);

    // 16) Issue a test command: Enable Slot
    test_xhci_enable_slot(hc);

    kprintf("xHCI: Controller initialized (maxSlots=%d)\n", max_slots);
    usb_hc_count++;
    return 0;
}

// -----------------------------------------------------------------------------
// xhci_register_interrupt
// -----------------------------------------------------------------------------

static int xhci_register_interrupt(usb_host_controller_t *hc) {
    kprintf("Registering xHCI interrupt on vector 0x50\n");
    register_interrupt_handler(0x50, xhci_interrupt_handler_main);
    return 0;
}

// Dump the entire command ring for debugging.
static void dump_cmd_ring(usb_host_controller_t *hc) {
    xhci_state_t *x = &hc->x.xhci;
    kprintf("----- Dumping Command Ring (Size = %d) -----\n", XHCI_CMD_RING_SIZE);
    for (int i = 0; i < XHCI_CMD_RING_SIZE; i++) {
        kprintf("cmd_ring[%2d]: field[0]=0x%08x, field[1]=0x%08x, field[2]=0x%08x, field[3]=0x%08x\n",
                i, x->cmd_ring[i].field[0], x->cmd_ring[i].field[1],
                x->cmd_ring[i].field[2], x->cmd_ring[i].field[3]);
    }
    kprintf("----------------------------------------------\n");
}

// Dump the event ring for debugging.
static void dump_event_ring(usb_host_controller_t *hc) {
    xhci_state_t *x = &hc->x.xhci;
    kprintf("----- Dumping Event Ring (Size = %d) -----\n", XHCI_EVENT_RING_SIZE);
    for (int i = 0; i < XHCI_EVENT_RING_SIZE; i++) {
        kprintf("event_ring[%2d]: field[0]=0x%08x, field[1]=0x%08x, field[2]=0x%08x, field[3]=0x%08x\n",
                i, x->event_ring[i].field[0], x->event_ring[i].field[1],
                x->event_ring[i].field[2], x->event_ring[i].field[3]);
    }
    kprintf("---------------------------------------------\n");
}

static void dump_op_regs(volatile uint32_t *op_regs) {
    // Offsets are in bytes. Dump from 0x00 to 0x40.
    kprintf("----- Dumping xHCI Operational Registers -----\n");
    kprintf("USBCMD   (0x00): 0x%08x\n", op_regs[0]);
    kprintf("USBSTS   (0x04): 0x%08x\n", op_regs[1]);
    kprintf("PAGESIZE (0x08): 0x%08x\n", op_regs[2]);
    kprintf("DNCTRL   (0x14): 0x%08x\n", op_regs[5]);  // Doorbell notification, if available.
    kprintf("CRCR Low (0x18): 0x%08x\n", op_regs[0x18/4]);
    kprintf("CRCR High(0x1C): 0x%08x\n", op_regs[0x1C/4]);
    kprintf("-----------------------------------------------\n");
}

// -----------------------------------------------------------------------------
// xhci_issue_command
// -----------------------------------------------------------------------------
static int xhci_issue_command(usb_host_controller_t *hc, xhci_trb_t *cmd_trb) {
    xhci_state_t *x = &hc->x.xhci;
    volatile uint32_t *op_regs = hc->op_base;
    uint32_t usbsts = op_regs[1];

    kprintf("xhci_issue_command: Starting command issuance.\n");
    kprintf("xhci_issue_command: Initial USBSTS = 0x%08x\n", usbsts);
    dump_op_regs(op_regs);

    if (usbsts & (1 << 0)) {
        kprintf("xhci_issue_command: Warning: xHC is halted!\n");
    }
    if (usbsts & (1 << 2)) {
        kprintf("xhci_issue_command: Warning: Host System Error set!\n");
    }

    // Ensure we don't write into the reserved Link TRB slot.
    if (x->cmd_ring_index == XHCI_CMD_RING_SIZE - 1) {
        x->cmd_ring_index = 0;
        x->cmd_cycle ^= 1;
        kprintf("xhci_issue_command: Skipped reserved Link TRB slot; toggled cycle -> %u\n", x->cmd_cycle & 1);
    }

    uint16_t idx = x->cmd_ring_index;
    x->cmd_ring[idx] = *cmd_trb;
    x->cmd_ring[idx].field[3] |= (x->cmd_cycle & 1);

    kprintf("xhci_issue_command: Wrote TRB at index = %u, cycle = %u\n", idx, (x->cmd_cycle & 1));
    kprintf("xhci_issue_command: TRB Contents: [0]=0x%08x, [1]=0x%08x, [2]=0x%08x, [3]=0x%08x\n",
            x->cmd_ring[idx].field[0], x->cmd_ring[idx].field[1],
            x->cmd_ring[idx].field[2], x->cmd_ring[idx].field[3]);

    // Dump the command ring so we can inspect its contents.
    dump_cmd_ring(hc);

    // Ensure the TRB write is globally visible.
    __asm__ volatile("mfence" ::: "memory");

    // Advance the ring pointer.
    idx = x->cmd_ring_index + 1;
    if (idx == XHCI_CMD_RING_SIZE - 1) {
        idx = 0;
        x->cmd_cycle ^= 1;
        kprintf("xhci_issue_command: Wrapped around; toggled command ring cycle -> %u\n", x->cmd_cycle & 1);
    }
    x->cmd_ring_index = idx;

    // Dump operational registers before ringing the doorbell.
    kprintf("xhci_issue_command: Before doorbell, dumping op regs:\n");
    dump_op_regs(op_regs);

    kprintf("xhci_issue_command: Ringing doorbell (index 0, value 0x00)\n");
    xhci_ring_doorbell(hc, 0, 0);

    // Ensure the doorbell write is flushed.
    __asm__ volatile("mfence" ::: "memory");

    kprintf("xhci_issue_command: After doorbell, dumping op regs:\n");
    dump_op_regs(op_regs);

    // Extra step: re-write CRCR to try to clear the CNR flag.
    kprintf("xhci_issue_command: Rewriting CRCR to attempt clearing CNR\n");
    {
        uint64_t cmd_ring_phys = virt_to_phys(x->cmd_ring);
        op_regs[0x18/4] = (uint32_t)cmd_ring_phys | (x->cmd_cycle & 1);
        op_regs[0x1C/4] = (uint32_t)(cmd_ring_phys >> 32);
        __asm__ volatile("mfence" ::: "memory");
    }
    kprintf("xhci_issue_command: After CRCR rewrite, dumping op regs:\n");
    dump_op_regs(op_regs);

    // Prepare for waiting on command completion.
    awaiting_cmd = true;
    last_cmd_type = (cmd_trb->field[3] >> 10) & 0x3F;
    last_cmd_comp_code = 0;
    uint32_t start_ms = get_time_ms();
    uint32_t last_log_ms = start_ms;

    // Wait up to 2000ms for command completion.
    while (awaiting_cmd) {
        uint32_t current_ms = get_time_ms();
        if (current_ms - start_ms > 2000) {
            kprintf("xhci_issue_command: Timeout after %ums waiting for command completion.\n", current_ms - start_ms);
            usbsts = op_regs[1];
            kprintf("xhci_issue_command: Final USBSTS = 0x%08x, CRCR: low = 0x%08x, high = 0x%08x\n",
                    usbsts, op_regs[0x18/4], op_regs[0x1C/4]);
            if (usbsts & (1 << 0)) {
                kprintf("xhci_issue_command: xHC is halted.\n");
            }
            if (usbsts & (1 << 2)) {
                kprintf("xhci_issue_command: Host System Error set.\n");
            }
            return -1;
        }
        if (current_ms - last_log_ms > 100) {
            kprintf("xhci_issue_command: Waiting... USBSTS = 0x%08x, CRCR: low = 0x%08x, high = 0x%08x\n",
                    op_regs[1], op_regs[0x18/4], op_regs[0x1C/4]);
            last_log_ms = current_ms;
        }
        yield();
    }

    if (last_cmd_comp_code != XHCI_COMP_SUCCESS) {
        kprintf("xhci_issue_command: Command 0x%x completed with compCode = %d\n", last_cmd_type, last_cmd_comp_code);
        return -1;
    }
    if (last_cmd_type == TRB_TYPE_ENABLE_SLOT) {
        kprintf("xhci_issue_command: ENABLE_SLOT command succeeded, slot = %u\n", last_cmd_slot);
        return last_cmd_slot;
    }

    kprintf("xhci_issue_command: Command 0x%x completed successfully.\n", last_cmd_type);
    return 0;
}

// -----------------------------------------------------------------------------
// xhci_interrupt_handler_main
// -----------------------------------------------------------------------------
void xhci_interrupt_handler_main(uint64_t vector, uint32_t error) {
    kprintf("xhci_interrupt_handler_main: Interrupt received. Vector = 0x%lx, error = 0x%08x\n", vector, error);
    usb_host_controller_t *hc = NULL;
    for (int i = 0; i < usb_hc_count; i++) {
        if (usb_controllers[i].type == USB_HC_XHCI) {
            hc = &usb_controllers[i];
            break;
        }
    }
    if (!hc) {
        kprintf("xhci_interrupt_handler_main: No xHCI controller found.\n");
        apic_send_eoi_if_necessary((uint8_t)vector);
        return;
    }
    xhci_state_t *x = &hc->x.xhci;

    // Dump the event ring for debugging.
    dump_event_ring(hc);

    while (true) {
        xhci_trb_t *evt = &x->event_ring[x->evt_ring_index];
        uint8_t cycle = evt->field[3] & 1;
        if (cycle != x->evt_cycle) {
            break;  // No new events.
        }
        uint8_t evt_type = (evt->field[3] >> 10) & 0x3F;
        kprintf("xhci_interrupt_handler_main: Processing event TRB. Type = 0x%x, Fields: [0]=0x%08x, [1]=0x%08x, [2]=0x%08x, [3]=0x%08x\n",
                evt_type, evt->field[0], evt->field[1], evt->field[2], evt->field[3]);
        if (evt_type == TRB_TYPE_CMD_COMPLETION) {
            last_cmd_comp_code = (evt->field[2] >> 24) & 0xFF;
            if (last_cmd_type == TRB_TYPE_ENABLE_SLOT && last_cmd_comp_code == XHCI_COMP_SUCCESS) {
                last_cmd_slot = (evt->field[3] >> 24) & 0xFF;
            }
            kprintf("xhci_interrupt_handler_main: CMD_COMPLETION received. compCode = %d, last_cmd_type = 0x%x\n",
                    last_cmd_comp_code, last_cmd_type);
            awaiting_cmd = false;
        } else if (evt_type == TRB_TYPE_TRANSFER_EVENT) {
            transfer_comp_code = (evt->field[2] >> 24) & 0xFF;
            transfer_remain = evt->field[2] & 0xFFFFFF;
            kprintf("xhci_interrupt_handler_main: TRANSFER_EVENT received. compCode = %d, remain = 0x%x\n",
                    transfer_comp_code, transfer_remain);
            if (awaiting_transfer &&
                (((evt->field[3] >> 16) & 0x1F) == pending_ep) &&
                (((evt->field[3] >> 24) & 0xFF) == pending_slot)) {
                awaiting_transfer = false;
            }
        } else if (evt_type == TRB_TYPE_PORT_STATUS_CHANGE) {
            uint8_t port_id = evt->field[0] & 0xFF;
            kprintf("xhci_interrupt_handler_main: Port %u status change detected.\n", port_id);
            usb_process_device_connect(hc, port_id - 1, 0);
        }
        // Advance the event ring.
        x->evt_ring_index = (x->evt_ring_index + 1) % XHCI_EVENT_RING_SIZE;
        if (x->evt_ring_index == 0) {
            x->evt_cycle ^= 1;
        }
        // Update ERDP.
        uint64_t erdp = virt_to_phys(&x->event_ring[x->evt_ring_index]);
        x->run_regs[0x20/4 + 0x18/4] = (uint32_t)erdp;
        x->run_regs[0x20/4 + 0x1C/4] = (uint32_t)(erdp >> 32) | (1 << 3);
    }
    apic_send_eoi_if_necessary((uint8_t)vector);
}

// -----------------------------------------------------------------------------
// Transfer stubs
// -----------------------------------------------------------------------------

int xhci_control_transfer(usb_host_controller_t *hc, uint8_t dev_addr,
    usb_setup_packet_t *setup, void *buffer, int length)
{
    // ...
    // (unchanged; same logic for SETUP->DATA->STATUS TRBs)
    return -1; // stub or implement as needed
}

int xhci_bulk_transfer(usb_host_controller_t *hc, uint8_t dev_addr,
                       uint8_t endpoint, void *buffer, int length) {
    kprintf("xHCI: Bulk transfer not implemented\n");
    return -1;
}

int xhci_interrupt_transfer(usb_host_controller_t *hc, uint8_t dev_addr,
                            uint8_t endpoint, void *buffer, int length) {
    kprintf("xHCI: Interrupt transfer not implemented\n");
    return -1;
}

int xhci_iso_transfer(usb_host_controller_t *hc, uint8_t dev_addr,
                      uint8_t endpoint, void *buffer, int length,
                      uint32_t frame) {
    kprintf("xHCI: Iso transfer not implemented\n");
    return -1;
}
