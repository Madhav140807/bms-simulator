#ifndef SIM_PACK_H
#define SIM_PACK_H

#include <stdbool.h>
#include <stdint.h>
#include "cell.h"

/* Series string of cells sharing one pack current, with a main contactor
 * and a passive balancing bleed resistor across each cell. */

#define SIM_PACK_CELLS 4

typedef struct {
    sim_cell_t cells[SIM_PACK_CELLS];
    bool   balance_on[SIM_PACK_CELLS];
    double heater_w[SIM_PACK_CELLS];     /* injected external heating */
    double short_r_ohm[SIM_PACK_CELLS];  /* injected internal short, 0 = none */
    double balance_r_ohm;    /* bleed resistor per cell */
    bool   contactor_closed;
    double load_a;           /* requested load, positive = discharge */
    double ambient_c;
} sim_pack_t;

/* Manufacturing spread between cells, as one standard deviation in percent
 * of the nominal value (SOC spread in percentage points). */
typedef struct {
    double capacity_pct;
    double r_pct;
    double soc_pct;
} sim_mismatch_t;

extern const sim_mismatch_t SIM_DEFAULT_MISMATCH;

/* Every cell starts identical. */
void   sim_pack_init(sim_pack_t *pack, double capacity_ah, double soc, double r_ohm);
/* Randomly perturbs each cell (seeded, clamped to +-3 sigma). */
void   sim_pack_apply_mismatch(sim_pack_t *pack, const sim_mismatch_t *m, uint32_t seed);
void   sim_pack_set_current(sim_pack_t *pack, double current_a);
void   sim_pack_set_contactor(sim_pack_t *pack, bool closed);
void   sim_pack_set_balance(sim_pack_t *pack, int cell, bool on);
double sim_pack_current(const sim_pack_t *pack);
double sim_pack_balance_current(const sim_pack_t *pack, int cell);
/* Fault injection. An internal short drains the cell through r_ohm and
 * dissipates V^2/R inside it; the contactor cannot stop it. */
void   sim_pack_set_heater(sim_pack_t *pack, int cell, double watts);
void   sim_pack_set_short(sim_pack_t *pack, int cell, double r_ohm);
double sim_pack_short_current(const sim_pack_t *pack, int cell);
void   sim_pack_step(sim_pack_t *pack, double dt_s);
double sim_pack_voltage(const sim_pack_t *pack);

#endif
