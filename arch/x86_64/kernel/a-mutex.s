	.file	"mutex.c"
	.text
.Ltext0:
	.file 0 "/home/camdenpina/Music/ModernOS/arch/x86_64/kernel" "mutex.c"
	.type	spinlock_enter, @function
spinlock_enter:
.LFB0:
	.file 1 "mutex.c"
	.loc 1 31 37
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	pushq	%rbx
	subq	$8, %rsp
	.cfi_offset 3, -24
.LBB2:
	.loc 1 32 72
/APP
# 32 "mutex.c" 1
	mov %rax, gs:$24
# 0 "" 2
/NO_APP
	movq	%rax, %rbx
	.loc 1 32 12
	movq	%rbx, %rax
.LBE2:
	.loc 1 32 21
	testq	%rax, %rax
	setne	%al
	.loc 1 32 9
	movzbl	%al, %eax
	.loc 1 32 8 discriminator 1
	testq	%rax, %rax
	je	.L2
.LBB3:
	.loc 1 33 74
/APP
# 33 "mutex.c" 1
	mov %rax, gs:$24
# 0 "" 2
/NO_APP
	movq	%rax, %rbx
	.loc 1 33 12
	movq	%rbx, %rax
.LBE3:
	.loc 1 33 28
	addq	$64, %rax
	.loc 1 33 9
	lock addl	$1, (%rax)
.L2:
	.loc 1 35 5
	movabsq	$critical_enter, %rax
	call	*%rax
.LVL0:
	.loc 1 36 1
	nop
	movq	-8(%rbp), %rbx
	leave
	.cfi_restore 6
	.cfi_restore 3
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE0:
	.size	spinlock_enter, .-spinlock_enter
	.section	.rodata
.LC0:
	.string	"mutex.c"
	.align 8
.LC1:
	.string	"assertion failed: spinlock_exit() with no spin locks held, file %s, line %d"
	.text
	.type	spinlock_exit, @function
spinlock_exit:
.LFB1:
	.loc 1 38 36
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	pushq	%rbx
	subq	$8, %rsp
	.cfi_offset 3, -24
.LBB4:
	.loc 1 39 72
/APP
# 39 "mutex.c" 1
	mov %rax, gs:$24
# 0 "" 2
/NO_APP
	movq	%rax, %rbx
	.loc 1 39 12
	movq	%rbx, %rax
.LBE4:
	.loc 1 39 21
	testq	%rax, %rax
	setne	%al
	.loc 1 39 9
	movzbl	%al, %eax
	.loc 1 39 8 discriminator 1
	testq	%rax, %rax
	je	.L4
.LBB5:
	.loc 1 40 60
/APP
# 40 "mutex.c" 1
	mov %rax, gs:$24
# 0 "" 2
/NO_APP
	movq	%rax, %rbx
	.loc 1 40 12
	movq	%rbx, %rax
.LBE5:
	.loc 1 40 20
	movl	64(%rax), %eax
	.loc 1 40 12
	testl	%eax, %eax
	jg	.L5
	.loc 1 40 39 discriminator 1
	movl	$40, %edx
	movabsq	$.LC0, %rax
	movq	%rax, %rsi
	movabsq	$.LC1, %rax
	movq	%rax, %rdi
	movl	$0, %eax
	movabsq	$panic, %rcx
	call	*%rcx
.LVL1:
.L5:
.LBB6:
	.loc 1 41 74
/APP
# 41 "mutex.c" 1
	mov %rax, gs:$24
# 0 "" 2
/NO_APP
	movq	%rax, %rbx
	.loc 1 41 12
	movq	%rbx, %rax
.LBE6:
	.loc 1 41 28
	addq	$64, %rax
	.loc 1 41 9
	lock subl	$1, (%rax)
.L4:
	.loc 1 43 5
	movabsq	$critical_exit, %rax
	call	*%rax
.LVL2:
	.loc 1 44 1
	nop
	movq	-8(%rbp), %rbx
	leave
	.cfi_restore 6
	.cfi_restore 3
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE1:
	.size	spinlock_exit, .-spinlock_exit
	.globl	_mtx_opts_to_lockobject_flags
	.type	_mtx_opts_to_lockobject_flags, @function
_mtx_opts_to_lockobject_flags:
.LFB2:
	.loc 1 46 55
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	subq	$24, %rsp
	movl	%edi, -20(%rbp)
	.loc 1 47 14
	movl	$4096, -4(%rbp)
	.loc 1 49 14
	movl	-20(%rbp), %eax
	andl	$1, %eax
	.loc 1 49 8
	testl	%eax, %eax
	je	.L7
	.loc 1 50 15
	orl	$1, -4(%rbp)
	.loc 1 51 14
	andl	$-5, -20(%rbp)
	jmp	.L8
.L7:
	.loc 1 53 15
	orl	$2, -4(%rbp)
.L8:
	.loc 1 56 34
	movl	-20(%rbp), %eax
	sall	$7, %eax
	andl	$256, %eax
	.loc 1 56 11
	orl	%eax, -4(%rbp)
	.loc 1 57 34
	movl	-20(%rbp), %eax
	sall	$6, %eax
	andl	$512, %eax
	.loc 1 57 11
	orl	%eax, -4(%rbp)
	.loc 1 58 34
	movl	-20(%rbp), %eax
	sall	$8, %eax
	andl	$1024, %eax
	.loc 1 58 11
	orl	%eax, -4(%rbp)
	.loc 1 60 12
	movl	-4(%rbp), %eax
	.loc 1 61 1
	leave
	.cfi_restore 6
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE2:
	.size	_mtx_opts_to_lockobject_flags, .-_mtx_opts_to_lockobject_flags
	.globl	_mtx_init
	.type	_mtx_init, @function
_mtx_init:
.LFB3:
	.loc 1 63 61
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	subq	$24, %rsp
	movq	%rdi, -8(%rbp)
	movl	%esi, -12(%rbp)
	movq	%rdx, -24(%rbp)
	.loc 1 64 18
	movq	-8(%rbp), %rax
	movq	-24(%rbp), %rdx
	movq	%rdx, (%rax)
	.loc 1 65 21
	movl	-12(%rbp), %eax
	movl	%eax, %edi
	movabsq	$_mtx_opts_to_lockobject_flags, %rax
	call	*%rax
.LVL3:
	.loc 1 65 19 discriminator 1
	movq	-8(%rbp), %rdx
	movl	%eax, 8(%rdx)
	.loc 1 66 18
	movq	-8(%rbp), %rax
	movl	$0, 12(%rax)
	.loc 1 67 19
	movq	-8(%rbp), %rax
	movq	$0, 16(%rax)
	.loc 1 68 1
	nop
	leave
	.cfi_restore 6
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE3:
	.size	_mtx_init, .-_mtx_init
	.section	.rodata
	.align 8
.LC2:
	.string	"assertion failed: _mtx_destroy() on locked mutex, file %s, line %d"
	.text
	.globl	_mtx_owner
	.type	_mtx_owner, @function
_mtx_owner:
.LFB4:
	.loc 1 70 34
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	subq	$32, %rsp
	movq	%rdi, -24(%rbp)
	.loc 1 71 15
	movq	-24(%rbp), %rax
	movq	16(%rax), %rax
	movq	%rax, -8(%rbp)
	.loc 1 72 8
	cmpq	$2, -8(%rbp)
	jne	.L12
	.loc 1 72 30 discriminator 1
	movl	$72, %edx
	movabsq	$.LC0, %rax
	movq	%rax, %rsi
	movabsq	$.LC2, %rax
	movq	%rax, %rdi
	movl	$0, %eax
	movabsq	$panic, %rcx
	call	*%rcx
.LVL4:
.L12:
	.loc 1 73 38
	movq	-8(%rbp), %rax
	andq	$-8, %rax
	.loc 1 74 1
	leave
	.cfi_restore 6
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE4:
	.size	_mtx_owner, .-_mtx_owner
	.section	.rodata
	.align 8
.LC3:
	.string	"assertion failed: _mtx_spin_lock() on destroyed mutex, %s:%d, file %s, line %d"
	.align 8
.LC4:
	.string	"assertion failed: _mtx_spin_lock() on non-spin mutex, %s:%d, file %s, line %d"
	.align 8
.LC5:
	.string	"assertion failed: _mtx_wait_lock() on non-recursive mutex, %s:%d, file %s, line %d"
.LC6:
	.string	"spin mutex deadlock, %s:%d"
	.text
	.globl	_mtx_spin_lock
	.type	_mtx_spin_lock, @function
_mtx_spin_lock:
.LFB5:
	.loc 1 76 61
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	pushq	%rbx
	subq	$88, %rsp
	.cfi_offset 3, -24
	movq	%rdi, -72(%rbp)
	movq	%rsi, -80(%rbp)
	movl	%edx, -84(%rbp)
	.loc 1 77 42
	movq	-72(%rbp), %rax
	movq	16(%rax), %rax
	.loc 1 77 54
	andq	$-8, %rax
	.loc 1 77 13
	movq	%rax, -24(%rbp)
	.loc 1 78 12
	movq	-72(%rbp), %rax
	movq	16(%rax), %rax
	.loc 1 78 6
	cmpq	$2, %rax
	jne	.L15
	.loc 1 78 33 discriminator 1
	movl	-84(%rbp), %edx
	movq	-80(%rbp), %rax
	movl	$78, %r8d
	movabsq	$.LC0, %rcx
	movq	%rax, %rsi
	movabsq	$.LC3, %rax
	movq	%rax, %rdi
	movl	$0, %eax
	movabsq	$panic, %r9
	call	*%r9
.LVL5:
.L15:
	.loc 1 79 22
	movq	-72(%rbp), %rax
	movl	8(%rax), %eax
	.loc 1 79 30
	movzbl	%al, %eax
	.loc 1 79 6
	cmpl	$1, %eax
	je	.L16
	.loc 1 79 54 discriminator 1
	movl	-84(%rbp), %edx
	movq	-80(%rbp), %rax
	movl	$79, %r8d
	movabsq	$.LC0, %rcx
	movq	%rax, %rsi
	movabsq	$.LC4, %rax
	movq	%rax, %rdi
	movl	$0, %eax
	movabsq	$panic, %r9
	call	*%r9
.LVL6:
.L16:
	.loc 1 81 3
	movabsq	$spinlock_enter, %rax
	call	*%rax
.LVL7:
.LBB7:
	.loc 1 82 79
/APP
# 82 "mutex.c" 1
	mov %rax, gs:$40
# 0 "" 2
/NO_APP
	movq	%rax, %rbx
	.loc 1 82 6
	movq	%rbx, %rax
.LBE7:
	.loc 1 82 15
	testq	%rax, %rax
	setne	%al
	.loc 1 82 7
	movzbl	%al, %eax
	.loc 1 82 6 discriminator 1
	testq	%rax, %rax
	je	.L17
	.loc 1 82 9 discriminator 1
	movq	-72(%rbp), %rax
.LBB8:
	.loc 1 82 83 discriminator 1
/APP
# 82 "mutex.c" 1
	mov %rdx, gs:$40
# 0 "" 2
/NO_APP
	movq	%rdx, %rbx
	.loc 1 82 6 discriminator 1
	movq	%rbx, %rdx
.LBE8:
	.loc 1 82 9 discriminator 1
	movq	%rdx, %rdi
	movl	-84(%rbp), %ecx
	movq	-80(%rbp), %rdx
	movl	%ecx, %r8d
	movq	%rdx, %rcx
	movl	$0, %edx
	movq	%rax, %rsi
	movabsq	$lock_claim_list_add, %rax
	call	*%rax
.LVL8:
.L17:
.LBB9:
	.loc 1 84 52
/APP
# 84 "mutex.c" 1
	mov %rax, gs:$24
# 0 "" 2
/NO_APP
	movq	%rax, %rbx
	.loc 1 84 10
	movq	%rbx, %rax
.LBE9:
	.loc 1 84 6
	testq	%rax, %rax
	je	.L18
	.loc 1 84 48 discriminator 1
	movq	-72(%rbp), %rax
	movq	16(%rax), %rax
	.loc 1 84 60 discriminator 1
	andq	$-8, %rax
	.loc 1 84 28 discriminator 1
	movq	%rax, %rdx
.LBB10:
	.loc 1 84 118 discriminator 1
/APP
# 84 "mutex.c" 1
	mov %rax, gs:$24
# 0 "" 2
/NO_APP
	movq	%rax, %rbx
	.loc 1 84 53 discriminator 1
	movq	%rbx, %rax
.LBE10:
	.loc 1 84 24 discriminator 1
	cmpq	%rax, %rdx
	jne	.L18
	.loc 1 85 24
	movq	-72(%rbp), %rax
	movl	8(%rax), %eax
	.loc 1 85 42
	andl	$1024, %eax
	.loc 1 85 8
	testl	%eax, %eax
	jne	.L19
	.loc 1 85 53 discriminator 1
	movl	-84(%rbp), %edx
	movq	-80(%rbp), %rax
	movl	$85, %r8d
	movabsq	$.LC0, %rcx
	movq	%rax, %rsi
	movabsq	$.LC5, %rax
	movq	%rax, %rdi
	movl	$0, %eax
	movabsq	$panic, %r9
	call	*%r9
.LVL9:
.L19:
	.loc 1 87 8
	movq	-72(%rbp), %rax
	movq	16(%rax), %rax
	.loc 1 87 19
	orq	$4, %rax
	movq	%rax, %rdx
	movq	-72(%rbp), %rax
	movq	%rdx, 16(%rax)
	.loc 1 88 12
	movq	-72(%rbp), %rax
	movl	12(%rax), %eax
	.loc 1 88 17
	leal	1(%rax), %edx
	movq	-72(%rbp), %rax
	movl	%edx, 12(%rax)
.LBB11:
	.loc 1 89 50
/APP
# 89 "mutex.c" 1
	mov %rax, gs:$24
# 0 "" 2
/NO_APP
	movq	%rax, %rbx
	.loc 1 89 8
	movq	%rbx, %rax
.LBE11:
	.loc 1 89 16
	movl	60(%rax), %edx
	.loc 1 89 28
	addl	$1, %edx
	movl	%edx, 60(%rax)
	jmp	.L14
.L18:
	.loc 1 93 21
	movl	$100, -44(%rbp)
	movl	$-1, -40(%rbp)
	movl	$0, -36(%rbp)
.LBB12:
	.loc 1 94 82
/APP
# 94 "mutex.c" 1
	mov %rax, gs:$24
# 0 "" 2
/NO_APP
	movq	%rax, %rbx
	.loc 1 94 27
	movq	%rbx, %rax
.LBE12:
	.loc 1 94 13
	orq	$1, %rax
	movq	%rax, -32(%rbp)
.L26:
.LBB13:
	.loc 1 98 42
	movq	$0, -64(%rbp)
	.loc 1 98 86
	movq	-72(%rbp), %rax
	leaq	16(%rax), %rsi
	.loc 1 98 58
	leaq	-64(%rbp), %rdx
	movq	(%rdx), %rax
	movq	-32(%rbp), %rcx
	lock cmpxchgq	%rcx, (%rsi)
	movq	%rax, %rcx
	sete	%al
	testb	%al, %al
	jne	.L21
	movq	%rcx, (%rdx)
.L21:
.LBE13:
	.loc 1 98 8 discriminator 1
	testb	%al, %al
	jne	.L28
	.loc 1 101 11
	jmp	.L24
.L25:
	.loc 1 102 12
	leaq	-44(%rbp), %rax
	movq	%rax, %rdi
	movabsq	$spin_delay_wait, %rax
	call	*%rax
.LVL10:
	.loc 1 102 10 discriminator 1
	testl	%eax, %eax
	jne	.L24
	.loc 1 104 9
	movl	-84(%rbp), %edx
	movq	-80(%rbp), %rax
	movq	%rax, %rsi
	movabsq	$.LC6, %rax
	movq	%rax, %rdi
	movl	$0, %eax
	movabsq	$panic, %rcx
	call	*%rcx
.LVL11:
.L24:
	.loc 1 101 28
	movq	-72(%rbp), %rax
	addq	$16, %rax
	.loc 1 101 12
	movq	(%rax), %rax
	.loc 1 101 47 discriminator 1
	testq	%rax, %rax
	jne	.L25
	.loc 1 98 8
	jmp	.L26
.L28:
	.loc 1 99 7
	nop
	.loc 1 108 16
	movq	-72(%rbp), %rax
	movl	$1, 12(%rax)
.L14:
	.loc 1 109 1
	movq	-8(%rbp), %rbx
	leave
	.cfi_restore 6
	.cfi_restore 3
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE5:
	.size	_mtx_spin_lock, .-_mtx_spin_lock
	.section	.rodata
	.align 8
.LC7:
	.string	"assertion failed: _mtx_spin_unlock() on destroyed mutex, file %s, line %d"
	.align 8
.LC8:
	.string	"assertion failed: _mtx_spin_unlock() on non-spin mutex, file %s, line %d"
	.align 8
.LC9:
	.string	"assertion failed: _mtx_spin_unlock() on unowned mutex, file %s, line %d"
	.align 8
.LC10:
	.string	"assertion failed: _mtx_spin_unlock() on non-recursive mutex, file %s, line %d"
	.align 8
.LC11:
	.string	"assertion failed: _mtx_spin_unlock() expected 0 count, got %d, file %s, line %d"
	.text
	.globl	_mtx_spin_unlock
	.type	_mtx_spin_unlock, @function
_mtx_spin_unlock:
.LFB6:
	.loc 1 111 63
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	pushq	%rbx
	subq	$56, %rsp
	.cfi_offset 3, -24
	movq	%rdi, -40(%rbp)
	movq	%rsi, -48(%rbp)
	movl	%edx, -52(%rbp)
	.loc 1 112 42
	movq	-40(%rbp), %rax
	movq	16(%rax), %rax
	.loc 1 112 54
	andq	$-8, %rax
	.loc 1 112 13
	movq	%rax, -24(%rbp)
	.loc 1 113 12
	movq	-40(%rbp), %rax
	movq	16(%rax), %rax
	.loc 1 113 6
	cmpq	$2, %rax
	jne	.L30
	.loc 1 113 33 discriminator 1
	movl	$113, %edx
	movabsq	$.LC0, %rax
	movq	%rax, %rsi
	movabsq	$.LC7, %rax
	movq	%rax, %rdi
	movl	$0, %eax
	movabsq	$panic, %rcx
	call	*%rcx
.LVL12:
.L30:
	.loc 1 114 22
	movq	-40(%rbp), %rax
	movl	8(%rax), %eax
	.loc 1 114 30
	movzbl	%al, %eax
	.loc 1 114 6
	cmpl	$1, %eax
	je	.L31
	.loc 1 114 54 discriminator 1
	movl	$114, %edx
	movabsq	$.LC0, %rax
	movq	%rax, %rsi
	movabsq	$.LC8, %rax
	movq	%rax, %rdi
	movl	$0, %eax
	movabsq	$panic, %rcx
	call	*%rcx
.LVL13:
.L31:
.LBB14:
	.loc 1 115 63
/APP
# 115 "mutex.c" 1
	mov %rax, gs:$24
# 0 "" 2
/NO_APP
	movq	%rax, %rbx
	.loc 1 115 6
	movq	%rbx, %rax
.LBE14:
	.loc 1 115 6
	cmpq	%rax, -24(%rbp)
	je	.L32
	.loc 1 115 17 discriminator 1
	movl	$115, %edx
	movabsq	$.LC0, %rax
	movq	%rax, %rsi
	movabsq	$.LC9, %rax
	movq	%rax, %rdi
	movl	$0, %eax
	movabsq	$panic, %rcx
	call	*%rcx
.LVL14:
.L32:
	.loc 1 117 10
	movq	-40(%rbp), %rax
	movl	12(%rax), %eax
	.loc 1 117 15
	leal	-1(%rax), %edx
	movq	-40(%rbp), %rax
	movl	%edx, 12(%rax)
	.loc 1 118 10
	movq	-40(%rbp), %rax
	movq	16(%rax), %rax
	.loc 1 118 21
	andl	$4, %eax
	.loc 1 118 6
	testq	%rax, %rax
	je	.L33
	.loc 1 118 38 discriminator 1
	movq	-40(%rbp), %rax
	movl	12(%rax), %eax
	.loc 1 118 28 discriminator 1
	testl	%eax, %eax
	je	.L33
	.loc 1 119 24
	movq	-40(%rbp), %rax
	movl	8(%rax), %eax
	.loc 1 119 42
	andl	$1024, %eax
	.loc 1 119 8
	testl	%eax, %eax
	jne	.L34
	.loc 1 119 53 discriminator 1
	movl	$119, %edx
	movabsq	$.LC0, %rax
	movq	%rax, %rsi
	movabsq	$.LC10, %rax
	movq	%rax, %rdi
	movl	$0, %eax
	movabsq	$panic, %rcx
	call	*%rcx
.LVL15:
.L34:
	.loc 1 120 16
	movq	-40(%rbp), %rax
	movl	12(%rax), %eax
	.loc 1 120 8
	cmpl	$1, %eax
	jne	.L39
	.loc 1 121 10
	movq	-40(%rbp), %rax
	movq	16(%rax), %rax
	.loc 1 121 21
	andq	$-5, %rax
	movq	%rax, %rdx
	movq	-40(%rbp), %rax
	movq	%rdx, 16(%rax)
	.loc 1 123 5
	jmp	.L39
.L33:
	.loc 1 126 16
	movq	-40(%rbp), %rax
	movl	12(%rax), %eax
	.loc 1 126 6
	testl	%eax, %eax
	je	.L37
	.loc 1 126 29 discriminator 1
	movq	-40(%rbp), %rax
	movl	12(%rax), %eax
	movl	$126, %ecx
	movabsq	$.LC0, %rdx
	movl	%eax, %esi
	movabsq	$.LC11, %rax
	movq	%rax, %rdi
	movl	$0, %eax
	movabsq	$panic, %r8
	call	*%r8
.LVL16:
.L37:
	.loc 1 127 20
	movq	-40(%rbp), %rax
	addq	$16, %rax
	.loc 1 127 3
	movl	$0, %edx
	movq	%rdx, (%rax)
.LBB15:
	.loc 1 129 79
/APP
# 129 "mutex.c" 1
	mov %rax, gs:$40
# 0 "" 2
/NO_APP
	movq	%rax, %rbx
	.loc 1 129 6
	movq	%rbx, %rax
.LBE15:
	.loc 1 129 15
	testq	%rax, %rax
	setne	%al
	.loc 1 129 7
	movzbl	%al, %eax
	.loc 1 129 6 discriminator 1
	testq	%rax, %rax
	je	.L38
	.loc 1 129 9 discriminator 1
	movq	-40(%rbp), %rax
.LBB16:
	.loc 1 129 86 discriminator 1
/APP
# 129 "mutex.c" 1
	mov %rdx, gs:$40
# 0 "" 2
/NO_APP
	movq	%rdx, %rbx
	.loc 1 129 6 discriminator 1
	movq	%rbx, %rdx
.LBE16:
	.loc 1 129 9 discriminator 1
	movq	%rax, %rsi
	movq	%rdx, %rdi
	movabsq	$lock_claim_list_remove, %rax
	call	*%rax
.LVL17:
.L38:
	.loc 1 130 3
	movabsq	$spinlock_exit, %rax
	call	*%rax
.LVL18:
	jmp	.L29
.L39:
	.loc 1 123 5
	nop
.L29:
	.loc 1 131 1
	movq	-8(%rbp), %rbx
	leave
	.cfi_restore 6
	.cfi_restore 3
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE6:
	.size	_mtx_spin_unlock, .-_mtx_spin_unlock
.Letext0:
	.file 2 "/home/camdenpina/opt/cross/lib/gcc/x86_64-elf/14.2.0/include/stdint-gcc.h"
	.file 3 "./include/lock.h"
	.file 4 "./include/mutex.h"
	.file 5 "/home/camdenpina/opt/cross/lib/gcc/x86_64-elf/14.2.0/include/stddef.h"
	.file 6 "./include/proc.h"
	.file 7 "./include/panic.h"
	.section	.debug_info,"",@progbits
.Ldebug_info0:
	.long	0x76e
	.value	0x5
	.byte	0x1
	.byte	0x8
	.long	.Ldebug_abbrev0
	.uleb128 0x12
	.long	.LASF57
	.byte	0x1d
	.long	.LASF0
	.long	.LASF1
	.quad	.Ltext0
	.quad	.Letext0-.Ltext0
	.long	.Ldebug_line0
	.uleb128 0x5
	.byte	0x1
	.byte	0x6
	.long	.LASF2
	.uleb128 0x5
	.byte	0x2
	.byte	0x5
	.long	.LASF3
	.uleb128 0x13
	.byte	0x4
	.byte	0x5
	.string	"int"
	.uleb128 0x5
	.byte	0x8
	.byte	0x5
	.long	.LASF4
	.uleb128 0x7
	.long	.LASF7
	.byte	0x2
	.byte	0x2e
	.byte	0x17
	.long	0x56
	.uleb128 0x5
	.byte	0x1
	.byte	0x8
	.long	.LASF5
	.uleb128 0x5
	.byte	0x2
	.byte	0x7
	.long	.LASF6
	.uleb128 0x7
	.long	.LASF8
	.byte	0x2
	.byte	0x34
	.byte	0x16
	.long	0x75
	.uleb128 0xe
	.long	0x64
	.uleb128 0x5
	.byte	0x4
	.byte	0x7
	.long	.LASF9
	.uleb128 0x7
	.long	.LASF10
	.byte	0x2
	.byte	0x37
	.byte	0x1b
	.long	0x88
	.uleb128 0x5
	.byte	0x8
	.byte	0x7
	.long	.LASF11
	.uleb128 0x7
	.long	.LASF12
	.byte	0x2
	.byte	0x56
	.byte	0x1b
	.long	0x88
	.uleb128 0xe
	.long	0x8f
	.uleb128 0xc
	.long	.LASF17
	.byte	0x10
	.byte	0x3
	.byte	0x12
	.byte	0x8
	.long	0xd5
	.uleb128 0x2
	.long	.LASF13
	.byte	0x3
	.byte	0x13
	.byte	0x11
	.long	0xd5
	.byte	0
	.uleb128 0x2
	.long	.LASF14
	.byte	0x3
	.byte	0x14
	.byte	0xe
	.long	0x64
	.byte	0x8
	.uleb128 0x2
	.long	.LASF15
	.byte	0x3
	.byte	0x15
	.byte	0xe
	.long	0x64
	.byte	0xc
	.byte	0
	.uleb128 0xa
	.long	0xe1
	.uleb128 0x5
	.byte	0x1
	.byte	0x6
	.long	.LASF16
	.uleb128 0x14
	.long	0xda
	.uleb128 0xc
	.long	.LASF18
	.byte	0xc
	.byte	0x3
	.byte	0x48
	.byte	0x8
	.long	0x11b
	.uleb128 0x2
	.long	.LASF19
	.byte	0x3
	.byte	0x49
	.byte	0xe
	.long	0x64
	.byte	0
	.uleb128 0x2
	.long	.LASF20
	.byte	0x3
	.byte	0x4a
	.byte	0xe
	.long	0x64
	.byte	0x4
	.uleb128 0x2
	.long	.LASF21
	.byte	0x3
	.byte	0x4d
	.byte	0xe
	.long	0x64
	.byte	0x8
	.byte	0
	.uleb128 0x15
	.string	"mtx"
	.byte	0x18
	.byte	0x4
	.byte	0x7
	.byte	0x10
	.long	0x142
	.uleb128 0x16
	.string	"lo"
	.byte	0x4
	.byte	0x8
	.byte	0x18
	.long	0xa0
	.byte	0
	.uleb128 0x2
	.long	.LASF22
	.byte	0x4
	.byte	0x9
	.byte	0x18
	.long	0x9b
	.byte	0x10
	.byte	0
	.uleb128 0x7
	.long	.LASF23
	.byte	0x4
	.byte	0xa
	.byte	0x3
	.long	0x11b
	.uleb128 0x7
	.long	.LASF24
	.byte	0x5
	.byte	0xd6
	.byte	0x1b
	.long	0x88
	.uleb128 0x5
	.byte	0x8
	.byte	0x5
	.long	.LASF25
	.uleb128 0x5
	.byte	0x10
	.byte	0x4
	.long	.LASF26
	.uleb128 0x5
	.byte	0x8
	.byte	0x7
	.long	.LASF27
	.uleb128 0xc
	.long	.LASF28
	.byte	0x48
	.byte	0x6
	.byte	0x4
	.byte	0x10
	.long	0x20c
	.uleb128 0x2
	.long	.LASF14
	.byte	0x6
	.byte	0x5
	.byte	0xe
	.long	0x64
	.byte	0
	.uleb128 0x2
	.long	.LASF29
	.byte	0x6
	.byte	0x6
	.byte	0xb
	.long	0x142
	.byte	0x8
	.uleb128 0x2
	.long	.LASF30
	.byte	0x6
	.byte	0x8
	.byte	0xf
	.long	0x8f
	.byte	0x20
	.uleb128 0x2
	.long	.LASF31
	.byte	0x6
	.byte	0x9
	.byte	0xc
	.long	0x14e
	.byte	0x28
	.uleb128 0x2
	.long	.LASF32
	.byte	0x6
	.byte	0xc
	.byte	0x9
	.long	0x3c
	.byte	0x30
	.uleb128 0x2
	.long	.LASF33
	.byte	0x6
	.byte	0xd
	.byte	0x17
	.long	0x70
	.byte	0x34
	.uleb128 0x2
	.long	.LASF34
	.byte	0x6
	.byte	0xf
	.byte	0xd
	.long	0x4a
	.byte	0x3a
	.uleb128 0x2
	.long	.LASF35
	.byte	0x6
	.byte	0x10
	.byte	0xd
	.long	0x4a
	.byte	0x3b
	.uleb128 0x2
	.long	.LASF36
	.byte	0x6
	.byte	0x13
	.byte	0x9
	.long	0x3c
	.byte	0x3c
	.uleb128 0x2
	.long	.LASF37
	.byte	0x6
	.byte	0x14
	.byte	0x9
	.long	0x3c
	.byte	0x40
	.uleb128 0x2
	.long	.LASF38
	.byte	0x6
	.byte	0x15
	.byte	0x9
	.long	0x3c
	.byte	0x44
	.byte	0
	.uleb128 0x7
	.long	.LASF39
	.byte	0x6
	.byte	0x16
	.byte	0x3
	.long	0x16f
	.uleb128 0x17
	.long	.LASF58
	.uleb128 0xa
	.long	0x218
	.uleb128 0xf
	.long	.LASF40
	.byte	0x19
	.uleb128 0x10
	.long	.LASF42
	.byte	0x46
	.long	0x23d
	.uleb128 0x6
	.long	0x21d
	.uleb128 0x6
	.long	0x23d
	.byte	0
	.uleb128 0xa
	.long	0xa0
	.uleb128 0xf
	.long	.LASF41
	.byte	0x18
	.uleb128 0x18
	.long	.LASF44
	.byte	0x3
	.byte	0x5b
	.byte	0x5
	.long	0x3c
	.long	0x25e
	.uleb128 0x6
	.long	0x25e
	.byte	0
	.uleb128 0xa
	.long	0xe6
	.uleb128 0x10
	.long	.LASF43
	.byte	0x45
	.long	0x287
	.uleb128 0x6
	.long	0x21d
	.uleb128 0x6
	.long	0x23d
	.uleb128 0x6
	.long	0x8f
	.uleb128 0x6
	.long	0xd5
	.uleb128 0x6
	.long	0x3c
	.byte	0
	.uleb128 0x19
	.long	.LASF45
	.byte	0x7
	.byte	0x13
	.byte	0x10
	.long	0x29a
	.uleb128 0x6
	.long	0xd5
	.uleb128 0x1a
	.byte	0
	.uleb128 0xd
	.long	.LASF49
	.byte	0x6f
	.quad	.LFB6
	.quad	.LFE6-.LFB6
	.uleb128 0x1
	.byte	0x9c
	.long	0x3b4
	.uleb128 0xb
	.string	"mtx"
	.byte	0x6f
	.byte	0x1e
	.long	0x3b4
	.uleb128 0x2
	.byte	0x91
	.sleb128 -56
	.uleb128 0x8
	.long	.LASF46
	.byte	0x6f
	.byte	0x2f
	.long	0xd5
	.uleb128 0x2
	.byte	0x91
	.sleb128 -64
	.uleb128 0x8
	.long	.LASF47
	.byte	0x6f
	.byte	0x39
	.long	0x3c
	.uleb128 0x3
	.byte	0x91
	.sleb128 -68
	.uleb128 0x9
	.long	.LASF48
	.byte	0x70
	.byte	0xd
	.long	0x3b9
	.uleb128 0x2
	.byte	0x91
	.sleb128 -40
	.uleb128 0x3
	.quad	.LBB14
	.quad	.LBE14-.LBB14
	.long	0x312
	.uleb128 0x4
	.string	"__v"
	.byte	0x73
	.byte	0x3a
	.long	0x7c
	.uleb128 0x1
	.byte	0x53
	.byte	0
	.uleb128 0x3
	.quad	.LBB15
	.quad	.LBE15-.LBB15
	.long	0x335
	.uleb128 0x4
	.string	"__v"
	.byte	0x81
	.byte	0x4a
	.long	0x7c
	.uleb128 0x1
	.byte	0x53
	.byte	0
	.uleb128 0x3
	.quad	.LBB16
	.quad	.LBE16-.LBB16
	.long	0x358
	.uleb128 0x4
	.string	"__v"
	.byte	0x81
	.byte	0x51
	.long	0x7c
	.uleb128 0x1
	.byte	0x53
	.byte	0
	.uleb128 0x1
	.quad	.LVL12
	.long	0x287
	.uleb128 0x1
	.quad	.LVL13
	.long	0x287
	.uleb128 0x1
	.quad	.LVL14
	.long	0x287
	.uleb128 0x1
	.quad	.LVL15
	.long	0x287
	.uleb128 0x1
	.quad	.LVL16
	.long	0x287
	.uleb128 0x1
	.quad	.LVL17
	.long	0x228
	.uleb128 0x1
	.quad	.LVL18
	.long	0x661
	.byte	0
	.uleb128 0xa
	.long	0x142
	.uleb128 0xa
	.long	0x20c
	.uleb128 0xd
	.long	.LASF50
	.byte	0x4c
	.quad	.LFB5
	.quad	.LFE5-.LFB5
	.uleb128 0x1
	.byte	0x9c
	.long	0x584
	.uleb128 0xb
	.string	"mtx"
	.byte	0x4c
	.byte	0x1c
	.long	0x3b4
	.uleb128 0x3
	.byte	0x91
	.sleb128 -88
	.uleb128 0x8
	.long	.LASF46
	.byte	0x4c
	.byte	0x2d
	.long	0xd5
	.uleb128 0x3
	.byte	0x91
	.sleb128 -96
	.uleb128 0x8
	.long	.LASF47
	.byte	0x4c
	.byte	0x37
	.long	0x3c
	.uleb128 0x3
	.byte	0x91
	.sleb128 -100
	.uleb128 0x9
	.long	.LASF48
	.byte	0x4d
	.byte	0xd
	.long	0x3b9
	.uleb128 0x2
	.byte	0x91
	.sleb128 -40
	.uleb128 0x9
	.long	.LASF51
	.byte	0x5d
	.byte	0x15
	.long	0xe6
	.uleb128 0x2
	.byte	0x91
	.sleb128 -60
	.uleb128 0x9
	.long	.LASF22
	.byte	0x5e
	.byte	0xd
	.long	0x8f
	.uleb128 0x2
	.byte	0x91
	.sleb128 -48
	.uleb128 0x3
	.quad	.LBB7
	.quad	.LBE7-.LBB7
	.long	0x454
	.uleb128 0x4
	.string	"__v"
	.byte	0x52
	.byte	0x4a
	.long	0x7c
	.uleb128 0x1
	.byte	0x53
	.byte	0
	.uleb128 0x3
	.quad	.LBB8
	.quad	.LBE8-.LBB8
	.long	0x477
	.uleb128 0x4
	.string	"__v"
	.byte	0x52
	.byte	0x4e
	.long	0x7c
	.uleb128 0x1
	.byte	0x53
	.byte	0
	.uleb128 0x3
	.quad	.LBB9
	.quad	.LBE9-.LBB9
	.long	0x49a
	.uleb128 0x4
	.string	"__v"
	.byte	0x54
	.byte	0x2f
	.long	0x7c
	.uleb128 0x1
	.byte	0x53
	.byte	0
	.uleb128 0x3
	.quad	.LBB10
	.quad	.LBE10-.LBB10
	.long	0x4bd
	.uleb128 0x4
	.string	"__v"
	.byte	0x54
	.byte	0x71
	.long	0x7c
	.uleb128 0x1
	.byte	0x53
	.byte	0
	.uleb128 0x3
	.quad	.LBB11
	.quad	.LBE11-.LBB11
	.long	0x4e0
	.uleb128 0x4
	.string	"__v"
	.byte	0x59
	.byte	0x2d
	.long	0x7c
	.uleb128 0x1
	.byte	0x53
	.byte	0
	.uleb128 0x3
	.quad	.LBB12
	.quad	.LBE12-.LBB12
	.long	0x503
	.uleb128 0x4
	.string	"__v"
	.byte	0x5e
	.byte	0x4d
	.long	0x7c
	.uleb128 0x1
	.byte	0x53
	.byte	0
	.uleb128 0x3
	.quad	.LBB13
	.quad	.LBE13-.LBB13
	.long	0x528
	.uleb128 0x9
	.long	.LASF52
	.byte	0x62
	.byte	0x2a
	.long	0x9b
	.uleb128 0x3
	.byte	0x91
	.sleb128 -80
	.byte	0
	.uleb128 0x1
	.quad	.LVL5
	.long	0x287
	.uleb128 0x1
	.quad	.LVL6
	.long	0x287
	.uleb128 0x1
	.quad	.LVL7
	.long	0x703
	.uleb128 0x1
	.quad	.LVL8
	.long	0x263
	.uleb128 0x1
	.quad	.LVL9
	.long	0x287
	.uleb128 0x1
	.quad	.LVL10
	.long	0x248
	.uleb128 0x1
	.quad	.LVL11
	.long	0x287
	.byte	0
	.uleb128 0x11
	.long	.LASF55
	.byte	0x46
	.byte	0xb
	.long	0x3b9
	.quad	.LFB4
	.quad	.LFE4-.LFB4
	.uleb128 0x1
	.byte	0x9c
	.long	0x5cf
	.uleb128 0xb
	.string	"mtx"
	.byte	0x46
	.byte	0x1d
	.long	0x3b4
	.uleb128 0x2
	.byte	0x91
	.sleb128 -40
	.uleb128 0x9
	.long	.LASF22
	.byte	0x47
	.byte	0xf
	.long	0x8f
	.uleb128 0x2
	.byte	0x91
	.sleb128 -24
	.uleb128 0x1
	.quad	.LVL4
	.long	0x287
	.byte	0
	.uleb128 0xd
	.long	.LASF53
	.byte	0x3f
	.quad	.LFB3
	.quad	.LFE3-.LFB3
	.uleb128 0x1
	.byte	0x9c
	.long	0x623
	.uleb128 0xb
	.string	"mtx"
	.byte	0x3f
	.byte	0x17
	.long	0x3b4
	.uleb128 0x2
	.byte	0x91
	.sleb128 -24
	.uleb128 0x8
	.long	.LASF54
	.byte	0x3f
	.byte	0x25
	.long	0x64
	.uleb128 0x2
	.byte	0x91
	.sleb128 -28
	.uleb128 0x8
	.long	.LASF13
	.byte	0x3f
	.byte	0x37
	.long	0xd5
	.uleb128 0x2
	.byte	0x91
	.sleb128 -40
	.uleb128 0x1
	.quad	.LVL3
	.long	0x623
	.byte	0
	.uleb128 0x11
	.long	.LASF56
	.byte	0x2e
	.byte	0xa
	.long	0x64
	.quad	.LFB2
	.quad	.LFE2-.LFB2
	.uleb128 0x1
	.byte	0x9c
	.long	0x661
	.uleb128 0x8
	.long	.LASF54
	.byte	0x2e
	.byte	0x31
	.long	0x64
	.uleb128 0x2
	.byte	0x91
	.sleb128 -36
	.uleb128 0x9
	.long	.LASF14
	.byte	0x2f
	.byte	0xe
	.long	0x64
	.uleb128 0x2
	.byte	0x91
	.sleb128 -20
	.byte	0
	.uleb128 0x1b
	.long	.LASF59
	.byte	0x1
	.byte	0x26
	.byte	0x14
	.quad	.LFB1
	.quad	.LFE1-.LFB1
	.uleb128 0x1
	.byte	0x9c
	.long	0x703
	.uleb128 0x3
	.quad	.LBB4
	.quad	.LBE4-.LBB4
	.long	0x6a2
	.uleb128 0x4
	.string	"__v"
	.byte	0x27
	.byte	0x43
	.long	0x7c
	.uleb128 0x1
	.byte	0x53
	.byte	0
	.uleb128 0x3
	.quad	.LBB5
	.quad	.LBE5-.LBB5
	.long	0x6c5
	.uleb128 0x4
	.string	"__v"
	.byte	0x28
	.byte	0x37
	.long	0x7c
	.uleb128 0x1
	.byte	0x53
	.byte	0
	.uleb128 0x3
	.quad	.LBB6
	.quad	.LBE6-.LBB6
	.long	0x6e8
	.uleb128 0x4
	.string	"__v"
	.byte	0x29
	.byte	0x45
	.long	0x7c
	.uleb128 0x1
	.byte	0x53
	.byte	0
	.uleb128 0x1
	.quad	.LVL1
	.long	0x287
	.uleb128 0x1
	.quad	.LVL2
	.long	0x222
	.byte	0
	.uleb128 0x1c
	.long	.LASF60
	.byte	0x1
	.byte	0x1f
	.byte	0x14
	.quad	.LFB0
	.quad	.LFE0-.LFB0
	.uleb128 0x1
	.byte	0x9c
	.uleb128 0x3
	.quad	.LBB2
	.quad	.LBE2-.LBB2
	.long	0x740
	.uleb128 0x4
	.string	"__v"
	.byte	0x20
	.byte	0x43
	.long	0x7c
	.uleb128 0x1
	.byte	0x53
	.byte	0
	.uleb128 0x3
	.quad	.LBB3
	.quad	.LBE3-.LBB3
	.long	0x763
	.uleb128 0x4
	.string	"__v"
	.byte	0x21
	.byte	0x45
	.long	0x7c
	.uleb128 0x1
	.byte	0x53
	.byte	0
	.uleb128 0x1
	.quad	.LVL0
	.long	0x242
	.byte	0
	.byte	0
	.section	.debug_abbrev,"",@progbits
.Ldebug_abbrev0:
	.uleb128 0x1
	.uleb128 0x48
	.byte	0
	.uleb128 0x7d
	.uleb128 0x1
	.uleb128 0x7f
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x2
	.uleb128 0xd
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x38
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0x3
	.uleb128 0xb
	.byte	0x1
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x7
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x4
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0x21
	.sleb128 1
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0x18
	.byte	0
	.byte	0
	.uleb128 0x5
	.uleb128 0x24
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3e
	.uleb128 0xb
	.uleb128 0x3
	.uleb128 0xe
	.byte	0
	.byte	0
	.uleb128 0x6
	.uleb128 0x5
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x7
	.uleb128 0x16
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x8
	.uleb128 0x5
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0x21
	.sleb128 1
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0x18
	.byte	0
	.byte	0
	.uleb128 0x9
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0x21
	.sleb128 1
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0x18
	.byte	0
	.byte	0
	.uleb128 0xa
	.uleb128 0xf
	.byte	0
	.uleb128 0xb
	.uleb128 0x21
	.sleb128 8
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xb
	.uleb128 0x5
	.byte	0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0x21
	.sleb128 1
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0x18
	.byte	0
	.byte	0
	.uleb128 0xc
	.uleb128 0x13
	.byte	0x1
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xd
	.uleb128 0x2e
	.byte	0x1
	.uleb128 0x3f
	.uleb128 0x19
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0x21
	.sleb128 1
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0x21
	.sleb128 6
	.uleb128 0x27
	.uleb128 0x19
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x7
	.uleb128 0x40
	.uleb128 0x18
	.uleb128 0x7a
	.uleb128 0x19
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xe
	.uleb128 0x35
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xf
	.uleb128 0x2e
	.byte	0
	.uleb128 0x3f
	.uleb128 0x19
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0x21
	.sleb128 6
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0x21
	.sleb128 6
	.uleb128 0x27
	.uleb128 0x19
	.uleb128 0x3c
	.uleb128 0x19
	.byte	0
	.byte	0
	.uleb128 0x10
	.uleb128 0x2e
	.byte	0x1
	.uleb128 0x3f
	.uleb128 0x19
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0x21
	.sleb128 3
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0x21
	.sleb128 6
	.uleb128 0x27
	.uleb128 0x19
	.uleb128 0x3c
	.uleb128 0x19
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x11
	.uleb128 0x2e
	.byte	0x1
	.uleb128 0x3f
	.uleb128 0x19
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0x21
	.sleb128 1
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x27
	.uleb128 0x19
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x7
	.uleb128 0x40
	.uleb128 0x18
	.uleb128 0x7a
	.uleb128 0x19
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x12
	.uleb128 0x11
	.byte	0x1
	.uleb128 0x25
	.uleb128 0xe
	.uleb128 0x13
	.uleb128 0xb
	.uleb128 0x3
	.uleb128 0x1f
	.uleb128 0x1b
	.uleb128 0x1f
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x7
	.uleb128 0x10
	.uleb128 0x17
	.byte	0
	.byte	0
	.uleb128 0x13
	.uleb128 0x24
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3e
	.uleb128 0xb
	.uleb128 0x3
	.uleb128 0x8
	.byte	0
	.byte	0
	.uleb128 0x14
	.uleb128 0x26
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x15
	.uleb128 0x13
	.byte	0x1
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x16
	.uleb128 0xd
	.byte	0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x38
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0x17
	.uleb128 0x13
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3c
	.uleb128 0x19
	.byte	0
	.byte	0
	.uleb128 0x18
	.uleb128 0x2e
	.byte	0x1
	.uleb128 0x3f
	.uleb128 0x19
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x27
	.uleb128 0x19
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x3c
	.uleb128 0x19
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x19
	.uleb128 0x2e
	.byte	0x1
	.uleb128 0x3f
	.uleb128 0x19
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x27
	.uleb128 0x19
	.uleb128 0x87
	.uleb128 0x19
	.uleb128 0x3c
	.uleb128 0x19
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x1a
	.uleb128 0x18
	.byte	0
	.byte	0
	.byte	0
	.uleb128 0x1b
	.uleb128 0x2e
	.byte	0x1
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x27
	.uleb128 0x19
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x7
	.uleb128 0x40
	.uleb128 0x18
	.uleb128 0x7a
	.uleb128 0x19
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x1c
	.uleb128 0x2e
	.byte	0x1
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x27
	.uleb128 0x19
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x7
	.uleb128 0x40
	.uleb128 0x18
	.uleb128 0x7a
	.uleb128 0x19
	.byte	0
	.byte	0
	.byte	0
	.section	.debug_aranges,"",@progbits
	.long	0x2c
	.value	0x2
	.long	.Ldebug_info0
	.byte	0x8
	.byte	0
	.value	0
	.value	0
	.quad	.Ltext0
	.quad	.Letext0-.Ltext0
	.quad	0
	.quad	0
	.section	.debug_line,"",@progbits
.Ldebug_line0:
	.section	.debug_str,"MS",@progbits,1
.LASF39:
	.string	"thread_t"
.LASF44:
	.string	"spin_delay_wait"
.LASF50:
	.string	"_mtx_spin_lock"
.LASF19:
	.string	"delay_count"
.LASF56:
	.string	"_mtx_opts_to_lockobject_flags"
.LASF22:
	.string	"mtx_lock"
.LASF58:
	.string	"lock_claim_list"
.LASF42:
	.string	"lock_claim_list_remove"
.LASF30:
	.string	"kstack_base"
.LASF10:
	.string	"uint64_t"
.LASF3:
	.string	"short int"
.LASF24:
	.string	"size_t"
.LASF33:
	.string	"flags2"
.LASF43:
	.string	"lock_claim_list_add"
.LASF45:
	.string	"panic"
.LASF51:
	.string	"delay"
.LASF32:
	.string	"cpu_id"
.LASF18:
	.string	"spin_delay"
.LASF46:
	.string	"file"
.LASF7:
	.string	"uint8_t"
.LASF12:
	.string	"uintptr_t"
.LASF23:
	.string	"mtx_t"
.LASF47:
	.string	"line"
.LASF36:
	.string	"lock_count"
.LASF35:
	.string	"piority"
.LASF25:
	.string	"long long int"
.LASF40:
	.string	"critical_exit"
.LASF52:
	.string	"__old"
.LASF4:
	.string	"long int"
.LASF53:
	.string	"_mtx_init"
.LASF41:
	.string	"critical_enter"
.LASF31:
	.string	"kstack_size"
.LASF13:
	.string	"name"
.LASF26:
	.string	"long double"
.LASF5:
	.string	"unsigned char"
.LASF48:
	.string	"owner"
.LASF2:
	.string	"signed char"
.LASF14:
	.string	"flags"
.LASF27:
	.string	"long long unsigned int"
.LASF8:
	.string	"uint32_t"
.LASF9:
	.string	"unsigned int"
.LASF55:
	.string	"_mtx_owner"
.LASF49:
	.string	"_mtx_spin_unlock"
.LASF28:
	.string	"thread"
.LASF6:
	.string	"short unsigned int"
.LASF16:
	.string	"char"
.LASF29:
	.string	"lock"
.LASF21:
	.string	"waits"
.LASF15:
	.string	"data"
.LASF57:
	.string	"GNU C23 14.2.0 -mcmodel=large -mno-red-zone -m64 -mtune=generic -march=x86-64 -g -O0 -std=c23 -fno-stack-protector -fno-omit-frame-pointer -fno-builtin-memset -ffreestanding"
.LASF38:
	.string	"crit_level"
.LASF11:
	.string	"long unsigned int"
.LASF59:
	.string	"spinlock_exit"
.LASF37:
	.string	"spin_count"
.LASF34:
	.string	"pri_base"
.LASF54:
	.string	"opts"
.LASF60:
	.string	"spinlock_enter"
.LASF17:
	.string	"lock_object"
.LASF20:
	.string	"max_waits"
	.section	.debug_line_str,"MS",@progbits,1
.LASF0:
	.string	"mutex.c"
.LASF1:
	.string	"/home/camdenpina/Music/ModernOS/arch/x86_64/kernel"
	.ident	"GCC: (GNU) 14.2.0"
