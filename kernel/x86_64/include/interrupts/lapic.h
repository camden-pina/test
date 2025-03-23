#ifndef LAPIC_H
#define LAPIC_H

#include <stdint.h>
#include <stdbool.h>

// PIC ports and initialization control words.
#define PIC1_CMD    0x20
#define PIC1_DATA   0x21
#define PIC2_CMD    0xA0
#define PIC2_DATA   0xA1

#define ICW1_INIT   0x10  // Start initialization
#define ICW1_ICW4   0x01  // Expect ICW4
#define ICW4_8086   0x01  // 8086 mode

// Local APIC register offsets.
#define LAPIC_ID          0x020
#define LAPIC_VER         0x030
#define LAPIC_TPR         0x080
#define LAPIC_EOI         0x0B0
#define LAPIC_SVR         0x0F0
#define LAPIC_LVT_TIMER   0x320
#define LAPIC_LVT_LINT0   0x350
#define LAPIC_LVT_LINT1   0x360
#define LAPIC_LVT_ERROR   0x370

typedef enum {
    APIC_ID_MSR            = 0x802,
    APIC_VERSION_MSR       = 0x803,
    APIC_TPR_MSR           = 0x808,
    APIC_PPR_MSR           = 0x80A,
    APIC_EOI_MSR           = 0x80B,
    APIC_LDR_MSR           = 0x80D,
    APIC_SVR_MSR           = 0x80F,
    APIC_ESR_MSR           = 0x828,
    APIC_LVT_CMCI_MSR      = 0x82F,
    APIC_ICR_MSR           = 0x830,
    APIC_LVT_TIMER_MSR     = 0x832,
    APIC_LVT_THERMAL_MSR   = 0x833,
    APIC_LVT_PERFC_MSR     = 0x834,
    APIC_LVT_LINT0_MSR     = 0x835,
    APIC_LVT_LINT1_MSR     = 0x836,
    APIC_LVT_ERROR_MSR     = 0x837,
    APIC_INITIAL_COUNT_MSR = 0x838,
    APIC_CURRENT_COUNT_MSR = 0x839,
    APIC_TIMER_DCR_MSR     = 0x83E,
    APIC_SELF_IPI_MSR      = 0x83F,
  } apicx2_reg_t;

// delivery mode
#define APIC_DM_FIXED       0x00000
#define APIC_DM_LOWEST      0x00100
#define APIC_DM_SMI         0x00200
#define APIC_DM_NMI         0x00400
#define APIC_DM_INIT        0x00500
#define APIC_DM_STARTUP     0x00600
#define APIC_DM_EXTINT      0x00700
// level
#define APIC_LVL_DEASSERT   0x00000
#define APIC_LVL_ASSERT     0x04000
// trigger mode
#define APIC_TM_EDGE        0x00000
#define APIC_TM_LEVEL       0x08000
// dest shorthand
#define APIC_DS_SELF        0x40000
#define APIC_DS_ALLINC      0x80000
#define APIC_DS_ALLBUT      0xC0000


#define APIC_FIXED        0
#define APIC_LOWEST_PRIOR 1
#define APIC_SMI          2
#define APIC_NMI          4
#define APIC_INIT         5
#define APIC_START_UP     6
#define APIC_ExtINT       7

#define APIC_DEST_TARGET        0
#define APIC_DEST_SELF          1
#define APIC_DEST_ALL_INCL_SELF 2
#define APIC_DEST_ALL_EXCL_SELF 3

#define APIC_DEST_PHYSICAL 0
#define APIC_DEST_LOGICAL  1

#define APIC_IDLE    0
#define APIC_PENDING 1

#define APIC_DEASSERT 0
#define APIC_ASSERT   1

#define APIC_EDGE  0
#define APIC_LEVEL 1

#define APIC_ONE_SHOT     0
#define APIC_PERIODIC     1
#define APIC_TSC_DEADLINE 2

#define APIC_CLUSTER_MODEL 0x0
#define APIC_FLAT_MODEL    0xF

#define APIC_UNMASK 0
#define APIC_MASK   1

#define APIC_DIVIDE_2   0
#define APIC_DIVIDE_4   1
#define APIC_DIVIDE_8   2
#define APIC_DIVIDE_16  3
#define APIC_DIVIDE_32  4
#define APIC_DIVIDE_64  5
#define APIC_DIVIDE_128 6
#define APIC_DIVIDE_1   7

typedef union {
    uint32_t raw;
    struct {
      uint32_t : 24;
      uint32_t id : 8;
    };
  } apic_reg_id_t;
  
  typedef union {
    uint32_t raw;
    struct {
      uint32_t version : 8;
      uint32_t : 8;
      uint32_t max_lvt_entry : 8;
      uint32_t can_suppress_eoi : 1;
      uint32_t : 7;
    };
  } apic_reg_version_t;

typedef union {
    uint32_t raw;
    struct {
      uint32_t vector : 8;
      uint32_t : 4;
      uint32_t deliv_status : 1;
      uint32_t : 3;
      uint32_t mask : 1;
      uint32_t timer_mode : 2;
      uint32_t : 13;
    };
  } apic_reg_lvt_timer_t;

  #define apic_icr_status(low) \
  (((low) >> 12) & 1)

// Initialize Local APIC and disable legacy PIC.
void lapic_init(void);

// Send End-of-Interrupt signal if needed.
void apic_send_eoi_if_necessary(uint8_t vector);

// Add this prototype:
void lapic_set_tpr(uint8_t priority);

uint32_t lapic_read(uint32_t reg_offset);

void lapic_write(uint32_t reg_offset, uint32_t value);

int apic_write_icr(uint32_t low, uint8_t dest_id);

void apic_udelay(uint64_t us);
void apic_mdelay(uint64_t ms);

void register_apic(uint8_t id);

#endif // LAPIC_H
