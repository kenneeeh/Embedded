#include <locks.h>
#include <stdio.h>
#include <stdint.h>
#include <spr.h>

void init_locks() {
  uint8_t *locks = (uint8_t *) LOCKS_START_ADDRESS;
  
  for (int i=0; i < NR_OF_LOCKS; i++) locks[i] = 0;
}

int get_lock(uint32_t lockId) {
  if (lockId >= NR_OF_LOCKS) return -1;
  uint8_t *locks = (uint8_t *) LOCKS_START_ADDRESS;
  uint8_t res;
  uint8_t cpuId = SPR_READ(9)&0xF;
  do {
    asm volatile ("l.cas %[out1],%[in1],%[in2],0":[out1]"=r"(res):
                  [in1]"r"(&locks[lockId]),[in2]"r"(cpuId));
  } while (res != cpuId);
  return 0;
}

int release_lock(uint32_t lockId) {
  if (lockId >= NR_OF_LOCKS) return -1;
  uint8_t *locks = (uint8_t *) LOCKS_START_ADDRESS;
  uint8_t cpuId = SPR_READ(9)&0xF;
  if (locks[lockId] != cpuId) return -1;
  locks[lockId] = 0;
  return 0;
}
