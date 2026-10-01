#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdbool.h>
#include <stdint.h>
#include "balance.h"
#include "pack.h"
#include "protection.h"
#include "soc.h"

/* The whole simulated system: one pack wired through the HAL to every
 * firmware module. Shared by the CSV scenario runner and the WASM API. */

#define SYS_CAPACITY_AH 3.0
#define SYS_CELL_R_OHM  0.02
#define SYS_DT_MS       1000u

typedef struct {
    sim_pack_t pack;
    prot_t     prot;
    soc_t      soc;
    bal_t      bal;
    bool       balancing;  /* user enable; faults also stop balancing */
    uint32_t   time_s;
} bms_sys_t;

/* Initialises every cell at `soc`, attaches the HAL and starts the firmware. */
void   sys_init(bms_sys_t *sys, double soc);
/* One second: run protection, SOC and balancing, then advance the sim. */
void   sys_step(bms_sys_t *sys);
void   sys_set_cell_soc(bms_sys_t *sys, int cell, double soc);
double sys_cell_spread_mv(const bms_sys_t *sys);

#endif
