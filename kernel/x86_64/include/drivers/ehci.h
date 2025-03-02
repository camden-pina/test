#ifndef EHCI_H
#define EHCI_H

#include <stdint.h>
#include <drivers/pci.h>
#include <drivers/usb.h>

/* EHCI Capability Registers structure.
 * The capability registers are at the beginning of the EHCI MMIO region.
 */
typedef struct __attribute__((packed)) {
    uint8_t caplength;      // Capability register length (in bytes)
    uint8_t reserved;       // Reserved
    uint16_t hciversion;    // EHCI version (in BCD, e.g. 0x0200)
    uint32_t hcsparams;     // Structural parameters (number of ports, companion controllers, etc.)
    uint32_t hccparams;     // Capability parameters
    uint32_t hcsp_portroute; // Companion port routing (optional)
} ehci_cap_regs_t;

/* EHCI Operational Registers start at offset = caplength.
 * These offsets are defined relative to the base address plus caplength.
 */
#define EHCI_USBCMD_OFFSET              0x00
#define EHCI_USBSTS_OFFSET              0x04
#define EHCI_USBINTR_OFFSET             0x08
#define EHCI_FRINDEX_OFFSET             0x0C
#define EHCI_CTRLDSSEG_OFFSET           0x10
#define EHCI_PERIODICLISTBASE_OFFSET    0x14
#define EHCI_ASYNCLISTADDR_OFFSET       0x18

/* EHCI Register Bit Definitions */

/* USBCMD Register */
#define EHCI_USBCMD_RUN               (1 << 0)
#define EHCI_USBCMD_RESET             (1 << 1)

/* CONFIGFLAG Register (offset is not fixed; often located after other regs) */
#define EHCI_CONFIGFLAG_FLAG          1

/* PORTSC registers: They start at an offset that is also specified in the capability registers.
   For simplicity, we assume the first PORTSC is at offset 0x20 from the operational base.
   Some controllers may vary; a more robust implementation would parse HCSPARAMS. */
#define EHCI_PORTSC_OFFSET(port)      (0x20 + ((port) * 4))

/* PORTSC Bit Definitions */
#define EHCI_PORTSC_CONNECT           (1 << 0)   // Current Connect Status
#define EHCI_PORTSC_ENABLE            (1 << 1)   // Port Enable/Disable
#define EHCI_PORTSC_RESET_BIT         (1 << 8)   // Port Reset
#define EHCI_PORTSC_LOW_SPEED         (1 << 9)   // Low Speed Device
#define EHCI_PORT_OWNER               (1 << 24)  // Port Owner: when set, handed to companion controller


int ehci_init_controller(pci_device_t *pci_dev, usb_host_controller_t *hc);
int ehci_control_transfer(usb_host_controller_t *hc, uint8_t device_address,
                          usb_setup_packet_t *setup, void *buffer, int length);

#endif // EHCI_H
