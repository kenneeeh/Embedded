#include <spr.h>
#include <barriers.h>

void wait_for_barrier() {
   unsigned int reg, mask;
   toggle_barrier();
   asm volatile("l.nop;l.nop;l.nop");
   mask = SPR_READ(0x5002)&1; // read my barrier
   mask = (mask == 0) ? 0 : 0x0000FF00;
   do {
     reg = SPR_READ(0x5002)&0xFF00;
   } while ((reg ^ mask) != 0);
}
