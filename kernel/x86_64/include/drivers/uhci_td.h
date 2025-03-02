#ifndef UHCI_TD_H
#define UHCI_TD_H

#include <stdint.h>

/*
 * UHCI Transfer Descriptor (TD) Structure
 *
 * This simplified TD is 16-byte aligned and contains:
 *   - link: Pointer to the next TD in the chain or termination (0x00000001).
 *   - ctrl_status: Control/status bits (we assume bit 31 is the Active bit).
 *   - token: Encodes transfer length in bits [16:30].
 *   - buffer: Physical address of the data buffer.
 *   - done: A flag (set by the interrupt handler) indicating completion.
 *
 * In a real implementation, additional error bits and status fields would be used.
 */
typedef struct uhci_td {
    uint32_t link;
    uint32_t ctrl_status;
    uint32_t token;
    uint32_t buffer;
    volatile uint32_t done;  // 0 = not done, 1 = completed
} __attribute__((aligned(16))) uhci_td;

#endif // UHCI_TD_H
