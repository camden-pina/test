#include <cpu/cpu_features.h>
#include <printf.h>

static cpu_features_t g_features;

/* 
 * cpuid - Executes the CPUID instruction with the specified leaf.
 * @leaf: The CPUID leaf number.
 * @eax, @ebx, @ecx, @edx: Pointers to store the corresponding register values.
 */
static inline void cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx,
                         uint32_t *ecx, uint32_t *edx)
{
    __asm__ volatile("cpuid"
                     : "=a" (*eax), "=b" (*ebx),
                       "=c" (*ecx), "=d" (*edx)
                     : "a" (leaf));
}

/*
 * cpuid_count - Executes the CPUID instruction with the specified leaf and subleaf.
 * @leaf: The CPUID leaf number.
 * @subleaf: The CPUID subleaf number.
 * @eax, @ebx, @ecx, @edx: Pointers to store the corresponding register values.
 */
static inline void cpuid_count(uint32_t leaf, uint32_t subleaf, uint32_t *eax,
                               uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
    __asm__ volatile("cpuid"
                     : "=a" (*eax), "=b" (*ebx),
                       "=c" (*ecx), "=d" (*edx)
                     : "a" (leaf), "c" (subleaf));
}

/*
 * xgetbv - Reads the value of the Extended Control Register (XCR) at the specified index.
 * @index: The XCR index to read.
 *
 * Returns the 64-bit value of the XCR.
 */
static inline uint64_t xgetbv(uint32_t index)
{
    uint32_t eax, edx;
    __asm__ volatile("xgetbv"
                     : "=a" (eax), "=d" (edx)
                     : "c" (index));
    return ((uint64_t)edx << 32) | eax;
}

/*
 * check_cpu_features - Populates the cpu_features_t structure with available CPU feature flags.
 * @features: Pointer to the cpu_features_t structure to be populated.
 */
void check_cpu_features(cpu_features_t *features)
{
    kprintf("TEXTBOOK");
    uint32_t eax, ebx, ecx, edx;

    /* Initialize all feature flags to 0 */
    features->sse    = 0;
    features->sse2   = 0;
    features->sse3   = 0;
    features->ssse3  = 0;
    features->sse4_1 = 0;
    features->sse4_2 = 0;
    features->avx    = 0;
    features->avx2   = 0;

    /* CPUID leaf 1: Get standard feature flags */
    cpuid(1, &eax, &ebx, &ecx, &edx);

    features->sse    = (edx & (1 << 25)) ? 1 : 0; /* SSE */
    features->sse2   = (edx & (1 << 26)) ? 1 : 0; /* SSE2 */
    features->sse3   = (ecx & (1 << 0))  ? 1 : 0; /* SSE3 */
    features->ssse3  = (ecx & (1 << 9))  ? 1 : 0; /* SSSE3 */
    features->sse4_1 = (ecx & (1 << 19)) ? 1 : 0; /* SSE4.1 */
    features->sse4_2 = (ecx & (1 << 20)) ? 1 : 0; /* SSE4.2 */

    /*
     * Check for AVX support.
     * AVX requires:
     * - CPUID leaf 1, ECX bit 27 (OSXSAVE) to be set.
     * - CPUID leaf 1, ECX bit 28 (AVX) to be set.
     * - The OS must have enabled the XSAVE/XRESTORE feature set.
     */
    if ((ecx & (1 << 27)) && (ecx & (1 << 28))) {
        uint64_t xcr0 = xgetbv(0);
        /* XCR0[2:1] should be 11b: bit 1 for SSE state and bit 2 for AVX state */
        if ((xcr0 & 0x6) == 0x6) {
            features->avx = 1;
        }
    }

    /*
     * Check for AVX2 support.
     * AVX2 is indicated in CPUID leaf 7, subleaf 0, EBX bit 5.
     * Note: AVX2 is only meaningful if AVX is supported by the OS.
     */
    cpuid_count(7, 0, &eax, &ebx, &ecx, &edx);
    if (features->avx && (ebx & (1 << 5))) {
        features->avx2 = 1;
    }
}

void print_cpu_features() {
        // Populate the features structure with the current CPU's supported features
        kprintf("KJFDL");
        cpu_features_t features;


        kprintf("ROCKY");
        check_cpu_features(&features);
    
        // Display the supported features
        kprintf("CPU Feature Support:\n");
        kprintf("SSE:    %s\n", features.sse ? "Yes" : "No");
        kprintf("SSE2:   %s\n", features.sse2 ? "Yes" : "No");
        kprintf("SSE3:   %s\n", features.sse3 ? "Yes" : "No");
        kprintf("SSSE3:  %s\n", features.ssse3 ? "Yes" : "No");
        kprintf("SSE4.1: %s\n", features.sse4_1 ? "Yes" : "No");
        kprintf("SSE4.2: %s\n", features.sse4_2 ? "Yes" : "No");
        kprintf("AVX:    %s\n", features.avx ? "Yes" : "No");
        kprintf("AVX2:   %s\n", features.avx2 ? "Yes" : "No");
}
