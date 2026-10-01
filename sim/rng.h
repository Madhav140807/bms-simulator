#ifndef SIM_RNG_H
#define SIM_RNG_H

#include <stdint.h>

/* Small deterministic PRNG (xorshift32) so runs are reproducible and the
 * native and WebAssembly builds produce identical numbers. */

typedef struct {
    uint32_t state;
} sim_rng_t;

void     sim_rng_seed(sim_rng_t *rng, uint32_t seed);
uint32_t sim_rng_next(sim_rng_t *rng);
double   sim_rng_uniform(sim_rng_t *rng);   /* [0, 1) */
/* Approximately standard normal (sum of 12 uniforms minus 6, |x| <= 6).
 * Uses no libm calls, so results match across C libraries. */
double   sim_rng_gauss(sim_rng_t *rng);

#endif
