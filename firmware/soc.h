#ifndef SOC_H
#define SOC_H

#include <stdint.h>
#include "hal.h"

/* State of charge estimation for the series pack.
 * Coulomb counting integrates pack current every step. When the pack has
 * rested (|current| <= rest_ma) for rest_ms, the estimate is corrected from
 * the open circuit voltage of the lowest cell, which limits a series pack.
 * SOC is reported in hundredths of a percent (0 .. 10000). */

#define SOC_FULL_CPCT 10000u

typedef struct {
    uint32_t capacity_mah;
    uint16_t rest_ma;   /* max |current| still treated as resting */
    uint32_t rest_ms;   /* rest time before OCV correction applies */
} soc_config_t;

typedef struct {
    soc_config_t cfg;
    int64_t      charge_mams;     /* remaining charge, mA*ms */
    uint32_t     rest_elapsed_ms;
} soc_t;

extern const soc_config_t SOC_DEFAULT_CONFIG;

/* Lookup OCV (mV) -> SOC (0.01 %), clamped to the table ends. */
uint16_t soc_from_ocv_mv(uint16_t mv);

/* cfg may be NULL to use SOC_DEFAULT_CONFIG. Assumes the pack is at rest
 * and seeds the estimate from the lowest cell voltage read via the HAL. */
void     soc_init(soc_t *s, const soc_config_t *cfg);
void     soc_set(soc_t *s, uint16_t soc_cpct);
uint16_t soc_get(const soc_t *s);

/* Pure update from a current sample and the lowest cell voltage. */
void     soc_update(soc_t *s, int32_t current_ma, uint16_t min_cell_mv, uint32_t dt_ms);
/* Reads the HAL and calls soc_update(). */
void     soc_step(soc_t *s, uint32_t dt_ms);

uint16_t soc_min_cell_mv(void);

#endif
