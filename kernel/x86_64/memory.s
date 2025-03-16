.include "kernel.inc"

# Macro: memset_slow reg, step
# Writes one unit (byte/word/dword/qword) at a time.
.macro memset_slow reg, step
    mov    %rdi, %rcx
    test   %rdx, %rdx
    jz     .Lend_slow_
.Lloop_slow_:
.if (\step == 1)
    movb   \reg, (%rdi)
.elseif (\step == 4)
    movl   \reg, (%rdi)
.elseif (\step == 8)
    movq   \reg, (%rdi)
.else
    .error "Unsupported step in memset_slow"
.endif
    add    $\step, %rdi
    sub    $1, %rdx
    jnz    .Lloop_slow_
.Lend_slow_:
.endm

# Macro: memset_fast_top size, suffix
# Saves registers and computes the number of 16-byte chunks.
.macro memset_fast_top size, suffix
    push   %rbp
    mov    %rsp, %rbp
    mov    %rdi, -8(%rbp)       # save dest
    mov    %rsi, -16(%rbp)      # save val
    mov    %rdx, -24(%rbp)      # save len
    mov    $ \size, %rax        # load size factor (immediate)
    xor    %rdx, %rdx
    imulq  -24(%rbp)           # rax = size * (saved len)
    mov    %rax, -24(%rbp)      # update len slot with total bytes
    # check destination alignment: dest % 128
    mov    %rdi, %rax          # rax = dest
    mov    $128, %rcx
    xor    %rdx, %rdx
    idiv   %rcx                # divide rax by 128, remainder in %rdx
    test   %rdx, %rdx
    jz     1f
    movabs $__memset_fast_unaligned, %r8
    jmp    2f
1:
    movabs $__memset_fast_aligned, %r8
2:
    # determine how many 16-byte chunks there are
    mov    -24(%rbp), %rax     # rax = total bytes
    mov    $16, %rcx
    xor    %rdx, %rdx
    idiv   %rcx                # divide by 16: quotient in rax, remainder in rdx
    test   %rax, %rax
    jz     .Lend_fast_bottom_\suffix
    mov    %rax, %r9          # save quotient in r9 (number of 16-byte chunks)
    mov    %rdx, %r10         # save remainder in r10
.Lend_fast_bottom_\suffix:
.endm

# Macro: memset_fast_bottom reg, step, suffix
# Uses slow memset for any remaining bytes.
.macro memset_fast_bottom reg, step, suffix
    mov    %r10, -24(%rbp)       # update len with remainder (saved in r10)
    mov    -16(%rbp), %rsi       # restore val
    mov    -24(%rbp), %rdx       # restore remaining len
    mov    %rdi, %rcx           # save current dest pointer
    test   %rdx, %rdx
    jz     .Lend_bottom_\suffix
.Lloop_bottom_\suffix:
.if (\step == 1)
    movb   \reg, (%rdi)
.elseif (\step == 4)
    movl   \reg, (%rdi)
.elseif (\step == 8)
    movq   \reg, (%rdi)
.else
    .error "Unsupported step in memset_fast_bottom"
.endif
    add    $\step, %rdi
    sub    $1, %rdx
    jnz    .Lloop_bottom_\suffix
.Lend_bottom_\suffix:
    mov    %rcx, %rax
    pop    %rbp
.endm

# __memset_fast_aligned - memset into 16-byte aligned memory
# dest in %rdi, number of 16-byte chunks in %r9, value in %xmm0.
.global __memset_fast_aligned
.type __memset_fast_aligned, @function
__memset_fast_aligned:
.Lloop_aligned:
    movntdq %xmm0, (%rdi)
    add    $16, %rdi
    sub    $1, %r9
    jnz    .Lloop_aligned
    ret

# __memset_fast_unaligned - memset into unaligned memory
# dest in %rdi, number of 16-byte chunks in %r9, value in %xmm0.
.global __memset_fast_unaligned
.type __memset_fast_unaligned, @function
__memset_fast_unaligned:
.Lloop_unaligned:
    movdqu %xmm0, (%rdi)
    add    $16, %rdi
    sub    $1, %r9
    jnz    .Lloop_unaligned
    ret

# void *__memset_slow(void *dest, int val, size_t len)
.global __memset_slow
.type __memset_slow, @function
__memset_slow:
    mov    %rdi, %rcx
    test   %rdx, %rdx
    jz     .Lend_slow_func
.Lloop_slow_func:
    movb   %sil, (%rdi)
    add    $1, %rdi
    sub    $1, %rdx
    jnz    .Lloop_slow_func
.Lend_slow_func:
    mov    %rcx, %rax
    ret

# void *__memset8(void *dest, uint8_t val, size_t len)
.global __memset8
.type __memset8, @function
__memset8:
    memset_fast_top 1, 8
    mov    $0x0101010101010101, %rax
    imulq  %rsi, %rax
    movq   %rax, %xmm0
    movddup %xmm0, %xmm0
    call   *%r8
    memset_fast_bottom %sil, 1, 8
    ret

# void *__memset32(void *dest, uint32_t val, size_t len)
.global __memset32
.type __memset32, @function
__memset32:
    memset_fast_top 4, 32
    mov    %rsi, %rax
    shl    $32, %rax
    or     %rsi, %rax
    movq   %rax, %xmm0
    movddup %xmm0, %xmm0
    call   *%r8
    memset_fast_bottom %eax, 4, 32
    ret

# void *__memset64(void *dest, uint64_t val, size_t len)
.global __memset64
.type __memset64, @function
__memset64:
    memset_fast_top 8, 64
    mov    %rsi, %rax
    movq   %rax, %xmm0
    movddup %xmm0, %xmm0
    call   *%r8
    memset_fast_bottom %rax, 8, 64
    ret
