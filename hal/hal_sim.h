#ifndef HAL_SIM_H
#define HAL_SIM_H

#include "pack.h"

/* Binds the HAL to a simulated pack. Used by the harness and tests only;
 * firmware must never include this header. Until a pack is attached, reads
 * return 0 and writes are ignored. */

void hal_sim_attach(sim_pack_t *pack);

/* Gaussian measurement noise, one standard deviation per reading. */
typedef struct {
    double cell_mv;
    double current_ma;
    double temp_c;
} hal_noise_t;

extern const hal_noise_t HAL_DEFAULT_NOISE;

/* Enables seeded noise on every read; NULL turns it off (the default). */
void hal_sim_set_noise(const hal_noise_t *noise, uint32_t seed);

#endif
