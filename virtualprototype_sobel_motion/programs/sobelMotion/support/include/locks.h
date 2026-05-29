#ifndef LOCKS_INCLUDE_H
#define LOCKS_INCLUDE_H

#include <stdint.h>

#define LOCKS_START_ADDRESS 0xE0000000;
#define NR_OF_LOCKS 256

/**
 * @brief This routine initialises the lock area in the internal SSRAM and should only be called once
 *
 */ 
void init_locks();

/**
 * @brief Waits blocking for a lock
 * returns a value unequal to zero if something went wrong
 *
 */
int get_lock(uint32_t lockId);

/**
 * @brief releases a lock if hold
 *
 */
int release_lock(uint32_t lockId);

#endif /* LOCKS_INCLUDE_H */
