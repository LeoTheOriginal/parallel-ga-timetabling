/*
 * PCG Random Number Generation for C.
 *
 * Copyright 2014 Melissa O'Neill <oneill@pcg-random.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * permissions and limitations under the License.
 *
 * For additional information about the PCG random number generation scheme,
 * including its license and other licensing options, visit
 *
 *       http://www.pcg-random.org
 */

/*
 * This code is derived from the full C implementation, which is in turn
 * derived from the canonical C++ PCG implementation. The C++ version
 * has many additional features and is preferable if you can use C++ in
 * your project.
 */

#include "pcg_basic.h"

/* Multi-step advance functions (jump-ahead, jump-back) */

static uint64_t pcg_advance_lcg_64(uint64_t state, uint64_t delta,
                                    uint64_t cur_mult, uint64_t cur_plus)
{
    uint64_t acc_mult = 1u;
    uint64_t acc_plus = 0u;
    while (delta > 0) {
        if (delta & 1) {
            acc_mult *= cur_mult;
            acc_plus = acc_plus * cur_mult + cur_plus;
        }
        cur_plus = (cur_mult + 1) * cur_plus;
        cur_mult *= cur_mult;
        delta /= 2;
    }
    return acc_mult * state + acc_plus;
}

/* ---------- State table (single global instance for non-_r functions) ---- */

/* --- pcg32 --- */

/* Output function: XSH RR */

static uint32_t pcg_output_xsh_rr_64_32(uint64_t state)
{
    return (uint32_t)(((state >> 18u) ^ state) >> 27u) >> (state >> 59u)
         | (uint32_t)(((state >> 18u) ^ state) >> 27u) << ((-((int32_t)(state >> 59u))) & 31);
}

void pcg32_srandom_r(pcg32_random_t *rng, uint64_t initstate, uint64_t initseq)
{
    rng->state = 0U;
    rng->inc = (initseq << 1u) | 1u;
    pcg32_random_r(rng);
    rng->state += initstate;
    pcg32_random_r(rng);
}

uint32_t pcg32_random_r(pcg32_random_t *rng)
{
    uint64_t oldstate = rng->state;
    rng->state = oldstate * 6364136223846793005ULL + rng->inc;
    return pcg_output_xsh_rr_64_32(oldstate);
}

uint32_t pcg32_boundedrand_r(pcg32_random_t *rng, uint32_t bound)
{
    /* To avoid bias, we need to make the range of the RNG a multiple of
     * bound, which we do by dropping output less than a threshold.
     * A naive scheme to calculate the threshold would be to do
     *
     *     uint32_t threshold = 0x100000000ull % bound;
     *
     * but 64-bit div/mod is costly on 32-bit platforms, so we do a bit
     * of cleverness equivalent to ((2^32-bound) % bound). */
    uint32_t threshold = -bound % bound;

    /* Uniformity guarantees that this loop will terminate.  In practice, it
     * should usually terminate quickly; on average (worst case), at 82.25%
     * rejection rate, we will need to repeat just 1.2 times. */
    for (;;) {
        uint32_t r = pcg32_random_r(rng);
        if (r >= threshold)
            return r % bound;
    }
}
