#include <stddef.h>
#include "soc.h"

#define OCV_POINTS   11u
#define MS_PER_HOUR  3600000LL

const soc_config_t SOC_DEFAULT_CONFIG = {
    .capacity_mah = 3000,
    .rest_ma      = 50,
    .rest_ms      = 300000,
};

/* Cell OCV (mV) at 0 %, 10 %, ... 100 % SOC. Must be strictly increasing. */
static const uint16_t ocv_mv[OCV_POINTS] = {
    3000, 3450, 3550, 3620, 3680, 3740, 3820, 3900, 3980, 4070, 4200
};

uint16_t soc_from_ocv_mv(uint16_t mv)
{
    if (mv <= ocv_mv[0]) {
        return 0;
    }
    if (mv >= ocv_mv[OCV_POINTS - 1]) {
        return SOC_FULL_CPCT;
    }
    uint8_t i = 0;
    while (mv >= ocv_mv[i + 1]) {
        i++;
    }
    uint32_t step = SOC_FULL_CPCT / (OCV_POINTS - 1);
    uint32_t span = (uint32_t)(ocv_mv[i + 1] - ocv_mv[i]);
    uint32_t frac = (uint32_t)(mv - ocv_mv[i]) * step / span;
    return (uint16_t)(i * step + frac);
}

static int64_t capacity_mams(const soc_t *s)
{
    return (int64_t)s->cfg.capacity_mah * MS_PER_HOUR;
}

void soc_set(soc_t *s, uint16_t soc_cpct)
{
    if (soc_cpct > SOC_FULL_CPCT) {
        soc_cpct = SOC_FULL_CPCT;
    }
    s->charge_mams = capacity_mams(s) * soc_cpct / SOC_FULL_CPCT;
}

uint16_t soc_get(const soc_t *s)
{
    int64_t cap = capacity_mams(s);
    if (cap <= 0) {
        return 0;
    }
    return (uint16_t)(s->charge_mams * SOC_FULL_CPCT / cap);
}

uint16_t soc_min_cell_mv(void)
{
    uint16_t min = UINT16_MAX;
    for (uint8_t i = 0; i < HAL_NUM_CELLS; i++) {
        uint16_t mv = hal_read_cell_mv(i);
        if (mv < min) {
            min = mv;
        }
    }
    return min;
}

void soc_init(soc_t *s, const soc_config_t *cfg)
{
    s->cfg = (cfg != NULL) ? *cfg : SOC_DEFAULT_CONFIG;
    s->rest_elapsed_ms = 0;
    soc_set(s, soc_from_ocv_mv(soc_min_cell_mv()));
}

static void coulomb_count(soc_t *s, int32_t current_ma, uint32_t dt_ms)
{
    int64_t cap = capacity_mams(s);
    s->charge_mams -= (int64_t)current_ma * dt_ms;
    if (s->charge_mams < 0) {
        s->charge_mams = 0;
    } else if (s->charge_mams > cap) {
        s->charge_mams = cap;
    }
}

static void track_rest(soc_t *s, int32_t current_ma, uint32_t dt_ms)
{
    int32_t mag = (current_ma < 0) ? -current_ma : current_ma;
    if (mag > (int32_t)s->cfg.rest_ma) {
        s->rest_elapsed_ms = 0;
    } else if (s->rest_elapsed_ms < s->cfg.rest_ms) {
        s->rest_elapsed_ms += dt_ms;
    }
}

void soc_update(soc_t *s, int32_t current_ma, uint16_t min_cell_mv, uint32_t dt_ms)
{
    coulomb_count(s, current_ma, dt_ms);
    track_rest(s, current_ma, dt_ms);
    if (s->rest_elapsed_ms >= s->cfg.rest_ms) {
        soc_set(s, soc_from_ocv_mv(min_cell_mv));
    }
}

void soc_step(soc_t *s, uint32_t dt_ms)
{
    /* Separate statements: argument evaluation order is unspecified, and
     * with noisy sensors the read order changes the values. */
    int32_t ma = hal_read_pack_current_ma();
    uint16_t min_mv = soc_min_cell_mv();
    soc_update(s, ma, min_mv, dt_ms);
}
