#include <stddef.h>
#include "api.h"
#include "hal_sim.h"
#include "pack.h"
#include "protection.h"

#define CAPACITY_AH 3.0
#define CELL_R_OHM  0.02

static sim_pack_t pack;
static prot_t prot;
static double time_s;

static int valid_cell(int cell)
{
    return cell >= 0 && cell < SIM_PACK_CELLS;
}

void api_reset(double soc)
{
    sim_pack_init(&pack, CAPACITY_AH, soc, CELL_R_OHM);
    hal_sim_attach(&pack);
    protection_init(&prot, NULL);
    time_s = 0.0;
}

void api_set_load(double current_a)
{
    sim_pack_set_current(&pack, current_a);
}

void api_set_ambient(double temp_c)
{
    pack.ambient_c = temp_c;
}

void api_set_cell_soc(int cell, double soc)
{
    if (valid_cell(cell)) {
        pack.cells[cell].soc = soc < 0.0 ? 0.0 : (soc > 1.0 ? 1.0 : soc);
    }
}

void api_step(int steps)
{
    for (int i = 0; i < steps; i++) {
        protection_step(&prot);
        sim_pack_step(&pack, API_DT_S);
        time_s += API_DT_S;
    }
}

int api_clear_faults(void)
{
    return protection_clear(&prot) ? 1 : 0;
}

double api_time_s(void)    { return time_s; }
double api_load_a(void)    { return pack.load_a; }
double api_current_a(void) { return sim_pack_current(&pack); }
double api_pack_v(void)    { return sim_pack_voltage(&pack); }
int    api_contactor(void) { return pack.contactor_closed ? 1 : 0; }
int    api_faults(void)    { return prot.faults; }
int    api_num_cells(void) { return SIM_PACK_CELLS; }

double api_cell_v(int cell)
{
    return valid_cell(cell) ? sim_cell_voltage(&pack.cells[cell]) : 0.0;
}

double api_cell_soc(int cell)
{
    return valid_cell(cell) ? pack.cells[cell].soc : 0.0;
}

double api_cell_temp_c(int cell)
{
    return valid_cell(cell) ? pack.cells[cell].temp_c : 0.0;
}
