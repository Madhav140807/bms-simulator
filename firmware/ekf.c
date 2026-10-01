#include <stddef.h>
#include "ekf.h"
#include "ocv.h"

#define MS_PER_HOUR 3600000.0f
#define CPCT_FULL   10000.0f

const ekf_config_t EKF_DEFAULT_CONFIG = {
    .capacity_mah = 3000,
    .r0_mohm      = 20.0f,
    .q_per_s      = 1e-8f,    /* ~0.01 % SOC drift per second */
    .r_mv2        = 25.0f,    /* 5 mV: sensor noise plus model error */
    .p0           = 0.0025f,  /* 5 % SOC */
};

static float clamp01(float x)
{
    return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x);
}

void ekf_init(ekf_t *e, const ekf_config_t *cfg)
{
    e->cfg = (cfg != NULL) ? *cfg : EKF_DEFAULT_CONFIG;
    for (uint8_t i = 0; i < HAL_NUM_CELLS; i++) {
        e->soc[i] = (float)ocv_soc_cpct(hal_read_cell_mv(i)) / CPCT_FULL;
        e->var[i] = e->cfg.p0;
    }
}

void ekf_set(ekf_t *e, uint16_t soc_cpct)
{
    for (uint8_t i = 0; i < HAL_NUM_CELLS; i++) {
        e->soc[i] = clamp01((float)soc_cpct / CPCT_FULL);
        e->var[i] = e->cfg.p0;
    }
}

void ekf_predict(ekf_t *e, uint8_t cell, int32_t current_ma, uint32_t dt_ms)
{
    float used = (float)current_ma * (float)dt_ms / MS_PER_HOUR;   /* mAh */
    e->soc[cell] = clamp01(e->soc[cell] - used / (float)e->cfg.capacity_mah);
    e->var[cell] += e->cfg.q_per_s * (float)dt_ms / 1000.0f;
}

void ekf_correct(ekf_t *e, uint8_t cell, uint16_t mv, int32_t current_ma)
{
    if (!cell_mv_plausible(mv)) {
        return;   /* failed sensor: keep predicting only */
    }
    float x = e->soc[cell];
    float h = ocv_mv_at(x) - (float)current_ma * e->cfg.r0_mohm / 1000.0f;
    float H = ocv_slope_mv(x);
    float s = H * H * e->var[cell] + e->cfg.r_mv2;
    float k = e->var[cell] * H / s;
    e->soc[cell] = clamp01(x + k * ((float)mv - h));
    e->var[cell] = (1.0f - k * H) * e->var[cell];
}

void ekf_update(ekf_t *e, const uint16_t mv[HAL_NUM_CELLS], int32_t current_ma,
                uint32_t dt_ms)
{
    for (uint8_t i = 0; i < HAL_NUM_CELLS; i++) {
        ekf_predict(e, i, current_ma, dt_ms);
        ekf_correct(e, i, mv[i], current_ma);
    }
}

void ekf_step(ekf_t *e, uint32_t dt_ms)
{
    uint16_t mv[HAL_NUM_CELLS];
    int32_t ma = hal_read_pack_current_ma();
    for (uint8_t i = 0; i < HAL_NUM_CELLS; i++) {
        mv[i] = hal_read_cell_mv(i);
    }
    ekf_update(e, mv, ma, dt_ms);
}

static uint16_t to_cpct(float x)
{
    return (uint16_t)(clamp01(x) * CPCT_FULL + 0.5f);
}

uint16_t ekf_cell_cpct(const ekf_t *e, uint8_t cell)
{
    return cell < HAL_NUM_CELLS ? to_cpct(e->soc[cell]) : 0;
}

/* Square root by Newton iteration, so the firmware needs no libm. */
static float sqrt_f(float v)
{
    if (v <= 0.0f) {
        return 0.0f;
    }
    float r = v > 1.0f ? v : 1.0f;
    for (uint8_t i = 0; i < 30; i++) {
        r = 0.5f * (r + v / r);
    }
    return r;
}

uint16_t ekf_sigma_cpct(const ekf_t *e, uint8_t cell)
{
    return cell < HAL_NUM_CELLS ? to_cpct(sqrt_f(e->var[cell])) : 0;
}

uint8_t ekf_min_cell(const ekf_t *e)
{
    uint8_t min = 0;
    for (uint8_t i = 1; i < HAL_NUM_CELLS; i++) {
        if (e->soc[i] < e->soc[min]) {
            min = i;
        }
    }
    return min;
}

uint16_t ekf_pack_cpct(const ekf_t *e)
{
    return ekf_cell_cpct(e, ekf_min_cell(e));
}
