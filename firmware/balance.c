#include <stddef.h>
#include "balance.h"

const bal_config_t BAL_DEFAULT_CONFIG = {
    .start_mv    = 15,
    .stop_mv     = 5,
    .min_mv      = 3400,
    .max_temp_dc = 500,
    .max_dsg_ma  = 500,
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

void balance_update(bal_t *b, const bal_sample_t *s, bool allowed)
{
    b->mask = allowed ? balance_select(&b->cfg, s, b->mask) : 0;
    apply(b->mask);
}

void balance_step(bal_t *b, bool allowed)
{
    bal_sample_t s;
    balance_read_sample(&s);
    balance_update(b, &s, allowed);
}
