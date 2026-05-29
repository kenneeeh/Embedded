#ifndef CPU2_INCLUDE_H
#define CPU2_INCLUDE_H

#include <spr.h>

/**
 * @brief sets the stack pointer of cpu2 to st
 *
 */
#define SET_CPU2_STACK_POINTER(st) \
    SPR_WRITE(0x5021,r)

/**
 * @brief sets the start address of cpu2's main routine
 * `r` should be a function pointer.
 *
 */
#define SET_CPU2_MAIN(r) \
    SPR_WRITE(0x5009,r)

/**
 * @brief starts the execution of CPU2
 *
 */
#define START_CPU2() ({\
    uint32_t r;\
    r = SPR_READ(0x5004);\
    asm volatile("l.nop;l.nop");\
    r |= 0x202;\
    SPR_WRITE(0x5004, r);\
    r; \
  })

/**
 * @brief sets the stack pointer of cpu2 related to the stack pointer of cpu1
 * off -> if the stackpointer of cpu1 is in SDRAM this defines the offset
 *
 */
void set_stack_cpu2(unsigned int off);

/**
 * @brief Initialises the exception handlers and stack pointer of cpu2 and calls main2
 *
 */
void init_cpu2();

#endif /* CPU2_INCLUDE_H */
