/* HAL backend for the simulated pack: rounds, clamps and optionally adds sensor noise. */
#include <math.h>
#include <stddef.h>
#include "hal.h"
#include "hal_sim.h"
#include "rng.h"

_Static_assert(HAL_NUM_CELLS == SIM_PACK_CELLS, "HAL and sim cell counts differ");

static sim_pack_t *sim;
static hal_noise_t noise;
static bool noisy;
static sim_rng_t rng;
static bool sense_open[HAL_NUM_CELLS];

const hal_noise_t HAL_DEFAULT_NOISE = {
    .cell_mv    = 2.0,
    .current_ma = 10.0,
    .temp_c     = 0.1,
};

void hal_sim_set_noise(const hal_noise_t *n, uint32_t seed)
{
    noisy = (n != NULL);
    if (noisy) {
        noise = *n;
        sim_rng_seed(&rng, seed);
    }
}

void hal_sim_set_sense_open(uint8_t cell, bool open)
{
    if (cell < HAL_NUM_CELLS) {
        sense_open[cell] = open;
    }
}

bool hal_sim_sense_open(uint8_t cell)
{
    return cell < HAL_NUM_CELLS && sense_open[cell];
}

static double jitter(double sigma)
{
    return noisy ? sim_rng_gauss(&rng) * sigma : 0.0;
}

void hal_sim_attach(sim_pack_t *pack)
{
    sim = pack;
}

static bool cell_ok(uint8_t cell)
{
    return sim != NULL && cell < HAL_NUM_CELLS;
}

static double clamp(double x, double lo, double hi)
{
    if (x < lo) {
        return lo;
    }
    if (x > hi) {
        return hi;
    }
    return x;
}

uint16_t hal_read_cell_mv(uint8_t cell)
{
    if (!cell_ok(cell) || sense_open[cell]) {
        return 0;
    }
    double mv = round(sim_cell_voltage(&sim->cells[cell]) * 1000.0 + jitter(noise.cell_mv));
    return (uint16_t)clamp(mv, 0.0, UINT16_MAX);
}

int32_t hal_read_pack_current_ma(void)
{
    if (sim == NULL) {
        return 0;
    }
    double ma = round(sim_pack_current(sim) * 1000.0 + jitter(noise.current_ma));
    return (int32_t)clamp(ma, INT32_MIN, INT32_MAX);
}

int16_t hal_read_cell_temp_dc(uint8_t cell)
{
    if (!cell_ok(cell)) {
        return 0;
    }
    double dc = round((sim->cells[cell].temp_c + jitter(noise.temp_c)) * 10.0);
    return (int16_t)clamp(dc, INT16_MIN, INT16_MAX);
}

void hal_set_contactor(bool closed)
{
    if (sim != NULL) {
        sim_pack_set_contactor(sim, closed);
    }
}

bool hal_get_contactor(void)
{
    return sim != NULL && sim->contactor_closed;
}

void hal_set_balance(uint8_t cell, bool on)
{
    if (cell_ok(cell)) {
        sim_pack_set_balance(sim, cell, on);
    }
}

bool hal_get_balance(uint8_t cell)
{
    return cell_ok(cell) && sim->balance_on[cell];
}

static hal_can_frame_t can_ring[HAL_SIM_CAN_RING];
static uint32_t can_time[HAL_SIM_CAN_RING];
static uint32_t can_total;
static uint32_t can_now_ms;

void hal_sim_can_set_time_ms(uint32_t ms)
{
    can_now_ms = ms;
}

void hal_sim_can_clear(void)
{
    can_total = 0;
    can_now_ms = 0;
}

bool hal_can_send(const hal_can_frame_t *frame)
{
    if (frame == NULL || frame->dlc > HAL_CAN_MAX_DLC || frame->id > 0x7FFu) {
        return false;
    }
    can_ring[can_total % HAL_SIM_CAN_RING] = *frame;
    can_time[can_total % HAL_SIM_CAN_RING] = can_now_ms;
    can_total++;
    return true;
}

uint32_t hal_sim_can_total(void)
{
    return can_total;
}

bool hal_sim_can_get(uint32_t seq, hal_can_frame_t *frame, uint32_t *time_ms)
{
    if (seq >= can_total || can_total - seq > HAL_SIM_CAN_RING) {
        return false;
    }
    *frame = can_ring[seq % HAL_SIM_CAN_RING];
    *time_ms = can_time[seq % HAL_SIM_CAN_RING];
    return true;
}
