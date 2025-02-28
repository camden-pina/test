#include <interrupts/lapic.h>
#include <interrupts/ioapic.h>
#include <printf.h>
#include <panic.h>
#include <msr.h>
#include <io.h>

#define ALL_ONES (~0ull)
#define BOTTOM_N_BITS_OFF(n) (ALL_ONES << (n))
#define BOTTOM_N_BITS_ON(n) (~BOTTOM_N_BITS_OFF(n))
#define FIELD_MASK(bit_size, bit_offset) \
  (BOTTOM_N_BITS_ON(bit_size) << (bit_offset))

// Local APIC Register Address Map
#define LAPIC_ID                                  0x020
#define LAPIC_VER                                 0x030
#define LAPIC_TPR                                 0x080
#define LAPIC_APR                                 0x090
#define LAPIC_PPR                                 0x0A0
#define LAPIC_EOI                                 0x0B0
#define LAPIC_RRD                                 0x0C0
#define LAPIC_LOGICAL_DESTINATION_REGISER         0x0D0
#define LAPIC_DESTINATION_FORMAT_REGISTER         0x0E0
#define LAPIC_SVR                                 0x0F0
#define LAPIC_ISR                                 0x100
#define LAPIC_TMR                                 0x180
#define LAPIC_IRR                                 0x200   
#define LAPIC_ERROR_STATUS                        0x280
#define LAPIC_CMCI                                0x2F0
#define LAPIC_ICR                                 0x300
#define LAPIC_LVT_TIMER                           0x320
#define LAPIC_LVT_TEREMAL_SENSOR                  0x330
#define LAPIC_LVT_PERFORMANCE_MONITORING_COUNTERS 0x340
#define LAPIC_LVT_LINT0                           0x350
#define LAPIC_LVT_LINT1                           0x360
#define LAPIC_LVT_ERROR                           0x370
#define LAPIC_INITIAL_COUNT                       0x380 // For Timer
#define LAPIC_CURRENT_COUNT                       0x390 // For Timer
#define LAPIC_DIVIDE_CONFIG                       0x3E0 // For Timer

// Legacy
#define PIC1         0x20  // IO base address for master PIC
#define PIC2         0xA0  // IO base address for slave PIC
#define PIC1_COMMAND PIC1
#define PIC1_DATA    (PIC1 + 1)
#define PIC2_COMMAND PIC2
#define PIC2_DATA    (PIC2 + 1)
#define PIC_EOI      0x20  // End-of-interrupt command code

#define ICW1_ICW4    0x01  // ICW4 (not) needed
#define ICW1_INIT    0x10  // Initialization required

#define ICW4_8086    0x01  // 8086/88 (MCS-80/85) mode

#define ICW1         0x11
#define ICW4         0x01

// This gets set from an MSR
static unsigned int* local_apic_base = (void*)0;

void apic_write(int index, unsigned int value)
{
    local_apic_base[index * 4] = value;  // Registers are 16 bytes wide
}

unsigned int apic_read(int index)
{
    return *(local_apic_base + index * 4);  // Registers are 16 bytes wide
}

static void disable_pic(void)
{
    // Set ICW1
    io_write_8(PIC1_COMMAND, ICW1_INIT + ICW1_ICW4);
    io_write_8(PIC2_COMMAND, ICW1_INIT + ICW1_ICW4);

    // Set ICW2 (IRQ base offsets)
    io_write_8(PIC1_DATA, 0xe0);
    io_write_8(PIC2_DATA, 0xe8);

    // Set ICW3
    io_write_8(PIC1_DATA, 4);
    io_write_8(PIC2_DATA, 2);

    // Set ICW4
    io_write_8(PIC1_DATA, ICW4_8086);
    io_write_8(PIC2_DATA, ICW4_8086);

    // Mask all interrupts
    io_write_8(PIC1_DATA, 0xff);
    io_write_8(PIC2_DATA, 0xff);
}

void apic_init(void)
{
    kprintf("Initializing APIC\n\r");
    disable_pic();

    if (!load_ioapic_address()) {
        kprintf("\nCould not find I/O APIC! This is currently required.\n");
    }

// Get the Local APIC Base
    unsigned long long apic_msr = read_msr(MSR_IA32_APIC_BASE);

    local_apic_base = (unsigned int*)(apic_msr & 0xffffff000);  // Store Local APIC Base Address

    // This Bit may be reserved in future processors
    apic_msr |= (1 << 11);  // Set "APIC Global Enable" bit
    
    kprintf("APIC MSR: %x\n\r", apic_msr);
    write_msr(MSR_IA32_APIC_BASE, apic_msr);

// Enable the Local APIC
    unsigned int spurious_irq_num = apic_read(LAPIC_SVR);
    spurious_irq_num |= (1 << 8);   //  Enable "APIC Software Enable/Disable" bit
    kprintf("Suprious_IRQ_Num: %x\n\r", spurious_irq_num);
    apic_write(LAPIC_SVR, spurious_irq_num);
}

static inline void apic_send_eoi(void)
{
    apic_write(0x0b, 1);
}

void apic_send_eoi_if_necessary(unsigned char interrupt_vector) {
    int apic_register_index = 0x10 + interrupt_vector / 32;
    unsigned int isr_value = apic_read(apic_register_index);

    if ((isr_value & FIELD_MASK(1, interrupt_vector % 32)) != 0)
        apic_send_eoi();
}