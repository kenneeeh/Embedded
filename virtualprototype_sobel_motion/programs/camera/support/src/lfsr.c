#include <lfsr.h>
#include <limits.h>
#include <assert.h>

static unsigned lfsr_count1s(lfsr_unsigned_t x)
{
    return __builtin_choose_expr(
        __builtin_types_compatible_p(unsigned int, lfsr_unsigned_t),
        __builtin_popcount((unsigned int)x),
        __builtin_choose_expr(
            __builtin_types_compatible_p(unsigned long, lfsr_unsigned_t),
            __builtin_popcountl((unsigned long)x),
            __builtin_choose_expr(
                __builtin_types_compatible_p(unsigned long long, lfsr_unsigned_t),
                __builtin_popcountll((unsigned long long)x),
                (void)0)));
}

static unsigned lfsr_xor_reduce(lfsr_unsigned_t x)
{
    return __builtin_choose_expr(
        __builtin_types_compatible_p(unsigned int, lfsr_unsigned_t),
        __builtin_parity((unsigned int)x),
        __builtin_choose_expr(
            __builtin_types_compatible_p(unsigned long, lfsr_unsigned_t),
            __builtin_parityl((unsigned long)x),
            __builtin_choose_expr(
                __builtin_types_compatible_p(unsigned long long, lfsr_unsigned_t),
                __builtin_parityll((unsigned long long)x),
                (void)0)));
}

static lfsr_unsigned_t lfsr_feedbacks[] = {
    LFSR_UNSIGNED(0x9),
    LFSR_UNSIGNED(0x12),
    LFSR_UNSIGNED(0x21),
    LFSR_UNSIGNED(0x41),
    LFSR_UNSIGNED(0x8E),
    LFSR_UNSIGNED(0x108),
    LFSR_UNSIGNED(0x204),
    LFSR_UNSIGNED(0x402),
    LFSR_UNSIGNED(0x829),
    LFSR_UNSIGNED(0x100D),
    LFSR_UNSIGNED(0x2015),
    LFSR_UNSIGNED(0x4001),
    LFSR_UNSIGNED(0x8016),
    LFSR_UNSIGNED(0x10004),
    LFSR_UNSIGNED(0x20013),
    LFSR_UNSIGNED(0x40013),
    LFSR_UNSIGNED(0x80004),
    LFSR_UNSIGNED(0x100002),
    LFSR_UNSIGNED(0x200001),
    LFSR_UNSIGNED(0x400010),
    LFSR_UNSIGNED(0x80000D),
    LFSR_UNSIGNED(0x1000004),
    LFSR_UNSIGNED(0x2000023),
    LFSR_UNSIGNED(0x4000013),
    LFSR_UNSIGNED(0x8000004),
    LFSR_UNSIGNED(0x10000002),
    LFSR_UNSIGNED(0x20000029),
    LFSR_UNSIGNED(0x40000004),
    LFSR_UNSIGNED(0x80000057),
    LFSR_UNSIGNED(0x100000029),
    LFSR_UNSIGNED(0x200000073),
    LFSR_UNSIGNED(0x400000002),
    LFSR_UNSIGNED(0x80000003B),
    LFSR_UNSIGNED(0x100000001F),
    LFSR_UNSIGNED(0x2000000031),
    LFSR_UNSIGNED(0x4000000008),
    LFSR_UNSIGNED(0x800000001C),
    LFSR_UNSIGNED(0x10000000004),
    LFSR_UNSIGNED(0x2000000001F),
    LFSR_UNSIGNED(0x4000000002C),
    LFSR_UNSIGNED(0x80000000032),
    LFSR_UNSIGNED(0x10000000000D),
    LFSR_UNSIGNED(0x200000000097),
    LFSR_UNSIGNED(0x400000000010),
    LFSR_UNSIGNED(0x80000000005B),
    LFSR_UNSIGNED(0x1000000000038),
    LFSR_UNSIGNED(0x200000000000E),
    LFSR_UNSIGNED(0x4000000000025),
    LFSR_UNSIGNED(0x8000000000004),
    LFSR_UNSIGNED(0x10000000000023),
    LFSR_UNSIGNED(0x2000000000003E),
    LFSR_UNSIGNED(0x40000000000023),
    LFSR_UNSIGNED(0x8000000000004A),
    LFSR_UNSIGNED(0x100000000000016),
    LFSR_UNSIGNED(0x200000000000031),
    LFSR_UNSIGNED(0x40000000000003D),
    LFSR_UNSIGNED(0x800000000000001),
    LFSR_UNSIGNED(0x1000000000000013),
    LFSR_UNSIGNED(0x2000000000000034),
    LFSR_UNSIGNED(0x4000000000000001),
    LFSR_UNSIGNED(0x800000000000000D)};

void lfsr_fibonacci_init(struct lfsr_fibonacci *lfsr_fibonacci, unsigned nbits, lfsr_unsigned_t state, int xnor)
{
    assert(nbits >= 4 && nbits <= 64);
    lfsr_fibonacci_init2(lfsr_fibonacci, nbits, state, xnor, lfsr_feedbacks[nbits - 4]);
}

void lfsr_fibonacci_init2(struct lfsr_fibonacci *lfsr_fibonacci, unsigned nbits, lfsr_unsigned_t state, int xnor, lfsr_unsigned_t feedback)
{
    assert(nbits >= 4 && nbits <= (sizeof(lfsr_unsigned_t) * CHAR_BIT));
    lfsr_fibonacci->_.mask = (LFSR_UNSIGNED(1) << nbits) - LFSR_UNSIGNED(1);

    assert((!xnor && (state > LFSR_UNSIGNED(0))) || (xnor && (state + LFSR_UNSIGNED(1) >= LFSR_UNSIGNED(0))));
    lfsr_fibonacci->_.state = state;
    lfsr_fibonacci->_.xnor = xnor;

    assert((!xnor || (xnor && (lfsr_count1s(feedback) % 2 == 0))) && "feedback must have an even number of 1s for xnor");
    lfsr_fibonacci->_.feedback = feedback;
}

lfsr_unsigned_t lfsr_fibonacci_next(struct lfsr_fibonacci *lfsr_fibonacci)
{
    lfsr_unsigned_t feedback = lfsr_fibonacci->_.feedback;
    lfsr_unsigned_t mask = lfsr_fibonacci->_.mask;
    lfsr_unsigned_t next = lfsr_fibonacci->_.state;

    if (lfsr_fibonacci->_.xnor)
        next = ((next << 1u) | !lfsr_xor_reduce(next & feedback)) & mask;
    else
        next = ((next << 1u) | lfsr_xor_reduce(next & feedback)) & mask;

    lfsr_fibonacci->_.state = next;
    return next;
}
