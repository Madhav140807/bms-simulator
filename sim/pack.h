#ifndef SIM_PACK_H
#define SIM_PACK_H

#include "cell.h"

/* Series string of cells sharing one pack current. */

#define SIM_PACK_CELLS 4

typedef struct {
    sim_cell_t cells[SIM_PACK_CELLS];
    double current_a;  /* positive = discharge */
    double ambient_c;
} sim_pack_t;

void   sim_pack_init(sim_pack_t *pack, double capacity_ah, double soc, double r_ohm);
void   sim_pack_set_current(sim_pack_t *pack, double current_a);
void   sim_pack_step(sim_pack_t *pack, double dt_s);
double sim_pack_voltage(const sim_pack_t *pack);

#endif
