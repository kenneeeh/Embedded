#ifndef PERF_H_INCLUDED
#define PERF_H_INCLUDED

#include <defs.h>
#include <spr.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PERF_INSTRUCTION_FETCH_MASK (((uint32_t)1) << 0)
#define PERF_ICACHE_MISS_MASK (((uint32_t)1) << 1)
#define PERF_ICACHE_MISS_PENALY_MASK (((uint32_t)1) << 2)
#define PERF_ICACHE_FLUSH_PENALTY_MASH (((uint32_t)1) << 3)
#define PERF_ICACHE_NOP_INSERTION_MASK (((uint32_t)1) << 4)
#define PERF_BRANCH_PENALTY_MASK (((uint32_t)1) << 9)
#define PERF_EXECUTED_INSTRUCTIONS_MASK (((uint32_t)1) << 10)
#define PERF_STALL_CYCLES_MASK (((uint32_t)1) << 11)
#define PERF_BUS_IDLE_MASK (((uint32_t)1) << 12)
#define PERF_DCACHE_UNCACHE_WRITE_MASK (((uint32_t)1) << 16)
#define PERF_DCACHE_UNCACHE_READ_MASK (((uint32_t)1) << 17)
#define PERF_DCACHE_CACHE_WRITE_MASK (((uint32_t)1) << 18)
#define PERF_DCACHE_CACHE_READ_MASK (((uint32_t)1) << 19)
#define PERF_DCACHE_SWAP_MASK (((uint32_t)1) << 20)
#define PERF_DCACHE_CAS_MASK (((uint32_t)1) << 21)
#define PERF_DCACHE_MISS_MASK (((uint32_t)1) << 22)
#define PERF_DCACHE_WRITE_BACK_MASK (((uint32_t)1) << 23)
#define PERF_DCACHE_DATA_DEP_MASK (((uint32_t)1) << 24)
#define PERF_DCACHE_WRITE_DEP_MASK (((uint32_t)1) << 25)
#define PERF_DCACHE_PIPE_STALL_MASK (((uint32_t)1) << 26)
#define PERF_DCACHE_INTENAL_STALL_MASK (((uint32_t)1) << 27)
#define PERF_DCACHE_WRITE_THROUGH_MASK (((uint32_t)1) << 28)
#define PERF_DCACHE_SNOOPY_INVAL_MASK (((uint32_t)1) << 29)

#define PERF_COUNTER_0 0
#define PERF_COUNTER_1 1
#define PERF_COUNTER_2 2
#define PERF_COUNTER_3 3
#define PERF_COUNTER_4 4
#define PERF_COUNTER_5 5
#define PERF_COUNTER_6 6
#define PERF_COUNTER_7 7
#define PERF_COUNTER_RUNTIME 8

#define PERF_SPR 0xF800
#define PERF_MEMDIST_SPR 0xD800

typedef uint64_t perf_cycles_t;

typedef struct {
    unsigned h, m, s, ms;
} perf_time_t;

/**
 * @brief Must be called first.
 * 
 */
void perf_init();

__static_inline uint32_t perf_get_mask(unsigned counter_id) {
    return SPR_READ2(PERF_SPR, counter_id + 1);
}

__static_inline uint32_t perf_set_mask(unsigned counter_id, uint32_t mask) {
    SPR_WRITE2(PERF_SPR, counter_id + 1, mask);
}

__static_inline perf_cycles_t perf_read_counter(unsigned counter_id) {
    perf_cycles_t lo = SPR_READ2(PERF_SPR, counter_id * 2 + 11);
    perf_cycles_t hi = SPR_READ2(PERF_SPR, counter_id * 2 + 12);
    return lo | (hi << 32);
}

__static_inline void perf_start() {
    SPR_WRITE(PERF_SPR, 1 << 9);
}

__static_inline void perf_stop() {
    SPR_WRITE(PERF_SPR, 0);
}

__static_inline void perf_memdist_set(uint32_t dist) {
    SPR_WRITE(PERF_MEMDIST_SPR, dist);
}

__static_inline uint32_t perf_memdist_get() {
    return SPR_READ(PERF_MEMDIST_SPR);
}

uint32_t perf_cpu_freq();
perf_time_t perf_cycles_to_time(perf_cycles_t cycles);
void perf_print_cycles(unsigned counter_id, const char *desc);
void perf_print_time(unsigned counter_id, const char *desc);

#ifdef __cplusplus
}
#endif

#endif /* PERF_H_INCLUDED */
