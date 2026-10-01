#ifndef SIM_PACK_H
#define SIM_PACK_H

#include <stdbool.h>
#include "cell.h"

/* Series string of cells sharing one pack current, with a main contactor
 * and a passive balancing bleed resistor across each cell. */

#define SIM_PACK_CELLS 4

typedef struct {
    sim_cell_t cells[SIM_PACK_CELLS];
    bool   balance_on[SIM_PACK_CELLS];
    double balance_r_ohm;    /* bleed resistor per cell */
    bool   contactor_closed;
    double load_a;           /* requested load, positive = discharge */
    double ambient_c;
} sim_pack_t;

void   sim_pack_init(sim_pack_t *pack, double capacity_ah, double soc, double r_ohm);
void   sim_pack_set_current(sim_pack_t *pack, double current_a);
void   sim_pack_set_contactor(sim_pack_t *pack, bool closed);
void   sim_pack_set_balance(sim_pack_t *pack, int cell, bool on);
double sim_pack_current(const sim_pack_t *pack);
double sim_pack_balance_current(const sim_pack_t *pack, int cell);
void   sim_pack_step(sim_pack_t *pack, double dt_s);
double sim_pack_voltage(const sim_pack_t *pack);

#endif
