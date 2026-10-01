#include <stddef.h>
#include "api.h"
#include "scenario.h"
#include "system.h"

static bms_sys_t sys;
static scn_run_t run;
static int scenario_active;
static int noise_on;

static int valid_cell(int cell)
{
    return cell >= 0 && cell < SIM_PACK_CELLS;
}

void api_reset(double soc)
{
    sys_init(&sys, soc);
    scenario_active = 0;
    noise_on = 1;
}

void api_set_load(double current_a)
{
    sim_pack_set_current(&sys.pack, current_a);
}

void api_set_ambient(double temp_c)
{
    sys.pack.ambient_c = temp_c;
}

void api_set_cell_soc(int cell, double soc)
{
    sys_set_cell_soc(&sys, cell, soc);
}

void api_step(int steps)
{
    for (int i = 0; i < steps; i++) {
        if (scenario_active && scenario_done(&run, &sys)) {
            return;
        }
        sys_step(&sys);
        if (scenario_active) {
            scenario_apply(&run, &sys);
        }
    }
}

void api_set_balancing(int enabled)
{
    sys.balancing = enabled != 0;
}

void api_set_noise(int enabled)
{
    noise_on = enabled != 0;
    sys_set_noise(noise_on);
}

int api_noise(void) { return noise_on; }

int api_clear_faults(void)
{
    return protection_clear(&sys.prot) ? 1 : 0;
}

int api_start_scenario(int index)
{
    const scenario_t *scn = scenario_get(index);
    if (scn == NULL) {
        return 0;
    }
    scenario_start(&run, scn, &sys);
    scenario_active = 1;
    noise_on = 1;
    return 1;
}

int api_scenario_count(void)  { return scenario_count(); }
int api_scenario_active(void) { return scenario_active; }
int api_scenario_done(void)   { return scenario_active && scenario_done(&run, &sys); }

const char *api_scenario_name(int index)
{
    const scenario_t *s = scenario_get(index);
    return s != NULL ? s->name : "";
}

const char *api_scenario_desc(int index)
{
    const scenario_t *s = scenario_get(index);
    return s != NULL ? s->desc : "";
}

double api_scenario_duration_s(void)
{
    return scenario_active ? run.scn->duration_s : 0.0;
}

double api_time_s(void)    { return sys.time_s; }
double api_load_a(void)    { return sys.pack.load_a; }
double api_ambient_c(void) { return sys.pack.ambient_c; }
double api_current_a(void) { return sim_pack_current(&sys.pack); }
double api_pack_v(void)    { return sim_pack_voltage(&sys.pack); }
int    api_contactor(void) { return sys.pack.contactor_closed ? 1 : 0; }
int    api_faults(void)    { return sys.prot.faults; }
int    api_num_cells(void) { return SIM_PACK_CELLS; }
int    api_balance_mask(void) { return sys.bal.mask; }
int    api_balancing(void) { return sys.balancing ? 1 : 0; }

double api_soc_est_pct(void)
{
    return soc_get(&sys.soc) / 100.0;
}

double api_cell_v(int cell)
{
    return valid_cell(cell) ? sim_cell_voltage(&sys.pack.cells[cell]) : 0.0;
}

double api_cell_soc(int cell)
{
    return valid_cell(cell) ? sys.pack.cells[cell].soc : 0.0;
}

double api_cell_temp_c(int cell)
{
    return valid_cell(cell) ? sys.pack.cells[cell].temp_c : 0.0;
}

double api_cell_capacity_ah(int cell)
{
    return valid_cell(cell) ? sys.pack.cells[cell].capacity_ah : 0.0;
}

double api_cell_r_mohm(int cell)
{
    return valid_cell(cell) ? sys.pack.cells[cell].r_ohm * 1000.0 : 0.0;
}

int api_cell_meas_mv(int cell)
{
    return valid_cell(cell) ? sys.prot.last.cell_mv[cell] : 0;
}

double api_ekf_pct(void)
{
    return ekf_pack_cpct(&sys.ekf) / 100.0;
}

double api_ekf_sigma_pct(void)
{
    return ekf_sigma_cpct(&sys.ekf, ekf_min_cell(&sys.ekf)) / 100.0;
}

double api_ekf_cell_pct(int cell)
{
    return valid_cell(cell) ? ekf_cell_cpct(&sys.ekf, (uint8_t)cell) / 100.0 : 0.0;
}

void api_corrupt_estimates(double pct)
{
    uint16_t cpct = (uint16_t)(pct < 0.0 ? 0.0 : (pct > 100.0 ? 10000.0 : pct * 100.0));
    soc_set(&sys.soc, cpct);
    ekf_set(&sys.ekf, cpct);
}
