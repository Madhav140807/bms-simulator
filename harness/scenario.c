/* Built in scenario table and the event player that applies it to the system. */
#include <stddef.h>
#include <string.h>
#include "scenario.h"

#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

static const scn_event_t discharge_1c[] = {
    { 0, EV_LOAD, 0, 3.0 },
};

static const scn_event_t charge[] = {
    { 0, EV_LOAD, 0, -2.9 },
};

static const scn_event_t overcurrent[] = {
    { 0,   EV_LOAD, 0, 3.0 },
    { 60,  EV_LOAD, 0, 12.0 },
    { 120, EV_LOAD, 0, 3.0 },
    { 150, EV_CLEAR_FAULTS, 0, 0.0 },
};

static const scn_event_t over_temp[] = {
    { 0, EV_AMBIENT, 0, 65.0 },
    { 0, EV_LOAD, 0, 2.0 },
};

static const scn_event_t imbalance[] = {
    { 0, EV_CELL_SOC, 0, 0.80 },
    { 0, EV_CELL_SOC, 1, 0.72 },
    { 0, EV_CELL_SOC, 2, 0.66 },
    { 0, EV_CELL_SOC, 3, 0.75 },
};

static const scn_event_t weak_cell[] = {
    { 0, EV_CELL_SOC, 2, 0.30 },
    { 0, EV_LOAD, 0, 3.0 },
};

static const scn_event_t cell_overheat[] = {
    { 0,   EV_LOAD, 0, 1.0 },
    { 120, EV_INJECT, 1, SYS_INJ_HEATER },
};

static const scn_event_t internal_short[] = {
    { 0,  EV_LOAD, 0, 1.0 },
    { 60, EV_INJECT, 2, SYS_INJ_SHORT },
};

static const scn_event_t sensor_failure[] = {
    { 0,   EV_LOAD, 0, 3.0 },
    { 300, EV_INJECT, 3, SYS_INJ_SENSOR },
};

static const scenario_t scenarios[] = {
    { "discharge_1c", "1C discharge from full until under voltage trips, then rest",
      1.0, 4200, discharge_1c, COUNT(discharge_1c) },
    { "charge", "Charge at 2.9 A from 20 % until over voltage trips, then rest",
      0.2, 3600, charge, COUNT(charge) },
    { "overcurrent", "12 A pulse trips over current; load drops back and faults are cleared",
      0.8, 600, overcurrent, COUNT(overcurrent) },
    { "over_temp", "2 A load at 65 C ambient until over temperature trips",
      0.9, 3600, over_temp, COUNT(over_temp) },
    { "imbalance", "Cells at 80/72/66/75 % resting while passive balancing evens them out",
      0.8, 18000, imbalance, COUNT(imbalance) },
    { "weak_cell", "One cell at 30 % limits a 1C discharge: early under voltage trip",
      1.0, 1800, weak_cell, COUNT(weak_cell) },
    { "cell_overheat", "A 3 W external heat source on cell 2 until over temperature trips",
      0.8, 1800, cell_overheat, COUNT(cell_overheat) },
    { "internal_short", "A 2 ohm internal short in cell 3: it drains and heats until protection trips",
      0.8, 1800, internal_short, COUNT(internal_short) },
    { "sensor_failure", "Cell 4 voltage sense wire opens: sensor fault instead of a false under voltage",
      0.8, 600, sensor_failure, COUNT(sensor_failure) },
};

int scenario_count(void)
{
    return COUNT(scenarios);
}

const scenario_t *scenario_get(int index)
{
    return (index >= 0 && index < COUNT(scenarios)) ? &scenarios[index] : NULL;
}

const scenario_t *scenario_find(const char *name)
{
    for (int i = 0; name != NULL && i < COUNT(scenarios); i++) {
        if (strcmp(scenarios[i].name, name) == 0) {
            return &scenarios[i];
        }
    }
    return NULL;
}

static void apply_event(const scn_event_t *ev, bms_sys_t *sys)
{
    switch (ev->kind) {
    case EV_LOAD:
        sim_pack_set_current(&sys->pack, ev->value);
        break;
    case EV_AMBIENT:
        sys->pack.ambient_c = ev->value;
        break;
    case EV_CELL_SOC:
        sys_set_cell_soc(sys, ev->cell, ev->value);
        break;
    case EV_CLEAR_FAULTS:
        protection_clear(&sys->prot);
        break;
    case EV_BALANCING:
        sys->balancing = ev->value != 0.0;
        break;
    case EV_INJECT:
        if (ev->value == 0.0) {
            sys_clear_injections(sys);
        } else {
            sys_inject(sys, ev->cell, (unsigned)ev->value, true);
        }
        break;
    }
}

void scenario_start(scn_run_t *run, const scenario_t *scn, bms_sys_t *sys)
{
    run->scn = scn;
    run->next = 0;
    sys_init(sys, scn->start_soc);
    scenario_apply(run, sys);
    /* Cell SOC events at t = 0 change the OCV, so reseed the SOC estimates.
     * Seed at rest, as a BMS does at power up before the load is on. */
    double load_a = sys->pack.load_a;
    sim_pack_set_current(&sys->pack, 0.0);
    soc_init(&sys->soc, NULL);
    ekf_init(&sys->ekf, NULL);
    sim_pack_set_current(&sys->pack, load_a);
}

void scenario_apply(scn_run_t *run, bms_sys_t *sys)
{
    while (run->next < run->scn->n_events &&
           run->scn->events[run->next].at_s <= sys->time_s) {
        apply_event(&run->scn->events[run->next], sys);
        run->next++;
    }
}

bool scenario_done(const scn_run_t *run, const bms_sys_t *sys)
{
    return sys->time_s >= run->scn->duration_s;
}
