#include "pack.h"

#define DEFAULT_AMBIENT_C 25.0

void sim_pack_init(sim_pack_t *pack, double capacity_ah, double soc, double r_ohm)
{
    for (int i = 0; i < SIM_PACK_CELLS; i++) {
        sim_cell_init(&pack->cells[i], capacity_ah, soc, r_ohm);
    }
    pack->current_a = 0.0;
    pack->ambient_c = DEFAULT_AMBIENT_C;
}

void sim_pack_set_current(sim_pack_t *pack, double current_a)
{
    pack->current_a = current_a;
}

void sim_pack_step(sim_pack_t *pack, double dt_s)
{
    for (int i = 0; i < SIM_PACK_CELLS; i++) {
        sim_cell_step(&pack->cells[i], pack->current_a, dt_s, pack->ambient_c);
    }
}

double sim_pack_voltage(const sim_pack_t *pack)
{
    double total = 0.0;
    for (int i = 0; i < SIM_PACK_CELLS; i++) {
        total += sim_cell_voltage(&pack->cells[i]);
    }
    return total;
}
