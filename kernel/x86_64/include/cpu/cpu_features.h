#ifndef CPU_FEATURES_H
#define CPU_FEATURES_H

#include <stdint.h>

/* Structure to hold the CPU feature flags */
typedef struct {
    int sse;
    int sse2;
    int sse3;
    int ssse3;
    int sse4_1;
    int sse4_2;
    int avx;
    int avx2;
} cpu_features_t;

/* Function to populate the cpu_features_t structure with available feature flags */
void check_cpu_features(cpu_features_t *features);

void print_cpu_features();

#endif /* CPU_FEATURES_H */
