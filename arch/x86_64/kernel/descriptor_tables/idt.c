#include <descriptor_tables/idt.h>
#include <descriptor_tables/gdt.h>

#include <stdio.h>

#define IDT_GATE_COUNT 256

#define DESCRIPTOR_PRIV_0 0		// kernel
#define DESCRIPTOR_PRIV_1 1		// unused
#define DESCRIPTOR_PRIV_2 2		// unused
#define DESCRIPTOR_PRIV_3 3		// userspace

#define INTERRUPT 0x0e
#define TRAP_GATE 0x0f

typedef struct gate_descriptor_t {
	unsigned short offset_0_15;
	unsigned short selector;
	unsigned char  ist : 3;
	unsigned char reserved0 : 5;
	unsigned char type : 4;
	unsigned char zero : 1;
	unsigned char dpl : 2;
	unsigned char p : 1;
	unsigned short offset_16_31;
	unsigned int offset_63_32;
	unsigned int reserved1;
} __attribute__((packed)) gate_descriptor_t;

/*
 * The structure of the idtr pointer
 * To be loaded.
 */
struct IDTR {
	unsigned short limit;
	unsigned long long base;
} __attribute__((packed)) IDTR;

static gate_descriptor_t IDT[IDT_GATE_COUNT] __attribute__((aligned(8)));

static void idt_set_entry(unsigned short index, unsigned long long isrAddr, unsigned short selector, unsigned char dpl, unsigned short type, unsigned char ist) {
	//memset(&IDT[index], 0, sizeof(gate_descriptor_t));
	IDT[index].ist = ist;
	IDT[index].dpl = dpl;
	IDT[index].selector = selector;
	IDT[index].type = type;
	IDT[index].reserved0 = 0;
	IDT[index].reserved1 = 0;
	//IDT[index].p = 1;
	IDT[index].zero = 0;

	IDT[index].offset_0_15 = (isrAddr) & 0x0000ffff;
	IDT[index].offset_16_31 = (isrAddr >> 16) & 0x0000ffff;
	IDT[index].offset_63_32 = (isrAddr >> 32) & 0xffffffff;
}

extern void idt_flush(void);

void idt_init(void) {
	printf("Initializing IDT...\n\r");
	memset(&IDT, 0, (sizeof(gate_descriptor_t) * IDT_GATE_COUNT));

	for (int idx = 0; idx < IDT_GATE_COUNT; idx++)
		IDT[idx].p = 1;

	// ISR
	idt_set_entry(0, (unsigned long long)isr0, KERNEL_CS, DESCRIPTOR_PRIV_0, INTERRUPT, 0);
	idt_set_entry(1, (unsigned long long)isr1, KERNEL_CS, DESCRIPTOR_PRIV_0, INTERRUPT, 0);
	idt_set_entry(2, (unsigned long long)isr2, KERNEL_CS, DESCRIPTOR_PRIV_0, INTERRUPT, 0);
	idt_set_entry(3, (unsigned long long)isr3, KERNEL_CS, DESCRIPTOR_PRIV_0, INTERRUPT, 0);
	idt_set_entry(4, (unsigned long long)isr4, KERNEL_CS, DESCRIPTOR_PRIV_0, INTERRUPT, 0);
	idt_set_entry(5, (unsigned long long)isr5, KERNEL_CS, DESCRIPTOR_PRIV_0, INTERRUPT, 0);
	idt_set_entry(7, (unsigned long long)isr7, KERNEL_CS, DESCRIPTOR_PRIV_0, INTERRUPT, 0);
	idt_set_entry(6, (unsigned long long)isr6, KERNEL_CS, DESCRIPTOR_PRIV_0, INTERRUPT, 0);
	idt_set_entry(8, (unsigned long long)isr8, KERNEL_CS, DESCRIPTOR_PRIV_0, INTERRUPT, 0);
	idt_set_entry(9, (unsigned long long)isr9, KERNEL_CS, DESCRIPTOR_PRIV_0, INTERRUPT, 0);
	idt_set_entry(10, (unsigned long long)isr10, KERNEL_CS, DESCRIPTOR_PRIV_0, INTERRUPT, 0);
	idt_set_entry(11, (unsigned long long)isr11, KERNEL_CS, DESCRIPTOR_PRIV_0, INTERRUPT, 0);
	idt_set_entry(12, (unsigned long long)isr12, KERNEL_CS, DESCRIPTOR_PRIV_0, INTERRUPT, 0);
	idt_set_entry(13, (unsigned long long)isr13, KERNEL_CS, DESCRIPTOR_PRIV_0, INTERRUPT, 0);
	idt_set_entry(14, (unsigned long long)isr14, KERNEL_CS, DESCRIPTOR_PRIV_0, INTERRUPT, 0);

	idt_set_entry(16, (unsigned long long)isr16, KERNEL_CS, DESCRIPTOR_PRIV_0, INTERRUPT, 0);
	idt_set_entry(17, (unsigned long long)isr17, KERNEL_CS, DESCRIPTOR_PRIV_0, INTERRUPT, 0);
	idt_set_entry(18, (unsigned long long)isr18, KERNEL_CS, DESCRIPTOR_PRIV_0, INTERRUPT, 0);
	idt_set_entry(19, (unsigned long long)isr19, KERNEL_CS, DESCRIPTOR_PRIV_0, INTERRUPT, 0);
	idt_set_entry(20, (unsigned long long)isr20, KERNEL_CS, DESCRIPTOR_PRIV_0, INTERRUPT, 0);

	idt_set_entry(30, (unsigned long long)isr30, KERNEL_CS, DESCRIPTOR_PRIV_0, INTERRUPT, 0);

	// IRQ
	idt_set_entry(32, (unsigned long long)isr32, KERNEL_CS, DESCRIPTOR_PRIV_0, INTERRUPT, 0);
	idt_set_entry(33, (unsigned long long)isr33, KERNEL_CS, DESCRIPTOR_PRIV_0, INTERRUPT, 0);
	idt_set_entry(34, (unsigned long long)isr34, KERNEL_CS, DESCRIPTOR_PRIV_0, INTERRUPT, 0);
	idt_set_entry(35, (unsigned long long)isr35, KERNEL_CS, DESCRIPTOR_PRIV_0, INTERRUPT, 0);
	idt_set_entry(36, (unsigned long long)isr36, KERNEL_CS, DESCRIPTOR_PRIV_0, INTERRUPT, 0);
	idt_set_entry(37, (unsigned long long)isr38, KERNEL_CS, DESCRIPTOR_PRIV_0, INTERRUPT, 0);
	idt_set_entry(38, (unsigned long long)isr38, KERNEL_CS, DESCRIPTOR_PRIV_0, INTERRUPT, 0);
	idt_set_entry(39, (unsigned long long)isr39, KERNEL_CS, DESCRIPTOR_PRIV_0, INTERRUPT, 0);
	idt_set_entry(40, (unsigned long long)isr40, KERNEL_CS, DESCRIPTOR_PRIV_0, INTERRUPT, 0);
	idt_set_entry(41, (unsigned long long)isr41, KERNEL_CS, DESCRIPTOR_PRIV_0, INTERRUPT, 0);
	idt_set_entry(42, (unsigned long long)isr42, KERNEL_CS, DESCRIPTOR_PRIV_0, INTERRUPT, 0);
	idt_set_entry(43, (unsigned long long)isr43, KERNEL_CS, DESCRIPTOR_PRIV_0, INTERRUPT, 0);
	idt_set_entry(44, (unsigned long long)isr44, KERNEL_CS, DESCRIPTOR_PRIV_0, INTERRUPT, 0);
	idt_set_entry(45, (unsigned long long)isr45, KERNEL_CS, DESCRIPTOR_PRIV_0, INTERRUPT, 0);
	idt_set_entry(46, (unsigned long long)isr46, KERNEL_CS, DESCRIPTOR_PRIV_0, INTERRUPT, 0);
	idt_set_entry(47, (unsigned long long)isr47, KERNEL_CS, DESCRIPTOR_PRIV_0, INTERRUPT, 0);

	idt_set_entry(128, (unsigned long long)isr128, KERNEL_CS, DESCRIPTOR_PRIV_3, TRAP_GATE, 0);

	IDTR.limit = sizeof(IDT) - 1;
	IDTR.base = (unsigned long long)&IDT[0];

	asm volatile("lidt (%0)" : : "r"(&IDTR) : "memory");

	//idt_flush();
}