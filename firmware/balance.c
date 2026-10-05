/* Passive balancing: filtered cell voltages, hysteresis and inhibit rules. */
#include <stddef.h>
#include "balance.h"

const bal_config_t BAL_DEFAULT_CONFIG = {
    .start_mv    = 15,
    .stop_mv     = 5,
    .min_mv      = 3400,
    .max_temp_dc = 500,
    .max_dsg_ma  = 500,
    .bleed_sag_mv = 2,    /* ~0.12 A bleed through ~20 mOhm */
};

static void apply(uint8_t mask)
{
    for (uint8_t i = 0; i < HAL_NUM_CELLS; i++) {
        hal_set_balance(i, (mask >> i) & 1u);
    }
}

void balance_init(bal_t *b, const bal_config_t *cfg)
{
    b->cfg = (cfg != NULL) ? *cfg : BAL_DEFAULT_CONFIG;
    b->mask = 0;
    b->primed = false;
    apply(0);
}

static uint16_t min_cell_mv(const bal_sample_t *s)
{
    uint16_t min = UINT16_MAX;
    for (uint8_t i = 0; i < HAL_NUM_CELLS; i++) {
        if (s->cell_mv[i] < min) {
            min = s->cell_mv[i];
        }
    }
    return min;
}

bool balance_inhibited(const bal_config_t *cfg, const bal_sample_t *s)
{
    if (s->current_ma > cfg->max_dsg_ma || min_cell_mv(s) < cfg->min_mv) {
        return true;
    }
    for (uint8_t i = 0; i < HAL_NUM_CELLS; i++) {
        if (s->cell_temp_dc[i] > cfg->max_temp_dc) {
            return true;
        }
    }
    return false;
}

uint8_t balance_select(const bal_config_t *cfg, const bal_sample_t *s, uint8_t mask)
{
    if (balance_inhibited(cfg, s)) {
        return 0;
    }
    uint16_t min = min_cell_mv(s);
    uint8_t next = 0;
    for (uint8_t i = 0; i < HAL_NUM_CELLS; i++) {
        uint16_t delta = (uint16_t)(s->cell_mv[i] - min);
        uint16_t limit = ((mask >> i) & 1u) ? cfg->stop_mv : cfg->start_mv;
        if (delta > limit) {
            next |= (uint8_t)(1u << i);
        }
    }
    return next;
}

void balance_read_sample(bal_sample_t *s)
{
    for (uint8_t i = 0; i < HAL_NUM_CELLS; i++) {
        s->cell_mv[i] = hal_read_cell_mv(i);
        s->cell_temp_dc[i] = hal_read_cell_temp_dc(i);
    }
    s->current_ma = hal_read_pack_current_ma();
}

void balance_filter(bal_t *b, bal_sample_t *s)
{
    for (uint8_t i = 0; i < HAL_NUM_CELLS; i++) {
        uint32_t raw = (uint32_t)s->cell_mv[i] << 4;
        if (!b->primed) {
            b->filt_mv_x16[i] = raw;
        } else {
            int32_t diff = (int32_t)raw - (int32_t)b->filt_mv_x16[i];
            b->filt_mv_x16[i] = (uint32_t)((int32_t)b->filt_mv_x16[i] + diff / (1 << BAL_FILTER_SHIFT));
        }
        s->cell_mv[i] = (uint16_t)((b->filt_mv_x16[i] + 8u) >> 4);
    }
    b->primed = true;
}

void balance_update(bal_t *b, const bal_sample_t *s, bool allowed)
{
    bal_sample_t f = *s;
    balance_filter(b, &f);
    for (uint8_t i = 0; i < HAL_NUM_CELLS; i++) {
        if ((b->mask >> i) & 1u) {
            f.cell_mv[i] = (uint16_t)(f.cell_mv[i] + b->cfg.bleed_sag_mv);
        }
    }
    b->mask = allowed ? balance_select(&b->cfg, &f, b->mask) : 0;
    apply(b->mask);
}

void balance_step(bal_t *b, bool allowed)
{
    bal_sample_t s;
    balance_read_sample(&s);
    balance_update(b, &s, allowed);
}
