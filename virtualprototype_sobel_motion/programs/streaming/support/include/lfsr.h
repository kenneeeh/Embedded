#ifndef UTIL_LFSR_H_INCLUDED
#define UTIL_LFSR_H_INCLUDED

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t lfsr_unsigned_t;

#define LFSR_UNSIGNED(x) ((lfsr_unsigned_t)(x##ull))

struct lfsr_fibonacci {
    struct {
        lfsr_unsigned_t mask;
        lfsr_unsigned_t state;
        int xnor;
        lfsr_unsigned_t feedback;
    } _;
};

/**
 * @brief Initializes the LFSR-based pseudo-random number generator.
 * @note Uses the default feedback.
 * @note The state cannot be zero for XOR-based feedback.
 * @note The state cannot be all ones for XNOR-based feedback.
 *
 * @param lfsr_fibonacci
 * @param nbits
 * @param state
 * @param xnor 1 for XNOR-based feedback, 0 for XOR-based feedback.
 */
void lfsr_fibonacci_init(struct lfsr_fibonacci* lfsr_fibonacci, unsigned nbits, lfsr_unsigned_t state, int xnor);

/**
 * @brief Initializes the LFSR-based pseudo-random number generator with a user-provided feedback.
 * @note The state cannot be zero for XOR-based feedback.
 * @note The state cannot be all ones for XNOR-based feedback.
 *
 * @param lfsr_fibonacci
 * @param nbits
 * @param state
 * @param xnor 1 for XNOR-based feedback, 0 for XOR-based feedback.
 * @param feedback
 */
void lfsr_fibonacci_init2(struct lfsr_fibonacci* lfsr_fibonacci, unsigned nbits, lfsr_unsigned_t state, int xnor, lfsr_unsigned_t feedback);

/**
 * @brief Gets the next random number.
 *
 * @param lfsr_fibonacci
 * @return lfsr_unsigned_t
 */
lfsr_unsigned_t lfsr_fibonacci_next(struct lfsr_fibonacci* lfsr_fibonacci);

#ifdef __cplusplus
}
#endif

#endif /* UTIL_LFSR_H_INCLUDED */
