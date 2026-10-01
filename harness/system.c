#include <stddef.h>
#include "hal_sim.h"
#include "system.h"

void sys_init(bms_sys_t *sys, double soc)
{
    sim_pack_init(&sys->pack, SYS_CAPACITY_AH, soc, SYS_CELL_R_OHM);
    sim_pack_apply_mismatch(&sys->pack, &SIM_DEFAULT_MISMATCH, SYS_MISMATCH_SEED);
    hal_sim_attach(&sys->pack);
    sys_set_noise(true);
    protection_init(&sys->prot, NULL);
    soc_init(&sys->soc, NULL);
    balance_init(&sys->bal, NULL);
    sys->balancing = true;
    sys->time_s = 0;
}

void sys_step(bms_sys_t *sys)
{
    hal_sim_attach(&sys->pack);
    protection_step(&sys->prot);
    soc_step(&sys->soc, SYS_DT_MS);
    balance_step(&sys->bal, sys->balancing && sys->prot.faults == PROT_FAULT_NONE);
    sim_pack_step(&sys->pack, SYS_DT_MS / 1000.0);
    sys->time_s++;
}

void sys_set_noise(bool on)
{
    hal_sim_set_noise(on ? &HAL_DEFAULT_NOISE : NULL, SYS_NOISE_SEED);
}

void sys_set_cell_soc(bms_sys_t *sys, int cell, double soc)
{
    if (cell >= 0 && cell < SIM_PACK_CELLS) {
        sys->pack.cells[cell].soc = soc < 0.0 ? 0.0 : (soc > 1.0 ? 1.0 : soc);
    }
}

double sys_cell_spread_mv(const bms_sys_t *sys)
{
    double lo = sim_cell_voltage(&sys->pack.cells[0]);
    double hi = lo;
    for (int i = 1; i < SIM_PACK_CELLS; i++) {
        double v = sim_cell_voltage(&sys->pack.cells[i]);
        lo = v < lo ? v : lo;
        hi = v > hi ? v : hi;
    }
    return (hi - lo) * 1000.0;
}
