#ifndef DELAY_INCLUDE_H
#define DELAY_INCLUDE_H

#define delay_blocking_usec( delay ) asm volatile ("l.nios_rrc r0,%[in1],r0,0x6"::[in1]"r"(delay))

#endif /* DELAY_INCLUDE_H */
