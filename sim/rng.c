#include "rng.h"

void sim_rng_seed(sim_rng_t *rng, uint32_t seed)
{
    rng->state = (seed != 0) ? seed : 0x9E3779B9u;
}

uint32_t sim_rng_next(sim_rng_t *rng)
{
    uint32_t x = rng->state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng->state = x;
    return x;
}

double sim_rng_uniform(sim_rng_t *rng)
{
    return sim_rng_next(rng) / 4294967296.0;
}

double sim_rng_gauss(sim_rng_t *rng)
{
    double sum = 0.0;
    for (int i = 0; i < 12; i++) {
        sum += sim_rng_uniform(rng);
    }
    return sum - 6.0;
}
