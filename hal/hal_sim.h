#ifndef HAL_SIM_H
#define HAL_SIM_H

#include "pack.h"

/* Binds the HAL to a simulated pack. Used by the harness and tests only;
 * firmware must never include this header. Until a pack is attached, reads
 * return 0 and writes are ignored. */

void hal_sim_attach(sim_pack_t *pack);

#endif
