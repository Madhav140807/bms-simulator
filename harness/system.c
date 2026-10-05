/* Wires sim, HAL and firmware into one system stepped one second at a time. */
#include <stddef.h>
#include "hal_sim.h"
#include "system.h"

void sys_init(bms_sys_t *sys, double soc)
{
    sim_pack_init(&sys->pack, SYS_CAPACITY_AH, soc, SYS_CELL_R_OHM);
    sim_pack_apply_mismatch(&sys->pack, &SIM_DEFAULT_MISMATCH, SYS_MISMATCH_SEED);
    hal_sim_attach(&sys->pack);
    sys_set_noise(true);
    sys_clear_injections(sys);
    protection_init(&sys->prot, NULL);
    soc_init(&sys->soc, NULL);
    ekf_init(&sys->ekf, NULL);
    balance_init(&sys->bal, NULL);
    can_tx_init(&sys->can);
    hal_sim_can_clear();
    sys->balancing = true;
    sys->time_s = 0;
}

void sys_step(bms_sys_t *sys)
{
    hal_sim_attach(&sys->pack);
    protection_step(&sys->prot);
    soc_step(&sys->soc, SYS_DT_MS);
    ekf_step(&sys->ekf, SYS_DT_MS);
    balance_step(&sys->bal, sys->balancing && sys->prot.faults == PROT_FAULT_NONE);
    can_status_t st;
    can_collect(&st, &sys->prot, &sys->soc, &sys->ekf, &sys->bal);
    hal_sim_can_set_time_ms(sys->time_s * 1000u);
    can_tx_step(&sys->can, &st);
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

void sys_inject(bms_sys_t *sys, int cell, unsigned flags, bool on)
{
    if (cell < 0 || cell >= SIM_PACK_CELLS) {
        return;
    }
    if (flags & SYS_INJ_HEATER) {
        sim_pack_set_heater(&sys->pack, cell, on ? SYS_HEATER_W : 0.0);
    }
    if (flags & SYS_INJ_SHORT) {
        sim_pack_set_short(&sys->pack, cell, on ? SYS_SHORT_R_OHM : 0.0);
    }
    if (flags & SYS_INJ_SENSOR) {
        hal_sim_set_sense_open((uint8_t)cell, on);
    }
}

unsigned sys_injected(const bms_sys_t *sys, int cell)
{
    if (cell < 0 || cell >= SIM_PACK_CELLS) {
        return 0;
    }
    unsigned f = 0;
    f |= sys->pack.heater_w[cell] > 0.0 ? SYS_INJ_HEATER : 0u;
    f |= sys->pack.short_r_ohm[cell] > 0.0 ? SYS_INJ_SHORT : 0u;
    f |= hal_sim_sense_open((uint8_t)cell) ? SYS_INJ_SENSOR : 0u;
    return f;
}

void sys_clear_injections(bms_sys_t *sys)
{
    for (int i = 0; i < SIM_PACK_CELLS; i++) {
        sys_inject(sys, i, SYS_INJ_HEATER | SYS_INJ_SHORT | SYS_INJ_SENSOR, false);
    }
}
