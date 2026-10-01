#ifndef BALANCE_H
#define BALANCE_H

#include <stdbool.h>
#include <stdint.h>
#include "hal.h"

/* Passive cell balancing: bleeds charge from cells that sit above the
 * lowest cell. A cell starts bleeding when it is more than start_mv above
 * the lowest cell and stops once it is within stop_mv (hysteresis).
 * Balancing is inhibited while discharging harder than max_dsg_ma, when the
 * lowest cell is below min_mv, when any cell is above max_temp_dc, or when
 * the caller does not allow it (e.g. a protection fault is latched). */

typedef struct {
    uint16_t start_mv;     /* delta above lowest cell to start bleeding */
    uint16_t stop_mv;      /* delta above lowest cell to stop bleeding */
    uint16_t min_mv;       /* no balancing below this lowest cell voltage */
    int16_t  max_temp_dc;  /* no balancing above this temperature */
    int32_t  max_dsg_ma;   /* no balancing while discharging harder */
} bal_config_t;

typedef struct {
    uint16_t cell_mv[HAL_NUM_CELLS];
    int16_t  cell_temp_dc[HAL_NUM_CELLS];
    int32_t  current_ma;
} bal_sample_t;

typedef struct {
    bal_config_t cfg;
    uint8_t      mask;  /* bit i set = cell i bleeding */
} bal_t;

extern const bal_config_t BAL_DEFAULT_CONFIG;

/* cfg may be NULL to use BAL_DEFAULT_CONFIG. Turns all bleeders off. */
void    balance_init(bal_t *b, const bal_config_t *cfg);
bool    balance_inhibited(const bal_config_t *cfg, const bal_sample_t *s);
/* Pure selection: next bleed mask given the current one. */
uint8_t balance_select(const bal_config_t *cfg, const bal_sample_t *s, uint8_t mask);
void    balance_read_sample(bal_sample_t *s);
/* Selects (or clears, if !allowed) and drives the HAL bleed switches. */
void    balance_update(bal_t *b, const bal_sample_t *s, bool allowed);
void    balance_step(bal_t *b, bool allowed);

#endif
