#ifndef HAL_SIM_H
#define HAL_SIM_H

#include "hal.h"
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

/* Fault injection: an open voltage sense wire makes the cell read 0 mV. */
void hal_sim_set_sense_open(uint8_t cell, bool open);
bool hal_sim_sense_open(uint8_t cell);

/* Simulated CAN bus: every sent frame is stored with a timestamp in a ring
 * of HAL_SIM_CAN_RING frames. Sequence numbers count every frame ever sent. */
#define HAL_SIM_CAN_RING 1024u

void     hal_sim_can_set_time_ms(uint32_t ms);
void     hal_sim_can_clear(void);
uint32_t hal_sim_can_total(void);
/* Copies frame `seq` if it is still in the ring. */
bool     hal_sim_can_get(uint32_t seq, hal_can_frame_t *frame, uint32_t *time_ms);

#endif
