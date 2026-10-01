#ifndef CSV_H
#define CSV_H

#include <stdio.h>
#include "scenario.h"
#include "system.h"

/* CSV output for scenario runs: one header line, then one row per sample. */

void csv_header(FILE *out);
void csv_row(FILE *out, const bms_sys_t *sys);
/* Runs a scenario to completion, writing a row every `every_s` seconds plus
 * the final state. Returns the number of data rows written. */
int  csv_run(FILE *out, const scenario_t *scn, uint32_t every_s);
/* Runs a scenario and writes every CAN frame in candump log format,
 * "(seconds.micros) can0 ID#DATA". Returns the number of frames. */
int  csv_run_can(FILE *out, const scenario_t *scn);

#endif
