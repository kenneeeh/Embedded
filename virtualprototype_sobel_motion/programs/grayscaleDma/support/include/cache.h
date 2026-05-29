// IMPORTANT: The caches can only be used in the OR1300 system!

#ifndef CACHE_H_INCLUDED
#define CACHE_H_INCLUDED

#include <defs.h>
#include <spr.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CACHE_DIRECT_MAPPED ((uint32_t)0)
#define CACHE_TWO_WAY ((uint32_t)1)
#define CACHE_FOUR_WAY ((uint32_t)2)
#define CACHE_WRITE_THROUGH (((uint32_t)0) << 8)
#define CACHE_WRITE_BACK (((uint32_t)1) << 8)
#define CACHE_REPLACE_FIFO (((uint32_t)0) << 16)
#define CACHE_REPLACE_PLRU (((uint32_t)1) << 16)
#define CACHE_REPLACE_LRU (((uint32_t)2) << 16)
#define CACHE_COHERENCE (((uint32_t)1) << 18)
#define CACHE_MSI (((uint32_t)0) << 20)
#define CACHE_MESI (((uint32_t)1) << 20)
#define CACHE_SNARFING_ENABLE (((uint32_t)1) << 21)
#define CACHE_FLUSH (((uint32_t)1) << 29)
#define CACHE_SIZE_1K (((uint32_t)0) << 30)
#define CACHE_SIZE_2K (((uint32_t)1) << 30)
#define CACHE_SIZE_4K (((uint32_t)2) << 30)
#define CACHE_SIZE_8K (((uint32_t)3) << 30)

#define CACHE_SPR_ENABLE 17
#define CACHE_SPR_ICACHE 6
#define CACHE_SPR_DCACHE 5

#define CACHE_ICACHE_SHIFT 4
#define CACHE_DCACHE_SHIFT 3

__static_inline void icache_enable(int enable) {
    uint32_t r = SPR_READ(CACHE_SPR_ENABLE) & ~(((uint32_t)1) << CACHE_ICACHE_SHIFT);
    r |= (enable & 1) << CACHE_ICACHE_SHIFT;
    SPR_WRITE(CACHE_SPR_ENABLE, r);
}

__static_inline int icache_enabled() {
    return SPR_READ(CACHE_SPR_ENABLE) & (((uint32_t)1) << CACHE_ICACHE_SHIFT);
}

__static_inline uint32_t icache_read_cfg() {
    return SPR_READ(CACHE_SPR_ICACHE);
}

__static_inline void icache_write_cfg(uint32_t cfg) {
    SPR_WRITE(CACHE_SPR_ICACHE, cfg );
}

__static_inline void icache_flush() {
    SPR_WRITE(CACHE_SPR_ICACHE, CACHE_FLUSH);
}

__static_inline void dcache_enable(int enable) {
    uint32_t r = SPR_READ(CACHE_SPR_ENABLE) & ~(((uint32_t)1) << CACHE_DCACHE_SHIFT);
    r |= (enable & 1) << CACHE_DCACHE_SHIFT;
    SPR_WRITE(CACHE_SPR_ENABLE, r);
}

__static_inline int dcache_enabled() {
    return SPR_READ(CACHE_SPR_ENABLE) & (((uint32_t)1) << CACHE_DCACHE_SHIFT);
}

__static_inline uint32_t dcache_read_cfg() {
    return SPR_READ(CACHE_SPR_DCACHE);
}

__static_inline void dcache_write_cfg(uint32_t cfg) {
    SPR_WRITE(CACHE_SPR_DCACHE, cfg);
}

__static_inline void dcache_flush() {
    SPR_WRITE(CACHE_SPR_DCACHE, CACHE_FLUSH);
}

void cache_printinfo(uint32_t value);

#ifdef __cplusplus
}
#endif

#endif /* CACHE_H_INCLUDED */
