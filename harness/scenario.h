#ifndef SCENARIO_H
#define SCENARIO_H

#include <stdbool.h>
#include <stdint.h>
#include "system.h"

/* Scripted test scenarios: a start SOC, a duration and a time ordered list
 * of events applied to the system as the clock reaches them. */

typedef enum {
    EV_LOAD,          /* value = pack load in A, positive = discharge */
    EV_AMBIENT,       /* value = ambient temperature in C */
    EV_CELL_SOC,      /* cell, value = SOC 0..1 */
    EV_CLEAR_FAULTS,
    EV_BALANCING,     /* value != 0 enables balancing */
    EV_INJECT         /* cell, value = SYS_INJ_* flags to turn on (0 clears all) */
} scn_kind_t;

typedef struct {
    uint32_t   at_s;
    scn_kind_t kind;
    int        cell;
    double     value;
} scn_event_t;

typedef struct {
    const char        *name;
    const char        *desc;
    double             start_soc;
    uint32_t           duration_s;
    const scn_event_t *events;
    int                n_events;
} scenario_t;

typedef struct {
    const scenario_t *scn;
    int               next;  /* index of the next event to apply */
} scn_run_t;

int               scenario_count(void);
const scenario_t *scenario_get(int index);           /* NULL if out of range */
const scenario_t *scenario_find(const char *name);   /* NULL if unknown */

/* Initialises the system at the scenario's start SOC and applies t = 0 events. */
void scenario_start(scn_run_t *run, const scenario_t *scn, bms_sys_t *sys);
/* Applies every pending event due at the current system time. */
void scenario_apply(scn_run_t *run, bms_sys_t *sys);
bool scenario_done(const scn_run_t *run, const bms_sys_t *sys);

#endif
