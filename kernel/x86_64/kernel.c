#include <kernel.h>
#include <descriptor_tables/gdt.h>
#include <descriptor_tables/idt.h>
#include <descriptor_tables/isr.h>
#include <interrupts/lapic.h>
#include <acpi/acpi.h>
#include <mm/pmm.h>
#include <mm/vmem.h>
#include <printf.h>

#include "ps2_mouse.h"

#include <stdarg.h>

#include <io.h>
#include <string.h>
#include <8250.h>
#include <lai/host.h>
#include <drivers/pci.h>
#include <drivers/usb.h>
#include <drivers/usb_keyboard.h>
#include <mm/pgtable.h>
#include <msi.h>
#include <init.h>
#include <workqueue.h>
#include <thread.h>
#include <mm/vmem.h>
#include <smpboot.h>
#include <sched.h>
#include <cpu/cpu.h>
#include <wm/wm.h>
#include <storage/ahci/ahci.h>
#include <storage/block_device.h>
#include <storage/fs/fat32.h>
#include <storage/partition.h>
#include <panic.h>
#include <cpu/cpu_features.h>

#include <task.h>
#include <sched.h>
#include <syscall.h>

static unsigned char SCAN_CODE_MAPPING[] = "\x00""\x1B""1234567890-=""\x08""\tqwertyuiop[]\n\0asdfghjkl;'`\0\\zxcvbnm,./\0*\0 \0\0\0\0\0\0\0\0\0\0\0\0\0-456+1230.\0\0\0\0\0";

static void keyboard_isr(uint64_t vector, uint32_t error)
{
	unsigned char scancode = inb(0x60);	// Read Scancode

	// Reset Keyboard Controller
	unsigned char a = inb(0x61);
	a |= 0x82;
	outb(0x61, a);
	a &= 0x7f;
	outb(0x61, a);

	if (scancode & 0x80)
	{
		scancode = scancode & ~0x80;

		unsigned char pressed_char = SCAN_CODE_MAPPING[scancode];

		kprintf("fo");
	}
}

static void ps2_keyboard_init(void)
{
	kprintf("Initializing Keyboard\n\r");
	register_interrupt_handler(33, keyboard_isr);
	ioapic_map_irq(1, 33);
	kprintf("DS");
}

extern uint64_t kernel_start;
extern uint64_t kernel_end;

// Inline functions to perform port I/O operations.
static inline void serial_outb(uint16_t port, uint8_t data) {
    __asm__ volatile ("outb %0, %1" : : "a"(data), "Nd"(port));
}

static inline uint8_t serial_inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

boot_info_v2_t __boot_data *boot_info_v2;

bool is_smp_enabled = true;

void kern_main(boot_info_v2_t* boot_hdr)
{
	serial_port_init(COM1_PORT);

	kprintf_early_init();

	pmm_init(boot_info_v2->mem_map.map, boot_info_v2->mem_map.size, sizeof(memory_map_entry_t));
	do_static_initializers();
	kprintf_init();

    print_cpu_features();

	__asm__ volatile("cli");
	gdt_init();
	idt_init();
	acpi_init(boot_info_v2->acpi_ptr);
	lapic_init();
	ioapic_init();
    syscall_init();
	__asm__ volatile("sti");
	vmem_init();

	kprintf("ModernOS (C)\n\n\r");

	// smp_init();

	pci_init();
	// usb_init();
	// usb_keyboard_init();
	// usb_print_devices();
	ps2_mouse_init();

	kprintf("Copyright (C) Ideal Technologies Inc.\n\r");

	vm_print_address_space();

	ahci_init();
    kprintf("Kernel: File loaded successfully. Continuing boot...\n");

	// wm_init();
	// wm_run();

    scheduler_init();

    struct task_struct *init_task = create_user_process("/init.o", NULL, NULL);\
    if (init_task) {
        init_task->state = TASK_READY;
        enqueue_task(init_task, get_cpu_id());
    kprintf("FD");
    } else {
        panic("Failed to create init process!\n");
    }

	while (1)
	{
        schedule();

	}
}

__used void ap_main() {
	// int cpu_id = get_cpu_id();
	// sched_init_cpu(cpu_id);
	// sched_start_cpu(cpu_id);
	while (1) {
	}
}








    /*
	int initial_count = get_block_device_count();
    kprintf("Initially we have %d block devices.\n", initial_count);

    if (initial_count <= 0) {
        kprintf("No devices found!\n");
        return;
    }

    // Suppose device #0 is our main disk
    block_device_t *raw_disk = get_block_device(0);
    if (!raw_disk) {
        // ...
    }
    kprintf("Kernel: Using raw device '%s' with %llu sectors.\n",
            raw_disk->name, (unsigned long long)raw_disk->sector_count);

    // Scan partitions
    int pcount = partition_scan(raw_disk);
    if (pcount < 0) {
        panic("No valid partition table or parse error.\n");
    }
    kprintf("Kernel: partition_scan found %d partition(s).\n", pcount);

    if (pcount == 0) {
        kprintf("No partitions found.\n");
        return;
    }

    // After scanning, let's see how many devices exist
    int after_count = get_block_device_count();
    kprintf("Now we have %d devices total.\n", after_count);

    // The newly created partition devices have indexes from [initial_count..after_count-1].
    // If we found 1 partition, that means there's exactly 1 new device, which should be 'initial_count'.
    int new_device_index = after_count - 1;  // the last device created
	block_list();
    block_device_t *part_dev = get_block_device(new_device_index);
    if (!part_dev) {
        panic("Couldn't get the newly registered partition device!\n");
    }
    kprintf("Kernel: Attempting to mount FAT32 on device #%d ('%s').\n",
            new_device_index, part_dev->name);

    if (fat32_mount(part_dev) != 0) {
        kprintf("Kernel: Failed to mount FAT32 on device '%s'\n", part_dev->name);
        return;
    }
    kprintf("Kernel: FAT32 filesystem mounted successfully.\n");

    // Try opening a file
    file_t *fp = fat32_open("hello.txt", false);
    if (!fp) {
        kprintf("Kernel: Failed to open /hello.txt\n");
        return;
    }
    char file_buf[1024];
    int br = fat32_read(fp, file_buf, sizeof(file_buf)-1);
    if (br < 0) {
        kprintf("Kernel: Error reading /hello.txt\n");
        fat32_close(fp);
        return;
    }
    file_buf[br] = '\0';
    kprintf("Kernel: /hello.txt =>\n%s\n", file_buf);
    fat32_close(fp);
    */

