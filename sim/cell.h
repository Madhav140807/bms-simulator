#ifndef SIM_CELL_H
#define SIM_CELL_H

/* Single lithium ion cell model (NMC chemistry).
 * Sign convention: positive current = discharge. */

typedef struct {
    double capacity_ah;     /* rated capacity */
    double soc;             /* state of charge, 0.0 .. 1.0 */
    double r_ohm;           /* internal series resistance */
    double temp_c;          /* cell temperature */
    double heat_cap_j_per_c;/* thermal mass */
    double cooling_w_per_c; /* heat transfer to ambient */
    double current_a;       /* current from the last step */
    double extra_heat_w;    /* heat from outside the I^2*R model */
} sim_cell_t;

void   sim_cell_init(sim_cell_t *cell, double capacity_ah, double soc, double r_ohm);
double sim_ocv_from_soc(double soc);
double sim_cell_voltage(const sim_cell_t *cell);
void   sim_cell_step(sim_cell_t *cell, double current_a, double dt_s, double ambient_c);

#endif
