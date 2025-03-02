#ifndef EHCI_QTD_H
#define EHCI_QTD_H

#include <stdint.h>

/*
 * EHCI Queue Transfer Descriptor (qTD) Structure
 *
 * This simplified structure is 32-byte aligned and contains:
 *   - next_qtd: Pointer to the next qTD in the chain (or termination flag 0x00000001)
 *   - alternate_next_qtd: Alternate pointer (unused in this simple implementation)
 *   - token: Contains the transfer length (bits 16-30) and the Active bit (bit 31) plus error status.
 *   - buffer[5]: Buffer pointers for data transfer.
 *
 * The structure size is padded to 64 bytes.
 */
typedef struct ehci_qtd {
    uint32_t next_qtd;
    uint32_t alternate_next_qtd;
    uint32_t token;
    uint32_t buffer[5];
    uint32_t reserved[4];  // Pad to 64 bytes total (if needed)
} __attribute__((aligned(32))) ehci_qtd;

#endif // EHCI_QTD_H
