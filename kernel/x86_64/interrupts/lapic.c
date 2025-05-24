#include <interrupts/lapic.h>
#include <acpi/acpi.h>
#include <msr.h>
#include <io.h>
#include <printf.h>
#include <panic.h>
#include <acpi/tables.h>  // Needed for MADT parsing when checking for overrides
#include <cpu/cpu.h>
#include <queue.h>
#include <mm/vmem.h>
#include <mm/pmm.h>
#include <init.h>

#define APIC_BASE_PA 0xFEE00000

#define ICR_LOW_REG_MASK     0xFFF99000
#define ICR_HIGH_REG_MASK    0x00FFFFFF

#define ICR_VECTOR_SHIFT     0
#define ICR_DELIV_MODE_SHIFT 8
#define ICR_DEST_MODE_SHIFT  10
#define ICR_LEVEL_SHIFT      14
#define ICR_TRIG_MODE_SHIFT  15
#define ICR_DEST_SHRT_SHIFT  18
#define ICR_DEST_SHIFT       24

typedef enum apic_reg {
  APIC_ID            = 0x020,
  APIC_VERSION       = 0x030,
  APIC_TPR           = 0x080,
  APIC_APR           = 0x090,
  APIC_PPR           = 0x0A0,
  APIC_EOI           = 0x0B0,
  APIC_RRD           = 0x0C0,
  APIC_LDR           = 0x0D0,
  APIC_DFR           = 0x0E0,
  APIC_SVR           = 0x0F0,
  APIC_ERROR         = 0x280,
  APIC_LVT_CMCI      = 0x2F0,
  APIC_ICR_LOW       = 0x300,
  APIC_ICR_HIGH      = 0x310,
  APIC_LVT_TIMER     = 0x320,
  APIC_LVT_LINT0     = 0x350,
  APIC_LVT_LINT1     = 0x360,
  APIC_LVT_ERROR     = 0x370,
  APIC_INITIAL_COUNT = 0x380,
  APIC_CURRENT_COUNT = 0x390,
  APIC_DIVIDE_CONFIG = 0x3E0,
} apic_reg_t;

struct apic_device {
    uint8_t id;
    uint8_t : 8;
    uint16_t : 16;
    uintptr_t phys_addr;
    uintptr_t address;
    LIST_ENTRY(struct apic_device) list;
  };

static uint32_t apic_clock; // ticks per second

static uint32_t *local_apic_base = NULL;  // Mapped address for Local APIC registers

uintptr_t apic_base = APIC_BASE_PA;
static size_t num_apics = 0;
static LIST_HEAD(struct apic_device) apics;

// Low-level functions to read and write Local APIC registers.
inline void lapic_write(uint32_t reg_offset, uint32_t value) {
    if (!local_apic_base) {
        kprintf("ERROR: Attempt to write LAPIC register when local_apic_base is NULL\n");
        panic("lapic_write: local_apic_base is NULL");
    }
    *(volatile uint32_t *)((uintptr_t)local_apic_base + reg_offset) = value;
}

inline uint32_t lapic_read(uint32_t reg_offset) {
    if (!local_apic_base) {
        kprintf("ERROR: Attempt to read LAPIC register when local_apic_base is NULL\n");
        panic("lapic_read: local_apic_base is NULL");
    }
    return *(volatile uint32_t *)((uintptr_t)local_apic_base + reg_offset);
}

// Helper to set the LAPIC Task Priority Register (TPR).
void lapic_set_tpr(uint8_t priority) {
    lapic_write(LAPIC_TPR, (uint32_t)priority);
}

// Disable the legacy 8259 PIC by remapping and masking all interrupts.
static void disable_pic(void) {
    kprintf("Disabling legacy 8259 PIC...\n");

    // Start PIC initialization (cascade mode, expect ICW4)
    outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4);
    outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4);
    io_wait();

    // Remap PIC vectors: Master to 0x20, Slave to 0x28.
    outb(PIC1_DATA, 0x20);
    outb(PIC2_DATA, 0x28);
    io_wait();

    // Setup cascade configuration.
    outb(PIC1_DATA, 0x04);  // Master: there is a slave at IRQ2.
    outb(PIC2_DATA, 0x02);  // Slave: its cascade identity is 2.
    io_wait();

    // Set PICs to 8086 mode.
    outb(PIC1_DATA, ICW4_8086);
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    // Mask all IRQs.
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
    kprintf("PIC remapped and masked (all IRQs off).\n");
}

static inline apic_reg_lvt_timer_t apic_read_timer() {
    apic_reg_lvt_timer_t timer = { .raw = lapic_read(APIC_LVT_TIMER) };
    return timer;
  }

static inline void apic_write_timer(apic_reg_lvt_timer_t timer) {
    lapic_write(APIC_LVT_TIMER, timer.raw);
  }

void apic_udelay(uint64_t us) {
    apic_reg_lvt_timer_t timer = apic_read_timer();
    timer.timer_mode = APIC_ONE_SHOT;
    timer.mask = APIC_MASK;
    apic_write_timer(timer);
    while (us > 0) {
      uint32_t val = min(us, US_PER_SEC);
      uint32_t count = apic_clock / (US_PER_SEC / val);
      lapic_write(APIC_INITIAL_COUNT, count);
  
      while (lapic_read(APIC_CURRENT_COUNT) != 0) {
        cpu_pause();
      }
      us -= val;
    }
  }
  
  void apic_mdelay(uint64_t ms) {
    apic_udelay(ms * 1000);
  }

  static inline volatile uint32_t *apic_reg_ptr(apic_reg_t reg) {
    uintptr_t addr = apic_base + reg;
    volatile uint32_t *ptr = (uint32_t *) addr;
    return ptr;
  }

  void poll_icr_status() {
    volatile uint32_t *low = apic_reg_ptr(APIC_ICR_LOW);
    while (apic_icr_status(*low)) {
      // if icr is pending poll until it finishes
      cpu_pause();
    }
  }

void lapic_init(void) {
    kprintf("Initializing Local APIC...\n");
    disable_pic();

    // Read APIC base from MSR and enable the LAPIC in hardware.
    uint64_t apic_base_msr = read_msr(MSR_IA32_APIC_BASE);
    local_apic_base = (uint32_t *)(uintptr_t)(apic_base_msr & 0xFFFFF000ULL);
    if (!local_apic_base) {
        kprintf("ERROR: Failed to obtain a valid Local APIC base address from MSR\n");
        panic("lapic_init: local_apic_base is NULL");
    }
    apic_base_msr |= (1ULL << 11);  // Set the enable bit.
    write_msr(MSR_IA32_APIC_BASE, apic_base_msr);
    kprintf("Local APIC base=%p (from MSR), APIC enabled via MSR (0x%llx)\n", 
            local_apic_base, apic_base_msr);

    // Set the Spurious-Interrupt Vector Register (SVR).
    uint32_t svr = lapic_read(LAPIC_SVR);
    svr &= 0xFFFFFF00;   // Clear the vector portion.
    svr |= 0xFF;         // Set spurious vector = 0xFF.
    svr |= (1 << 8);     // Set APIC software-enable bit.
    lapic_write(LAPIC_SVR, svr);
    kprintf("LAPIC: SVR set to 0x%08x (spurious vector=0x%02X, APIC enabled)\n", svr, svr & 0xFF);

    // Mask LINT0 and LINT1.
    uint32_t lint0 = lapic_read(LAPIC_LVT_LINT0);
    uint32_t lint1 = lapic_read(LAPIC_LVT_LINT1);
    lint0 |= (1 << 16);
    lint1 |= (1 << 16);
    lapic_write(LAPIC_LVT_LINT0, lint0);
    lapic_write(LAPIC_LVT_LINT1, lint1);
    kprintf("LAPIC: LINT0/LINT1 masked (0x%08x, 0x%08x)\n", lint0, lint1);

    // Set TPR to 0 => unmask all interrupt priorities.
    lapic_set_tpr(0);
    kprintf("LAPIC: TPR set to 0 (no priority masking)\n");

    kprintf("Local APIC initialization complete.\n");
}

// Public function to send an End-of-Interrupt (EOI) signal if needed.
void apic_send_eoi_if_necessary(uint8_t vector) {
    if (!local_apic_base) {
        kprintf("ERROR: Cannot send EOI - local_apic_base is NULL\n");
        return;
    }
    // Calculate the In-Service Register (ISR) index and bit.
    uint32_t isr_reg_index = 0x100 + (vector / 32) * 0x10;
    uint32_t isr_bit = 1u << (vector % 32);
    uint32_t isr_val = lapic_read(isr_reg_index);
    if (isr_val & isr_bit) {
        lapic_write(LAPIC_EOI, 0);
    }
}

struct apic_device *get_apic_by_id(uint8_t id) {
    struct apic_device *apic;
    LIST_FOREACH(apic, &apics, list) {
      if (apic->id == id) {
        return apic;
      }
    }
    return NULL;
  }

void remap_apic_registers(void *data) {
    apic_base = vmap_phys(APIC_BASE_PA, 0, PAGE_SIZE, VM_WRITE | VM_NOCACHE | VM_EXEC, "apic");
  
    struct apic_device *apic;
    LIST_FOREACH(apic, &apics, list) {
      apic->address = apic_base;
    }
  }

void register_apic(uint8_t id) {
    if (get_apic_by_id(id) != NULL) {
      return;
    }
  
    kprintf("registering APIC[%d]\n", id);
    apic_reg_id_t id_reg = { .raw = lapic_read(APIC_ID) };
    if (id == id_reg.id) {
      register_init_address_space_callback(remap_apic_registers, NULL);
    }
  
    struct apic_device *apic = kmalloc(sizeof(struct apic_device));
    apic->id = id;
    apic->phys_addr = APIC_BASE_PA;
    apic->address = APIC_BASE_PA;
  
    num_apics++;
    LIST_ADD(&apics, apic, list);
  }

int apic_write_icr(uint32_t low, uint8_t dest_id) {
    uint64_t rflags = cpu_save_clear_interrupts();
    uint32_t icr_high = lapic_read(APIC_ICR_HIGH) & ICR_HIGH_REG_MASK;
    icr_high |= dest_id << 24;
    lapic_write(APIC_ICR_HIGH, icr_high);
  
    lapic_write(APIC_ICR_LOW, low);
    poll_icr_status();
    cpu_restore_interrupts(rflags);
    return 0;
  }