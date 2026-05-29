#ifndef BARRIERS_INCLUDE_H
#define BARRIERS_INCLUDE_H
#include <spr.h>

#define toggle_barrier() SPR_WRITE(0x5002, SPR_READ(0x5002) ^ 1)

void wait_for_barrier();

#endif /* BARRIERS_INCLUDE_H */
