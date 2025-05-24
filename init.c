void _start() {
    unsigned long long val = 0;

    __asm__ __volatile__(
        "mov $25, %%rax\n\t"
        "mov %%rax, %0\n\t"
        : "=r"(val)
        :
        : "rax"
    );

    while (1) {
    }
}
