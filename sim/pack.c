#include "pack.h"
#include "rng.h"

#define DEFAULT_AMBIENT_C     25.0
#define DEFAULT_BALANCE_R_OHM 33.0

void sim_pack_init(sim_pack_t *pack, double capacity_ah, double soc, double r_ohm)
{
    for (int i = 0; i < SIM_PACK_CELLS; i++) {
        sim_cell_init(&pack->cells[i], capacity_ah, soc, r_ohm);
        pack->balance_on[i] = false;
    }
    pack->balance_r_ohm = DEFAULT_BALANCE_R_OHM;
    pack->contactor_closed = true;
    pack->load_a = 0.0;
    pack->ambient_c = DEFAULT_AMBIENT_C;
}

const sim_mismatch_t SIM_DEFAULT_MISMATCH = {
    .capacity_pct = 1.5,
    .r_pct        = 8.0,
    .soc_pct      = 0.5,
};

/* One gaussian draw scaled by sigma, clamped to +-3 sigma. */
static double spread(sim_rng_t *rng, double sigma)
{
    double g = sim_rng_gauss(rng);
    g = g > 3.0 ? 3.0 : (g < -3.0 ? -3.0 : g);
    return g * sigma;
}

void sim_pack_apply_mismatch(sim_pack_t *pack, const sim_mismatch_t *m, uint32_t seed)
{
    sim_rng_t rng;
    sim_rng_seed(&rng, seed);
    for (int i = 0; i < SIM_PACK_CELLS; i++) {
        sim_cell_t *c = &pack->cells[i];
        c->capacity_ah *= 1.0 + spread(&rng, m->capacity_pct) / 100.0;
        c->r_ohm *= 1.0 + spread(&rng, m->r_pct) / 100.0;
        double soc = c->soc + spread(&rng, m->soc_pct) / 100.0;
        c->soc = soc < 0.0 ? 0.0 : (soc > 1.0 ? 1.0 : soc);
    }
}

void sim_pack_set_current(sim_pack_t *pack, double current_a)
{
    pack->load_a = current_a;
}

void sim_pack_set_contactor(sim_pack_t *pack, bool closed)
{
    pack->contactor_closed = closed;
}

void sim_pack_set_balance(sim_pack_t *pack, int cell, bool on)
{
    if (cell >= 0 && cell < SIM_PACK_CELLS) {
        pack->balance_on[cell] = on;
    }
}

/* Current actually flowing through the string: zero when the contactor is open. */
double sim_pack_current(const sim_pack_t *pack)
{
    return pack->contactor_closed ? pack->load_a : 0.0;
}

double sim_pack_balance_current(const sim_pack_t *pack, int cell)
{
    if (cell < 0 || cell >= SIM_PACK_CELLS || !pack->balance_on[cell]) {
        return 0.0;
    }
    return sim_ocv_from_soc(pack->cells[cell].soc) / pack->balance_r_ohm;
}

void sim_pack_step(sim_pack_t *pack, double dt_s)
{
    double string_a = sim_pack_current(pack);
    for (int i = 0; i < SIM_PACK_CELLS; i++) {
        double cell_a = string_a + sim_pack_balance_current(pack, i);
        sim_cell_step(&pack->cells[i], cell_a, dt_s, pack->ambient_c);
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
