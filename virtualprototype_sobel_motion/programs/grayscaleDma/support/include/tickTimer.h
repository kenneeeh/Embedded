#ifndef __TICK_TIMER_H__
#define __TICK_TIMER_H__

#include <spr.h>
#include <defs.h>
#include <stdint.h>
#include <stdio.h>

#define TICK_TIMER_MODE_REGISTER  0x5000
#define TICK_TIMER_COUNT_REGISTER 0x5001

#define TICK_TIME_PERIOD_MASK 0xFFFFFFF
#define TICK_INTERRUPT_PENDING_BIT 1<<28
#define TICK_INTERRUPT_ENABLE_BIT 1<<29
#define TICK_TIMER_DISABLED 0<<30
#define TICK_TIMER_CONTINUES_MODE 1<<30
#define TICK_TIMER_ONE_SHOT 2<<30

#define TICK_TIMER_EXCEPTION_BIT 1<<1

#define NS_DIVIDE_VALUE 1000000000
#define US_DIVIDE_VALUE 1000000
#define MS_DIVIDE_VALUE 1000

__static_inline uint32_t getCpuFrequencyInHz() {
  uint32_t temp = SPR_READ(9);
  uint32_t result = (temp>>28)&0xF;
  result = (result*10)+((temp>>24)&0xF);
  result = (result*10)+((temp>>20)&0xF);
  result = (result*10)+((temp>>16)&0xF);
  result = (result*10)+((temp>>12)&0xF);
  result = (result*10)+((temp>>8)&0xF);
  return result * 1000;
}

__static_inline uint32_t timerMaxValue(uint32_t multiplyValue, uint32_t divideValue) {
  uint64_t result = ((uint64_t) getCpuFrequencyInHz()) * multiplyValue;
  result /= divideValue;
  return result;
}

__static_inline void setTickTimerModeRegister(uint32_t ttmr) {
  SPR_WRITE(TICK_TIMER_MODE_REGISTER, ttmr);
}

__static_inline uint32_t getTickTimerModeRegister() {
  return SPR_READ(TICK_TIMER_MODE_REGISTER);
}

__static_inline void enableTickTimerIrq(uint32_t enable) {
  uint32_t supervisor = SPR_READ(17);
  supervisor = (enable == 1) ? supervisor | TICK_TIMER_EXCEPTION_BIT : supervisor & ~TICK_TIMER_EXCEPTION_BIT;
  SPR_WRITE(17,supervisor);
}

__static_inline void clearTickTimerIrq() {
  uint32_t ttmr = SPR_READ(TICK_TIMER_MODE_REGISTER);
  ttmr &= TICK_INTERRUPT_PENDING_BIT ^ 0xFFFFFFFF;
  SPR_WRITE(TICK_TIMER_MODE_REGISTER, ttmr);
}

__static_inline void clearTickTimerCountRegister() {
  SPR_WRITE(TICK_TIMER_COUNT_REGISTER,0);
}

#endif
