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
#include <io.h>
#include <atomics.h>
#include <timer.h>
#include <mm/vmem.h>

atomic_bool awaiting_cmd = ATOMIC_VAR_INIT(true);
static volatile uint8_t last_cmd_type      = 0;
static volatile uint8_t last_cmd_comp_code = 0;
static volatile uint8_t last_cmd_slot      = 0;
atomic_bool awaiting_transfer = ATOMIC_VAR_INIT(false);
static volatile uint8_t transfer_comp_code = 0;

#define FIXED_VIRT_BASE 0xFFFFFF8000D00000ULL
#define PAGE_SIZE 4096
#define MEMORY_BARRIER() __asm__ __volatile__ ("mfence" ::: "memory")

void xhci_interrupt_handler_main(uint64_t vector, uint32_t error);
int xhci_issue_command(usb_host_controller_t *hc, xhci_trb_t *cmd_trb);

static void dump_cmd_ring(usb_host_controller_t *hc) {
    xhci_state_t *x = &hc->x.xhci;
    kprintf("----- Dumping Command Ring (Size=%d) -----\n", XHCI_CMD_RING_SIZE);
    for (int i = 0; i < XHCI_CMD_RING_SIZE; i++) {
        kprintf("cmd_ring[%2d]: [0]=0x%08x, [1]=0x%08x, [2]=0x%08x, [3]=0x%08x\n",
                i,
                x->cmd_ring[i].field[0], x->cmd_ring[i].field[1],
                x->cmd_ring[i].field[2], x->cmd_ring[i].field[3]);
    }
    kprintf("------------------------------------------\n");
}

static void dump_event_ring(usb_host_controller_t *hc) {
    xhci_state_t *x = &hc->x.xhci;
    kprintf("----- Dumping Event Ring (Size=%d) -----\n", XHCI_EVENT_RING_SIZE);
    for (int i = 0; i < XHCI_EVENT_RING_SIZE; i++) {
        kprintf("event_ring[%2d]: [0]=0x%08x, [1]=0x%08x, [2]=0x%08x, [3]=0x%08x\n",
                i,
                x->event_ring[i].field[0], x->event_ring[i].field[1],
                x->event_ring[i].field[2], x->event_ring[i].field[3]);
    }
    kprintf("----------------------------------------\n");
}

static void dump_op_regs(volatile uint32_t *op_regs) {
    kprintf("----- xHCI Operational Registers -----\n");
    kprintf("USBCMD   (0x00): 0x%08x\n", op_regs[0]);
    kprintf("USBSTS   (0x04): 0x%08x\n", op_regs[1]);
    kprintf("PAGESIZE (0x08): 0x%08x\n", op_regs[2]);
    kprintf("DNCTRL   (0x14): 0x%08x\n", op_regs[5]);
    kprintf("CRCR Low (0x18): 0x%08x\n", op_regs[0x18/4]);
    kprintf("CRCR High(0x1C): 0x%08x\n", op_regs[0x1C/4]);
    kprintf("--------------------------------------\n");
}

static void dump_runtime_regs(volatile uint32_t *run_regs) {
    if (!run_regs) {
        kprintf("dump_runtime_regs: run_regs is NULL\n");
        return;
    }
    volatile uint32_t *ir_base = run_regs + (0x20 / sizeof(uint32_t));
    kprintf("----- Runtime (Interrupt) Register Dump -----\n");
    kprintf("IMAN   (0x20): 0x%08x\n", ir_base[0]);
    kprintf("IMOD   (0x24): 0x%08x\n", ir_base[1]);
    kprintf("ERSTSZ (0x28): 0x%08x\n", ir_base[2]);
    kprintf("ERSTBA (0x2C/0x30): Low=0x%08x, High=0x%08x\n", ir_base[3], ir_base[4]);
    kprintf("ERDP   (0x34/0x38): Low=0x%08x, High=0x%08x\n", ir_base[5], ir_base[6]);
    kprintf("---------------------------------------------\n");
}

static void xhci_ring_doorbell(usb_host_controller_t *hc, uint8_t dbIndex, uint8_t value) {
    xhci_state_t *x = &hc->x.xhci;
    volatile uint32_t *op_regs = hc->op_base;
    uint32_t usbsts = op_regs[1];
    if (usbsts & 0x1)
        kprintf("xhci_ring_doorbell: Warning => xHC is halted!\n");
    if (usbsts & (1 << 2))
        kprintf("xhci_ring_doorbell: Warning => Host System Error!\n");
    kprintf("xhci_ring_doorbell(dbIndex=%u, val=0x%x)\n", dbIndex, value);
    dump_op_regs(op_regs);
    if (dbIndex == 0) {
        x->db_regs[0] = 0;
    } else {
        x->db_regs[dbIndex] = value;
    }
    MEMORY_BARRIER();
    dump_op_regs(op_regs);
    kprintf("doorbell[%d] readback=0x%08x\n", dbIndex, x->db_regs[dbIndex]);
}

static int xhci_configure_event_ring(usb_host_controller_t *hc) {
    xhci_state_t *x = &hc->x.xhci;
    kprintf("[ERCFG] xhci_configure_event_ring => begin\n");
    x->event_ring = dma_alloc_coherent(PAGE_SIZE, NULL); // vmalloc(PAGE_SIZE, VM_NOCACHE | VM_READ | VM_WRITE); // usb_alloc_page();
    if (!x->event_ring)
        return -1;
    memset(x->event_ring, 0, PAGE_SIZE);
    x->evt_ring_index = 0;
    x->evt_cycle = 1;
    x->erst = (xhci_erst_entry_t *)dma_alloc_coherent(PAGE_SIZE, NULL); // vmalloc(PAGE_SIZE, VM_NOCACHE | VM_READ | VM_WRITE); // usb_alloc_page();
    if (!x->erst)
        return -1;
    memset(x->erst, 0, PAGE_SIZE);
    uint64_t ring_phys = virt_to_phys(x->event_ring);
    uint64_t erst_phys = virt_to_phys(x->erst);
    kprintf("Event ring => virt=0x%p, phys=0x%lx\n", x->event_ring, ring_phys);
    kprintf("ERST => virt=0x%p, phys=0x%lx\n", (void*)x->erst, erst_phys);
    x->erst[0].seg_addr = ring_phys;
    x->erst[0].seg_size = XHCI_EVENT_RING_SIZE;
    x->erst[0].reserved = 0;
    MEMORY_BARRIER();
    kprintf("[ERCFG] ERST[0] => seg_addr=0x%016lx, seg_size=%u\n",
            x->erst[0].seg_addr, x->erst[0].seg_size);
    volatile uint32_t *ir_base = x->run_regs + (0x20 / sizeof(uint32_t));
    if (!ir_base)
        return -1;
    ir_base[0] &= ~(1 << 1);
    ir_base[0] |= 1 << 0; // clear IP
ir_base[0] |= 1 << 1; // enable IE

    MEMORY_BARRIER();
    for (volatile int i = 0; i < 500; i++);
    ir_base[1] = 0;
    ir_base[2] = 1;

// Check if the upper 32 bits are non-zero, which means the address is not within the low 4GB.
if ((erst_phys >> 32) != 0) {
    panic("Error: ERST physical address 0x%lx is above 4GB. 32-bit addressing is required.\n", erst_phys);
    // Option 1: Return an error
    // Option 2: Alternatively, attempt to reallocate from a low-memory pool if possible.
}

    ir_base[5] = (uint32_t)(erst_phys >> 32);
    ir_base[4] = (uint32_t)(erst_phys & 0xFFFFFFFFULL);
    MEMORY_BARRIER();
    for (volatile int i = 0; i < 500; i++);
    uint64_t aligned_erdp = ring_phys & ~0xFULL;
    ir_base[6] = (uint32_t)(aligned_erdp & 0xFFFFFFFFULL);
    uint32_t erdp_high = (uint32_t)((aligned_erdp >> 32) & 0xFFFFFFFFULL);
    ir_base[7] = erdp_high;
    MEMORY_BARRIER();
    for (volatile int i = 0; i < 500; i++);
    ir_base[0] |= (1 << 1);
    MEMORY_BARRIER();
    for (volatile int i = 0; i < 500; i++);
    dump_runtime_regs(x->run_regs);
    dump_op_regs(hc->op_base);
    uint64_t read_erdp_low = ir_base[6];
    uint64_t read_erdp_high = ir_base[7];
    uint64_t read_erdp = (read_erdp_high << 32) | read_erdp_low;
    uint64_t expected_erdp = aligned_erdp;
    if (read_erdp != expected_erdp)
        return -1;
    kprintf("Event ring config => done.\n");
    kprintf("[ERCFG] xhci_configure_event_ring => done\n");
    return 0;
}

static int xhci_port_warm_reset(volatile uint32_t *op_regs, int port_index) {
    #define PORTSC_WPR (1 << 19)
    volatile uint32_t *port_reg = &op_regs[(0x400/4) + (port_index * (0x10/4))];
    uint32_t ps = *port_reg;
    kprintf("[WR] Port[%d]: current PORTSC=0x%08x => attempting Warm Reset...\n", port_index, ps);
    ps &= ~(1 << 4);
    ps |= PORTSC_WPR;
    *port_reg = ps;
    MEMORY_BARRIER();
    uint32_t start_ms = get_time_ms();
    bool done = false;
    while ((get_time_ms() - start_ms) < 200) {
        uint32_t cur = *port_reg;
        if ((cur & PORTSC_WPR) == 0) {
            kprintf("[WR] Warm Reset bit cleared after ~%u ms => PORTSC=0x%08x\n",
                    (get_time_ms() - start_ms), cur);
            done = true;
            break;
        }
        yield();
    }
    if (!done)
        return -1;
    start_ms = get_time_ms();
    while ((get_time_ms() - start_ms) < 300) {
        uint32_t cur = *port_reg;
        uint8_t link_state = (cur >> 5) & 0xF;
        if (link_state != 0xE) {
            kprintf("[WR] Port[%d] no longer in compliance => linkState=0x%x, PORTSC=0x%08x\n",
                    port_index, link_state, cur);
            return 0;
        }
        yield();
    }
    kprintf("[WR] STILL stuck in compliance => port[%d]\n", port_index);
    return -2;
}

int test_xhci_noop(usb_host_controller_t *hc) {
    xhci_trb_t trb;
    memset(&trb, 0, sizeof(trb));
    trb.field[3] = (TRB_TYPE_DISABLE_SLOT << 10) | (hc->x.xhci.cmd_cycle & 1);
    return xhci_issue_command(hc, &trb);
}

int test_xhci_enable_slot(usb_host_controller_t *hc) {
    xhci_trb_t trb;
    memset(&trb, 0, sizeof(trb));
    trb.field[3] = (TRB_TYPE_ENABLE_SLOT << 10) | (hc->x.xhci.cmd_cycle & 1);
    return xhci_issue_command(hc, &trb);
}

static int xhci_register_interrupt(usb_host_controller_t *hc) {
    kprintf("[INT] xhci_register_interrupt => vector=0x50\n");
    register_interrupt_handler(0x50, xhci_interrupt_handler_main);
    return 0;
}

#define XHCI_CONTEXT_SIZE 32
#define USB_SPEED_FULL 2
#define USB_SPEED_HIGH 3
#define USB_SPEED_SUPER 4
#define CTRL_EP_TYPE 4
#define XHCI_INPUT_CTX_SIZE (XHCI_CONTEXT_SIZE * 3)

typedef struct __attribute__((packed, aligned(64))) {
    uint32_t drop_flags;
    uint32_t add_flags;
    uint32_t rsvd_icc[5];
    uint32_t icc_config_value;
    uint32_t slot_ctx[8];
    uint32_t ep0_ctx[8];
} xhci_input_context_t;

static inline void set_slot_context_speed(uint32_t *slot_ctx, uint8_t speed) {
    uint32_t tmp = slot_ctx[1];
    tmp &= ~(0xF << 20);
    tmp |= ((speed & 0xF) << 20);
    slot_ctx[1] = tmp;
}

static inline void set_slot_context_port(uint32_t *slot_ctx, uint8_t port_id) {
    uint32_t tmp = slot_ctx[3];
    tmp &= 0x00FFFFFF;
    tmp |= ((uint32_t)port_id << 24);
    slot_ctx[3] = tmp;
}

static inline void set_slot_context_numsf(uint32_t *slot_ctx, uint8_t num_ctx) {
    uint32_t tmp1 = slot_ctx[1];
    tmp1 &= 0xFFFFFF00;
    tmp1 |= (num_ctx & 0xFF);
    slot_ctx[1] = tmp1;
}

static inline void set_ep0_context(uint32_t *ep0_ctx, uint16_t max_packet_size) {
    uint32_t val = 0;
    val |= ((uint32_t)CTRL_EP_TYPE << 3);
    val |= ((uint32_t)max_packet_size << 16);
    ep0_ctx[1] = val;
}

static int xhci_address_device(usb_host_controller_t *hc, uint8_t slot_id, uint8_t port_id, uint8_t speed) {
    xhci_state_t *x = &hc->x.xhci;
    void *ictx_virt = dma_alloc_coherent(PAGE_SIZE, NULL); // vmalloc(PAGE_SIZE, VM_NOCACHE | VM_READ | VM_WRITE); // pmm_alloc();
    if (!ictx_virt)
        return -1;
    memset(ictx_virt, 0, PAGE_SIZE);
    uint64_t ictx_phys = virt_to_phys(ictx_virt);
    xhci_input_context_t *ictx = (xhci_input_context_t *)ictx_virt;
    ictx->drop_flags = 0;
    ictx->add_flags  = 0x03;
    set_slot_context_speed(&ictx->slot_ctx[0], speed);
    set_slot_context_port(&ictx->slot_ctx[0], port_id+1);
    set_slot_context_numsf(&ictx->slot_ctx[0], 1);
    set_ep0_context(&ictx->ep0_ctx[0], 64);
    xhci_trb_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.field[0] = (uint32_t)(ictx_phys & 0xFFFFFFFFU);
    cmd.field[1] = (uint32_t)(ictx_phys >> 32);
    uint32_t slot_bits = ((uint32_t)slot_id << 24);
    uint32_t trb_type = (TRB_TYPE_ADDRESS_DEVICE << 10);
    uint32_t bsr_bit  = (1 << 9);
    uint32_t cycle    = (x->cmd_cycle & 1);
    cmd.field[3] = (slot_bits | trb_type | bsr_bit | cycle);
    kprintf("[xhci_address_device] => Issuing AddressDevice (slot=%u, port=%u, speed=%u)\n",
            slot_id, port_id, speed);
    int rc = xhci_issue_command(hc, &cmd);
    if (rc < 0)
        return rc;
    kprintf("[xhci_address_device] => SUCCESS => slot=%u is now Addressed at xHC\n", slot_id);
    return 0;
}

void fix_link_trb_cycle_bit(xhci_trb_t *link_trb, uint64_t ring_phys) {
    link_trb->field[0] = (uint32_t)(ring_phys & 0xFFFFFFFFULL);
    link_trb->field[1] = (uint32_t)(ring_phys >> 32);
    link_trb->field[2] = 0;
    link_trb->field[3] = (TRB_TYPE_LINK << 10) | (1 << 1); // TC = 1, Cycle bit cleared
}

#include <thread.h>

int init_xhci_controller(pci_device_t *pci_dev) {
    kprintf("======== ENTER init_xhci_controller() ========\n");
    kprintf("init_xhci_controller: bus=%02x slot=%02x func=%x\n",
            pci_dev->bus, pci_dev->slot, pci_dev->function);
    for (int offset = 0; offset < 256; offset += 4) {
        uint32_t val = pci_read32(pci_dev->bus, pci_dev->slot, pci_dev->function, offset);
        kprintf("  PCI cfg @0x%02x => 0x%08x\n", offset, val);
    }
    uint16_t cmd_reg = pci_read16(pci_dev->bus, pci_dev->slot, pci_dev->function, 0x04);
    cmd_reg |= 0x0007;
    pci_write16(pci_dev->bus, pci_dev->slot, pci_dev->function, 0x04, cmd_reg);
    #define PCI_FLADJ_OFFSET 0xD8
    #define XHCI_FLADJ_VALUE 0x20
    pci_write8(pci_dev->bus, pci_dev->slot, pci_dev->function, PCI_FLADJ_OFFSET, XHCI_FLADJ_VALUE);
    kprintf("[DBG] FLADJ register set to 0x%x\n", XHCI_FLADJ_VALUE);
    int bar_index = -1;
    for (int i = 0; i < 6; i++) {
        if (pci_dev->bar[i] != 0 && pci_dev->bar[i] != 0xFFFFFFFFFFFFFFFFULL) {
            bar_index = i;
            break;
        }
    }
    if (bar_index < 0)
        return -1;
    uintptr_t base_phys = pci_resource_start(pci_dev, bar_index);
    size_t mmio_size    = pci_resource_len(pci_dev, bar_index);
    size_t num_pages    = (mmio_size + PAGE_SIZE - 1) / PAGE_SIZE;
    kprintf("[DBG] xHCI base_phys=0x%lx, mmio_size=0x%lx\n",
            (unsigned long)base_phys, (unsigned long)mmio_size); // FIXED_VIRT_BASE);
    uintptr_t bar_virt = vmap_phys(base_phys, 0, num_pages * PAGE_SIZE, VM_READ | VM_WRITE | VM_NOCACHE, "xhci mmio");
    // early_map_entries(FIXED_VIRT_BASE, base_phys, num_pages, (VM_READ | VM_WRITE | VM_NOCACHE));
    volatile uint32_t *cap_regs = (volatile uint32_t *)bar_virt; // FIXED_VIRT_BASE;
    uint8_t cap_length = (uint8_t)(cap_regs[0] & 0xFF);
    volatile uint32_t *op_regs = cap_regs + (cap_length / 4);
    #define USBSTS_REG op_regs[1]
    #define USBSTS_HCHALT (1U << 0)
    #define USBSTS_CNR_BIT (1U << 12)
    uint32_t usbcmd = op_regs[0];
    uint32_t usbsts = USBSTS_REG;
    if (!(usbsts & USBSTS_HCHALT)) {
        usbcmd &= ~1U;
        op_regs[0] = usbcmd;
        uint32_t timeout = 1000000;
        while (!(USBSTS_REG & USBSTS_HCHALT) && --timeout) {
            yield();
        }
        if (!timeout)
            return -1;
    }
    #define USBCMD_HCRESET (1 << 1)
    op_regs[0] = usbcmd | USBCMD_HCRESET;
    {
        uint32_t timeout = 1000000;
        while ((op_regs[0] & USBCMD_HCRESET) && --timeout) {
            yield();
        }
        if (!timeout)
            return -1;
    }
    {
        uint32_t timeout = 1000000;
        while (!(USBSTS_REG & USBSTS_HCHALT) && --timeout) {
            yield();
        }
        if (!timeout)
            return -1;
    }
    kprintf("[DBG] Software Reset complete => USBCMD=0x%08x USBSTS=0x%08x\n", op_regs[0], USBSTS_REG);
    {
        uint32_t timeout = 1000000;
        while ((USBSTS_REG & USBSTS_CNR_BIT) && --timeout) {
            yield();
        }
        if (!timeout)
            return -1;
        kprintf("[DBG] xHC hardware-ready => USBSTS=0x%08x\n", USBSTS_REG);
    }
    usb_host_controller_t *hc = &usb_controllers[usb_hc_count];
    memset(hc, 0, sizeof(*hc));
    hc->type    = USB_HC_XHCI;
    hc->pci_dev = pci_dev;
    hc->op_base = op_regs;
    uint32_t dboff_reg  = cap_regs[0x14/4];
    uint32_t rtsoff_reg = cap_regs[0x18/4];
    uint32_t dboff  = (dboff_reg & ~0x3);
    uint32_t rtsoff = (rtsoff_reg & ~0x1F);
    hc->x.xhci.db_regs  = (volatile uint32_t *)((uintptr_t)cap_regs + dboff);
    hc->x.xhci.run_regs = (volatile uint32_t *)((uintptr_t)cap_regs + rtsoff);
    op_regs[2] = 0x1;
    kprintf("[DBG] PAGESIZE => readback=0x%08x\n", op_regs[2]);
    uint32_t hcsp1 = cap_regs[1];
    uint8_t max_slots = (uint8_t)(hcsp1 & 0xFF);
    op_regs[0x38/4] = max_slots;
    kprintf("[DBG] CONFIG => MaxSlotsEn=%u => readback=0x%08x\n", max_slots, op_regs[0x38/4]);
    void *dcbaa_page = dma_alloc_coherent(PAGE_SIZE, NULL); // kmalloc(PAGE_SIZE); vmalloc(PAGE_SIZE, VM_NOCACHE | VM_READ | VM_WRITE); // pmm_alloc();
    if (!dcbaa_page)
        return -1;
    memset(dcbaa_page, 0, PAGE_SIZE);
    hc->x.xhci.dcbaa = (uint64_t *)dcbaa_page;
    uint64_t dcbaa_phys = virt_to_phys(dcbaa_page);
    op_regs[0x30/4] = (uint32_t)(dcbaa_phys & 0xFFFFFFFFULL);
    op_regs[0x34/4] = (uint32_t)(dcbaa_phys >> 32);
    void *cmd_page = dma_alloc_coherent(PAGE_SIZE, NULL); // vmalloc(PAGE_SIZE, VM_NOCACHE | VM_READ | VM_WRITE); // pmm_alloc();
    if (!cmd_page)
        return -1;
    memset(cmd_page, 0, PAGE_SIZE);
    hc->x.xhci.cmd_ring       = (xhci_trb_t *)cmd_page;
    hc->x.xhci.cmd_ring_index = 0;
    hc->x.xhci.cmd_cycle      = 1;
    uint64_t cmd_ring_phys    = virt_to_phys(cmd_page);
    unsigned last_trb    = XHCI_CMD_RING_SIZE - 1;
    xhci_trb_t *link_trb = &hc->x.xhci.cmd_ring[last_trb];
    memset(link_trb, 0, sizeof(*link_trb));
    link_trb->field[0] = (uint32_t)(cmd_ring_phys & 0xFFFFFFFFULL);
    link_trb->field[1] = (uint32_t)(cmd_ring_phys >> 32);
    link_trb->field[3] = (TRB_TYPE_LINK << 10) | (1 << 1); //  | (hc->x.xhci.cmd_cycle & 1);
    op_regs[0x18/4] = (uint32_t)(cmd_ring_phys | (hc->x.xhci.cmd_cycle & 1));
    op_regs[0x1C/4] = (uint32_t)(cmd_ring_phys >> 32);
    hc->irq = pci_dev->irq;
    if (hc->irq == 0xFF)
        hc->irq = 10;
    int msix_res = pci_enable_msix(pci_dev, 0x50, 1);
    if (msix_res != 0) {
        if (hc->irq != 0xFF && hc->irq != 0) {
            ioapic_map_irq(hc->irq, 0x50);
            kprintf("[DBG] Using legacy => IRQ=0x%02x\n", 0x50);
        }
    }
    if (xhci_configure_event_ring(hc) != 0)
        return -1;
    uint32_t cmd_val = op_regs[0];
    cmd_val |= (1 << 2);
    op_regs[0] = cmd_val;
    usbcmd = op_regs[0];
    usbcmd |= 1U;
    op_regs[0] = usbcmd;
    {
        uint32_t timeout = 1000000;
        while ((USBSTS_REG & USBSTS_HCHALT) && --timeout) {
            yield();
        }
        if (!timeout)
            return -1;
    }
    kprintf("[DBG] xHC is running => USBSTS=0x%08x\n", USBSTS_REG);
    if (USBSTS_REG & (1 << 2)) {
        USBSTS_REG |= (1 << 2);
        kprintf("[DBG] HSE was set => cleared => USBSTS=0x%08x\n", USBSTS_REG);
    }
    if (xhci_register_interrupt(hc) != 0)
        return -1;
    usb_hc_count++;
    hc->num_ports = hcsp1 & 0xFF;
    kprintf("[DBG] xHCI: Detected %u root hub ports\n", hc->num_ports);
    for (int port = 100; port < hc->num_ports; port++) {
        uint32_t port_status = *((volatile uint32_t *)((uintptr_t)hc->op_base + PORTSC_OFFSET(port)));
        if (port_status & PORTSC_CONNECTED_BIT) {
            kprintf("Polling: Device detected on port %d\n", port);
            int slot = test_xhci_enable_slot(hc);
            if (slot <= 0) {
                kprintf("[ERR] EnableSlot failed for port %d\n", port);
                continue;
            }
            kprintf("USB: Device connected on controller xHCI port %d, assigned slot %d\n",
                    port, slot);
            uint8_t port_speed = (port_status >> 10) & 0xF;
            if (port_speed == 0)
                port_speed = USB_SPEED_FULL;
            int rc = xhci_address_device(hc, slot, port, port_speed);
            if (rc < 0)
                continue;
            usb_process_device_connect(hc, port, slot);
        }
    }
    int ret = test_xhci_enable_slot(hc); // this is where the interrupt never fires again
    kprintf("[DBG] test_xhci_enable_slot => ret=%d\n", ret);
    if (ret < 0) {
        kprintf("[ERR] EnableSlot cmd => fail => ret=%d\n", ret);
    }
    dump_op_regs(hc->op_base);
    kprintf("======== EXIT init_xhci_controller() success ========\n");
    return 0;
}

int xhci_issue_command(usb_host_controller_t *hc, xhci_trb_t *cmd_trb) {
    xhci_state_t *x = &hc->x.xhci;
    volatile uint32_t *op_regs = hc->op_base;
    uint32_t usbsts = op_regs[1];
    kprintf("[CMD] xhci_issue_command => start => USBSTS=0x%08x\n", usbsts);
    dump_op_regs(op_regs);
    if (usbsts & 1)
        kprintf("[CMD] WARNING => xHC is halted!\n");
    if (usbsts & (1 << 2))
        kprintf("[CMD] WARNING => Host System Error bit set.\n");
    if (x->cmd_ring_index == (XHCI_CMD_RING_SIZE - 1)) {
        x->cmd_ring_index = 0;
        x->cmd_cycle ^= 1;
        kprintf("[CMD] Command ring wrap => new cycle=%u\n", x->cmd_cycle & 1);
    }
    uint16_t idx = x->cmd_ring_index;
    x->cmd_ring[idx] = *cmd_trb;
    x->cmd_ring[idx].field[3] &= ~1u;
    x->cmd_ring[idx].field[3] |= (x->cmd_cycle & 1);
    kprintf("[CMD] Issued cmd TRB@idx=%u => [0]=0x%08x [1]=0x%08x [2]=0x%08x [3]=0x%08x\n",
            idx,
            x->cmd_ring[idx].field[0],
            x->cmd_ring[idx].field[1],
            x->cmd_ring[idx].field[2],
            x->cmd_ring[idx].field[3]);
    dump_cmd_ring(hc);
    idx++;
    if (idx == (XHCI_CMD_RING_SIZE - 1)) {
        idx = 0;
        x->cmd_cycle ^= 1;
        kprintf("[CMD] wrapped cmd ring again => cycle=%u\n", x->cmd_cycle & 1);
    }
    x->cmd_ring_index = idx;
    atomic_store_explicit(&awaiting_cmd, true, memory_order_release);
    last_cmd_type = (cmd_trb->field[3] >> 10) & 0x3F;
    last_cmd_comp_code = 0;
    kprintf("[CMD] Doorbell => 0\n");
    xhci_ring_doorbell(hc, 0, 0);
    uint32_t start = get_time_ms();
    kprintf("[CMD] waiting up to 2000 ms for command completion\n");
    while (atomic_load_explicit(&awaiting_cmd, memory_order_acquire)) {
        uint32_t now = get_time_ms();
        if (now - start > 2000) {
            kprintf("[CMD] TIMEOUT => no cmd completion after 2000ms.\n");
            usbsts = op_regs[1];
            kprintf("[CMD] Final USBSTS=0x%08x => CRCR Low=0x%08x High=0x%08x\n",
                    usbsts, op_regs[0x18/4], op_regs[0x1C/4]);
            dump_cmd_ring(hc);
            dump_runtime_regs(x->run_regs);
            return -1;
        }
    }
    if (last_cmd_comp_code != XHCI_COMP_SUCCESS) {
        kprintf("[CMD] Command=0x%x => compCode=%u => error\n",
                last_cmd_type, last_cmd_comp_code);
        return -1;
    }
    if (last_cmd_type == TRB_TYPE_ENABLE_SLOT) {
        kprintf("[CMD] EnableSlot => got slot=%u\n", last_cmd_slot);
        return last_cmd_slot;
    }
    kprintf("[CMD] Command=0x%x completed successfully\n", last_cmd_type);
    return 0;
}

void xhci_interrupt_handler_main(uint64_t vector, uint32_t error) {
    kprintf("[ISR] xhci_interrupt_handler_main => vector=0x%lx, err=0x%x\n", vector, error);

    usb_host_controller_t *hc = NULL;
    for (int i = 0; i < usb_hc_count; i++) {
        if (usb_controllers[i].type == USB_HC_XHCI) {
            hc = &usb_controllers[i];
            break;
        }
    }

    if (!hc) {
        apic_send_eoi_if_necessary((uint8_t)vector);
        return;
    }

    xhci_state_t *x = &hc->x.xhci;
    volatile uint32_t *run_regs = x->run_regs;

    kprintf("[ISR] Dump event ring:\n");
    dump_event_ring(hc);

    // Acknowledge this interrupt in IMAN:
    uint32_t iman = run_regs[0x20/4];
    kprintf("[ISR] IMAN read => 0x%08x\n", iman);

    // Clear interrupt pending bit (bit0: interrupt pending)
    iman |= 1;
    run_regs[0x20/4] = iman;
    MEMORY_BARRIER();

    int count = 0;
    while (true) {
        xhci_trb_t *evt_trb = &x->event_ring[x->evt_ring_index];
        uint8_t cycle_bit = (evt_trb->field[3] & 0x01);

        if (cycle_bit != x->evt_cycle) {
            kprintf("[ISR] No new event at idx=%u => processed %d events\n",
                    x->evt_ring_index, count);
            break;
        }

        uint8_t trb_type = (evt_trb->field[3] >> 10) & 0x3F;
        switch (trb_type) {

            case TRB_TYPE_CMD_COMPLETION: {
                uint8_t comp_code = (evt_trb->field[2] >> 24) & 0xFF;
                last_cmd_comp_code = comp_code;
                if (last_cmd_type == TRB_TYPE_ENABLE_SLOT) {
                    if (comp_code == XHCI_COMP_SUCCESS) {
                        last_cmd_slot = (evt_trb->field[3] >> 24) & 0xFF;
                        kprintf("[ISR] EnableSlot succeeded, slot=%u\n", last_cmd_slot);
                    } else {
                        kprintf("[ISR] EnableSlot failed, code=%u\n", comp_code);
                    }
                }
                atomic_store(&awaiting_cmd, false);
                break;
                /*if (last_cmd_type == TRB_TYPE_ENABLE_SLOT && comp_code == XHCI_COMP_SUCCESS) {
                    last_cmd_slot = (evt_trb->field[3] >> 24) & 0xFF;
                    kprintf("[ISR] EnableSlot command completed => slot=%u\n", last_cmd_slot);
                }
                atomic_store_explicit(&awaiting_cmd, false, memory_order_release);
                break;*/
            }

            case TRB_TYPE_PORT_STATUS_CHANGE: {
                // FIX: Port ID is in bits [31:24] of field[0].
                dump_op_regs(hc->op_base);
                uint8_t port_id = (uint8_t)((evt_trb->field[0] >> 24) & 0xFF);

                // Log the correct port # now
                kprintf("[ISR] Port status change => port=%u => run usb_process_device_connect\n",
                        port_id);

                // If your code expects 0-based ports inside usb_process_device_connect, 
                // subtract 1. If your code is 1-based internally, pass port_id as-is.
                // Commonly xHCI registers are 1-based, so you might do (port_id - 1):
                usb_process_device_connect(hc, (uint8_t)(port_id - 1), 0); // use 0-based inedex
                break;
            }

            case TRB_TYPE_TRANSFER_EVENT: {
                uint8_t comp_code = (evt_trb->field[2] >> 24) & 0xFF;
                kprintf("[ISR] Transfer Event => code=%u => handle the completed transfer\n", comp_code);
                transfer_comp_code = comp_code;
                atomic_store_explicit(&awaiting_transfer, false, memory_order_release);
                break;
            }

            default: {
                panic("[ISR] Unknown Event: type=0x%x\n", trb_type);
                break;
            }
        }

        // Advance the event ring index
        count++;
        x->evt_ring_index = (x->evt_ring_index + 1) % XHCI_EVENT_RING_SIZE;

        // Toggle cycle if we wrapped
        if (x->evt_ring_index == 0) {
            x->evt_cycle ^= 1;
            kprintf("[ISR] Event ring cycle toggled => %u\n", x->evt_cycle);
        }

        // Advance ERDP (Event Ring Dequeue Pointer)
        uint64_t new_erdp = virt_to_phys(&x->event_ring[x->evt_ring_index]);
        // new_erdp &= ~0xFULL;
        new_erdp |= (1 << 3);

        // Lower 32 bits
        run_regs[0x38/4] = (uint32_t)(new_erdp & 0xFFFFFFFFULL);

        // Upper 32 bits + set EHB (bit 3)
        uint32_t erdp_high = (uint32_t)((new_erdp >> 32) & 0xFFFFFFFFULL);
        erdp_high |= (1 << 3);
        run_regs[0x3C/4] = erdp_high;
        MEMORY_BARRIER();
    }

    kprintf("[ISR] ISR => processed %d events => EOI\n", count);
    __asm__ ("sti");
    MEMORY_BARRIER();
    kprintf("awaiting_cmd: %llu\n", awaiting_cmd);
    apic_send_eoi_if_necessary((uint8_t)vector);
}

static int xhci_submit_transfer(usb_host_controller_t *hc, uint8_t dev_addr, uint8_t endpoint, xhci_trb_t *trbs, int trb_count) {
    xhci_state_t *x = &hc->x.xhci;
    if (!x->ctrl_ring) {
        x->ctrl_ring = (xhci_trb_t *)dma_alloc_coherent(PAGE_SIZE, NULL); // vmalloc(PAGE_SIZE, VM_NOCACHE | VM_READ | VM_WRITE); // pmm_alloc();
        if (!x->ctrl_ring)
            return -1;
        memset(x->ctrl_ring, 0, PAGE_SIZE);
        x->ctrl_ring_index = 0;
        x->ctrl_cycle = 1;
    }
    for (int i = 0; i < trb_count; i++) {
        uint16_t idx = x->ctrl_ring_index;
        x->ctrl_ring[idx] = trbs[i];
        x->ctrl_ring[idx].field[3] = (x->ctrl_ring[idx].field[3] & ~1u) | (x->ctrl_cycle & 1);
        idx++;
        if (idx >= XHCI_CTRL_RING_SIZE) {
            idx = 0;
            x->ctrl_cycle ^= 1;
        }
        x->ctrl_ring_index = idx;
    }
    xhci_ring_doorbell(hc, dev_addr, endpoint);
    return 0;
}

int xhci_control_transfer(usb_host_controller_t *hc, uint8_t dev_addr, usb_setup_packet_t *setup, void *buffer, int length) {
    kprintf("DEBUG: xhci_control_transfer: Starting control transfer for device %d\n", dev_addr);
    kprintf("DEBUG: Dumping Operational Registers at start:\n");
    dump_op_regs(hc->op_base);

    // Build the TRB chain for the control transfer
    xhci_trb_t trbs[3];
    memset(trbs, 0, sizeof(trbs));
    int trb_count = 0;

    // --- Setup Stage TRB ---
    kprintf("DEBUG: Building Setup Stage TRB\n");
    // Copy 8 bytes of the setup packet into TRB field[0..1]
    memcpy(&trbs[0].field[0], setup, 8);
    kprintf("DEBUG: Setup TRB fields before setting type:\n");
    kprintf("        field[0] = 0x%08x\n", trbs[0].field[0]);
    kprintf("        field[1] = 0x%08x\n", trbs[0].field[1]);
    trbs[0].field[3] = (TRB_TYPE_SETUP << 10) | 1; // Set type and cycle bit
    kprintf("DEBUG: Setup TRB field[3] set to 0x%08x\n", trbs[0].field[3]);
    trb_count++;

    // --- Data Stage TRB (if any) ---
    if (length > 0) {
        kprintf("DEBUG: Building Data Stage TRB for length=%d\n", length);
        uint64_t buf_phys = virt_to_phys(buffer);
        kprintf("DEBUG: Buffer virtual address = %p, physical = 0x%016llx\n", buffer, buf_phys);
        trbs[1].field[0] = (uint32_t)(buf_phys & 0xFFFFFFFFULL);
        trbs[1].field[1] = (uint32_t)(buf_phys >> 32);
        trbs[1].field[2] = (uint32_t)length;
        uint32_t direction_flag = (setup->bmRequestType & 0x80) ? (1 << 16) : 0;
        trbs[1].field[3] = (TRB_TYPE_DATA << 10) | direction_flag | 1;  // Set type, direction flag, cycle bit
        kprintf("DEBUG: Data TRB fields:\n");
        kprintf("        field[0] = 0x%08x\n", trbs[1].field[0]);
        kprintf("        field[1] = 0x%08x\n", trbs[1].field[1]);
        kprintf("        field[2] = 0x%08x\n", trbs[1].field[2]);
        kprintf("        field[3] = 0x%08x\n", trbs[1].field[3]);
        trb_count++;
    } else {
        kprintf("DEBUG: No Data Stage TRB required (length == 0)\n");
    }

    // --- Status Stage TRB ---
    kprintf("DEBUG: Building Status Stage TRB\n");
    trbs[trb_count].field[3] = (TRB_TYPE_STATUS << 10) | 1;
    kprintf("DEBUG: Status TRB field[3] set to 0x%08x\n", trbs[trb_count].field[3]);
    trb_count++;

    // Dump entire TRB chain for debugging
    kprintf("DEBUG: Final TRB chain (total TRBs = %d):\n", trb_count);
    for (int i = 0; i < trb_count; i++) {
        kprintf("  TRB[%d]: [0]=0x%08x, [1]=0x%08x, [2]=0x%08x, [3]=0x%08x\n",
                i,
                trbs[i].field[0],
                trbs[i].field[1],
                trbs[i].field[2],
                trbs[i].field[3]);
    }

    // Set up transfer completion tracking
    atomic_store_explicit(&awaiting_transfer, true, memory_order_release);
    kprintf("DEBUG: Set awaiting_transfer flag to true\n");

    // Submit the TRB chain via xhci_submit_transfer
    kprintf("DEBUG: Submitting control transfer TRB chain\n");
    int ret = xhci_submit_transfer(hc, dev_addr, 0, trbs, trb_count);
    if (ret < 0) {
        kprintf("DEBUG: xhci_submit_transfer returned error code %d\n", ret);
        return ret;
    }
    kprintf("DEBUG: xhci_submit_transfer completed successfully, waiting for transfer completion...\n");

    // Start a timer and wait for the transfer to complete
    uint32_t start = get_time_ms();
    uint32_t elapsed = 0;
    while (atomic_load_explicit(&awaiting_transfer, memory_order_acquire)) {
        elapsed = get_time_ms() - start;
        if (elapsed > 2000) {
            kprintf("DEBUG: Timeout waiting for transfer completion after %u ms\n", elapsed);
            dump_op_regs(hc->op_base);
            return -1;
        }
        // Optionally, add a short yield and log every 100ms
        if (elapsed % 100 < 10) {
        }
    }
    kprintf("DEBUG: Transfer completion detected after %u ms\n", elapsed);

    // Check the transfer completion code
    if (transfer_comp_code != XHCI_COMP_SUCCESS) {
        kprintf("DEBUG: xhci_control_transfer: Transfer failed, comp code=%u\n", transfer_comp_code);
        dump_op_regs(hc->op_base);
        return -1;
    }

    kprintf("DEBUG: xhci_control_transfer: Control transfer completed successfully\n");
    dump_op_regs(hc->op_base);
    return 0;
}

int xhci_bulk_transfer(usb_host_controller_t *hc, uint8_t dev_addr, uint8_t endpoint, void *buffer, int length) {
    kprintf("xhci_bulk_transfer => unimplemented.\n");
    return -1;
}

int xhci_interrupt_transfer(usb_host_controller_t *hc, uint8_t dev_addr, uint8_t endpoint, void *buffer, int length) {
    kprintf("xhci_interrupt_transfer => unimplemented.\n");
    return -1;
}

int xhci_iso_transfer(usb_host_controller_t *hc, uint8_t dev_addr, uint8_t endpoint, void *buffer, int length, uint32_t frame) {
    kprintf("xhci_iso_transfer => unimplemented.\n");
    return -1;
}
