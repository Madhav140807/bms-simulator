#include "cell.h"

#define OCV_POINTS 11
#define DEFAULT_TEMP_C 25.0
#define DEFAULT_HEAT_CAP_J_PER_C 40.0
#define DEFAULT_COOLING_W_PER_C 0.05
#define SECONDS_PER_HOUR 3600.0

/* Open circuit voltage at 0%, 10%, ... 100% SOC. */
static const double ocv_table[OCV_POINTS] = {
    3.00, 3.45, 3.55, 3.62, 3.68, 3.74, 3.82, 3.90, 3.98, 4.07, 4.20
};

static double clamp01(double x)
{
    if (x < 0.0) {
        return 0.0;
    }
    if (x > 1.0) {
        return 1.0;
    }
    return x;
}

void sim_cell_init(sim_cell_t *cell, double capacity_ah, double soc, double r_ohm)
{
    cell->capacity_ah = capacity_ah;
    cell->soc = clamp01(soc);
    cell->r_ohm = r_ohm;
    cell->temp_c = DEFAULT_TEMP_C;
    cell->heat_cap_j_per_c = DEFAULT_HEAT_CAP_J_PER_C;
    cell->cooling_w_per_c = DEFAULT_COOLING_W_PER_C;
    cell->current_a = 0.0;
    cell->extra_heat_w = 0.0;
}

double sim_ocv_from_soc(double soc)
{
    double pos = clamp01(soc) * (OCV_POINTS - 1);
    int idx = (int)pos;
    if (idx >= OCV_POINTS - 1) {
        return ocv_table[OCV_POINTS - 1];
    }
    double frac = pos - idx;
    return ocv_table[idx] + frac * (ocv_table[idx + 1] - ocv_table[idx]);
}

double sim_cell_voltage(const sim_cell_t *cell)
{
    return sim_ocv_from_soc(cell->soc) - cell->current_a * cell->r_ohm;
}

static void step_thermal(sim_cell_t *cell, double dt_s, double ambient_c)
{
    double heat_w = cell->current_a * cell->current_a * cell->r_ohm + cell->extra_heat_w;
    double loss_w = cell->cooling_w_per_c * (cell->temp_c - ambient_c);
    cell->temp_c += (heat_w - loss_w) * dt_s / cell->heat_cap_j_per_c;
}

void sim_cell_step(sim_cell_t *cell, double current_a, double dt_s, double ambient_c)
{
    double delta_ah = current_a * dt_s / SECONDS_PER_HOUR;
    cell->current_a = current_a;
    cell->soc = clamp01(cell->soc - delta_ah / cell->capacity_ah);
    step_thermal(cell, dt_s, ambient_c);
}
