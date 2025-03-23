.section .text.smpboot, "ax"
.global smpboot_start
.intel_syntax noprefix

.set SMPBOOT_START, 0x1000
.set SMPDATA_START, 0x2000

/* struct smpboot offsets */
.set SMP_LOCK,    0x00
.set SMP_GATE,    0x04
.set SMP_CPU_ID,  0x08
.set SMP_COUNT,   0x0C
.set SMP_PML4,    0x10
.set SMP_STACK,   0x18
.set SMP_PERCPU,  0x20

.set CODE_SEGMENT, 0x08
.set DATA_SEGMENT, 0x10

.set GSBASE_MSR,      0xC0000101
.set IA32_EFER_MSR,   0xC0000080

.extern ap_entry

/* ----------------- 16-Bit Code ----------------- */
smpboot_start:
.code16
ap_boot:
    lidt [SMPBOOT_START + (idt_desc - ap_boot)]

    /* Get APIC ID */
    mov eax, 0x1
    cpuid
    mov cl, 24
    shr ebx, cl
    and ebx, 0xFF

    /* Atomically increment CPU count */
    lock add dword ptr [SMPDATA_START + SMP_COUNT], 1

.acquire_lock:
    xor eax, eax                // Expected lock value = 0
    mov ecx, 1                  // New lock value = 1
    lock cmpxchg dword ptr [SMPDATA_START + SMP_LOCK], ecx
    jz .lock_acquired           // Jump if successfully acquired the lock

.spin_lock:
    pause
    cmp dword ptr [SMPDATA_START + SMP_GATE], 0
    je .acquire_lock            // Retry acquiring if gate is 0
    jmp .spin_lock              // Otherwise, continue spinning

.lock_acquired:
    /* Store APIC ID */
    mov byte ptr [SMPDATA_START + SMP_CPU_ID], bl

    /* Wait for BSP to release gate */
.wait_for_bsp:
    pause
    cmp dword ptr [SMPDATA_START + SMP_GATE], 1
    je .wait_for_bsp

    /* Enable PAE/PSE/PGE */
    mov eax, 0b10110000
    mov cr4, eax

    /* Load PML4 */
    mov eax, [SMPDATA_START + SMP_PML4]
    mov cr3, eax

    /* Enable Long Mode */
    mov ecx, IA32_EFER_MSR
    rdmsr
    or eax, 0x100
    wrmsr

    /* Enable paging & protection */
    mov eax, cr0
    and eax, 0x9FFFFFFF
    or eax, 0x80000001
    mov cr0, eax

    /* Load GDT */
    lgdt [SMPBOOT_START + (gdt_desc - ap_boot)]

    /* Jump to 64-bit code */
    .set ap_boot64_abs, SMPBOOT_START + (ap_boot64 - ap_boot)
    jmp CODE_SEGMENT:ap_boot64_abs

/* ----------------- 64-Bit Code ----------------- */
.code64
ap_boot64:
    mov ax, DATA_SEGMENT
    mov ss, ax
    xor ax, ax
    mov ds, ax
    mov es, ax

    /* Set stack and GS.base */
    mov rsp, [SMPDATA_START + SMP_STACK]
    mov rax, [SMPDATA_START + SMP_PERCPU]
    mov rdx, rax
    shr rdx, 32
    mov ecx, GSBASE_MSR
    wrmsr

    /* Release lock and gate */
    mov byte ptr [SMPDATA_START + SMP_GATE], 1
    mov byte ptr [SMPDATA_START + SMP_LOCK], 0

    /* Use 64-bit absolute address for higher-half kernel */
    movabs rax, offset ap_entry  /* Changed to movabs for 64-bit immediate */
    jmp rax

.global smpboot_end
smpboot_end:
/* ----------------- Trampoline Data ----------------- */
.align 4
gdt:
    .quad 0x0000000000000000    /* Null descriptor */
    .quad 0x00209A0000000000    /* Code descriptor */
    .quad 0x0000920000000000    /* Data descriptor */

.align 4
gdt_desc:
    .word gdt_desc - gdt - 1    /* Limit */
    .int SMPBOOT_START + (gdt - ap_boot) /* Base */

.align 4
idt_desc:
    .word 0                     /* Limit */
    .quad 0                     /* Base */

.att_syntax prefix