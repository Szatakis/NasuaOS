#include "rand.hpp"

static unsigned long next_random = 1;

static inline unsigned long long read_tsc(void) {
    unsigned int lo, hi;

    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((unsigned long long)hi << 32) | lo;
}

void system_srand(void) {
    next_random = (unsigned long)read_tsc();
}

int system_rand(void) {
    if (next_random == 1) {
        next_random = (unsigned long)read_tsc();
    }

    next_random = next_random * 1103515245 + 12345;
    
    return (int)(next_random / 65536) % 32768;
}
