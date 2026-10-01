#include <math.h>
#include <stddef.h>
#include "hal.h"
#include "hal_sim.h"

_Static_assert(HAL_NUM_CELLS == SIM_PACK_CELLS, "HAL and sim cell counts differ");

static sim_pack_t *sim;

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
    if (!cell_ok(cell)) {
        return 0;
    }
    double mv = round(sim_cell_voltage(&sim->cells[cell]) * 1000.0);
    return (uint16_t)clamp(mv, 0.0, UINT16_MAX);
}

int32_t hal_read_pack_current_ma(void)
{
    if (sim == NULL) {
        return 0;
    }
    double ma = round(sim_pack_current(sim) * 1000.0);
    return (int32_t)clamp(ma, INT32_MIN, INT32_MAX);
}

int16_t hal_read_cell_temp_dc(uint8_t cell)
{
    if (!cell_ok(cell)) {
        return 0;
    }
    double dc = round(sim->cells[cell].temp_c * 10.0);
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
