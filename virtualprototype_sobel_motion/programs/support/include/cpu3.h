#ifndef CPU3_INCLUDE_H
#define CPU3_INCLUDE_H

#include <spr.h>
/**
 * @brief sets the stack pointer of cpu2 to st
 *
 */
#define SET_CPU3_STACK_POINTER(st) \
    SPR_WRITE(0x5022,r)

/**
 * @brief sets the start address of cpu2's main routine
 * `r` should be a function pointer.
 *
 */
#define SET_CPU3_MAIN(r) \
    SPR_WRITE(0x500A,r)

/**
 * @brief starts the execution of CPU2
 *
 */
#define START_CPU3() ({\
    uint32_t r;\
    r = SPR_READ(0x5004);\
    asm volatile("l.nop;l.nop");\
    r |= 0x404;\
    SPR_WRITE(0x5004, r);\
    r; \
  })

/**
 * @brief sets the stack pointer of cpu2 related to the stack pointer of cpu1
 * off -> if the stackpointer of cpu1 is in SDRAM this defines the offset
 *
 */
void set_stack_cpu3(unsigned int off);

/**
 * @brief Initialises the exception handlers and stack pointer of cpu2 and calls main2
 *
 */
void init_cpu3();

#endif /* CPU3_INCLUDE_H */
