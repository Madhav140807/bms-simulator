#ifndef EKF_H
#define EKF_H

#include <stdint.h>
#include "hal.h"

/* Per cell extended Kalman filter for state of charge.
 *   state      x = SOC (fraction 0..1), variance P
 *   predict    x -= I * dt / capacity          P += q * dt
 *   measure    z = cell voltage (mV), h(x) = OCV(x) - I * R0
 *   correct    H = dOCV/dSOC, K = P H / (H^2 P + r), x += K (z - h(x))
 * Unlike plain coulomb counting it corrects itself under load, so a wrong
 * starting estimate converges within seconds. The pack SOC is the lowest
 * cell's estimate. Float math (single precision). */

typedef struct {
    uint32_t capacity_mah;  /* nominal cell capacity */
    float    r0_mohm;       /* nominal cell series resistance */
    float    q_per_s;       /* process noise variance per second (SOC^2) */
    float    r_mv2;         /* measurement noise variance (mV^2) */
    float    p0;            /* initial variance (SOC^2) */
} ekf_config_t;

typedef struct {
    ekf_config_t cfg;
    float        soc[HAL_NUM_CELLS];
    float        var[HAL_NUM_CELLS];
} ekf_t;

extern const ekf_config_t EKF_DEFAULT_CONFIG;

/* cfg may be NULL for the defaults. Seeds each cell from its OCV (via HAL). */
void     ekf_init(ekf_t *e, const ekf_config_t *cfg);
/* Overrides every cell's estimate, with variance p0 (e.g. after a reset). */
void     ekf_set(ekf_t *e, uint16_t soc_cpct);
void     ekf_predict(ekf_t *e, uint8_t cell, int32_t current_ma, uint32_t dt_ms);
void     ekf_correct(ekf_t *e, uint8_t cell, uint16_t mv, int32_t current_ma);
/* Predict + correct every cell from one sample. */
void     ekf_update(ekf_t *e, const uint16_t mv[HAL_NUM_CELLS], int32_t current_ma,
                    uint32_t dt_ms);
/* Reads the HAL and calls ekf_update(). */
void     ekf_step(ekf_t *e, uint32_t dt_ms);

uint16_t ekf_cell_cpct(const ekf_t *e, uint8_t cell);   /* 0.01 % */
uint16_t ekf_sigma_cpct(const ekf_t *e, uint8_t cell);  /* 1 sigma, 0.01 % */
uint16_t ekf_pack_cpct(const ekf_t *e);                 /* lowest cell */
uint8_t  ekf_min_cell(const ekf_t *e);

#endif
