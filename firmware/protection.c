#include <stddef.h>
#include "protection.h"

const prot_limits_t PROT_DEFAULT_LIMITS = {
    .ov_mv     = 4250,
    .uv_mv     = 3000,
    .oc_dsg_ma = 10000,
    .oc_chg_ma = 3000,
    .ot_dc     = 600,
    .debounce  = 3,
};

void protection_init(prot_t *p, const prot_limits_t *limits)
{
    p->limits = (limits != NULL) ? *limits : PROT_DEFAULT_LIMITS;
    p->state = PROT_STATE_OK;
    p->faults = PROT_FAULT_NONE;
    for (uint8_t i = 0; i < PROT_NUM_FAULTS; i++) {
        p->counts[i] = 0;
    }
    protection_read_sample(&p->last);
    hal_set_contactor(true);
}

static uint8_t check_cells(const prot_limits_t *lim, const prot_sample_t *s)
{
    uint8_t f = PROT_FAULT_NONE;
    for (uint8_t i = 0; i < HAL_NUM_CELLS; i++) {
        if (s->cell_mv[i] > lim->ov_mv) {
            f |= PROT_FAULT_OV;
        }
        if (s->cell_mv[i] < lim->uv_mv) {
            f |= PROT_FAULT_UV;
        }
        if (s->cell_temp_dc[i] > lim->ot_dc) {
            f |= PROT_FAULT_OT;
        }
    }
    return f;
}

static uint8_t check_current(const prot_limits_t *lim, int32_t ma)
{
    if (ma > lim->oc_dsg_ma) {
        return PROT_FAULT_OC_DSG;
    }
    if (ma < -lim->oc_chg_ma) {
        return PROT_FAULT_OC_CHG;
    }
    return PROT_FAULT_NONE;
}

uint8_t protection_check(const prot_limits_t *limits, const prot_sample_t *s)
{
    return check_cells(limits, s) | check_current(limits, s->current_ma);
}

void protection_read_sample(prot_sample_t *s)
{
    for (uint8_t i = 0; i < HAL_NUM_CELLS; i++) {
        s->cell_mv[i] = hal_read_cell_mv(i);
        s->cell_temp_dc[i] = hal_read_cell_temp_dc(i);
    }
    s->current_ma = hal_read_pack_current_ma();
}

/* Returns the fault bits whose debounce counters reached the limit. */
static uint8_t debounce(prot_t *p, uint8_t active)
{
    uint8_t need = (p->limits.debounce == 0) ? 1 : p->limits.debounce;
    uint8_t tripped = PROT_FAULT_NONE;
    for (uint8_t i = 0; i < PROT_NUM_FAULTS; i++) {
        uint8_t bit = (uint8_t)(1u << i);
        if ((active & bit) == 0) {
            p->counts[i] = 0;
            continue;
        }
        if (p->counts[i] < need) {
            p->counts[i]++;
        }
        if (p->counts[i] >= need) {
            tripped |= bit;
        }
    }
    return tripped;
}

void protection_update(prot_t *p, const prot_sample_t *s)
{
    p->last = *s;
    p->faults |= debounce(p, protection_check(&p->limits, s));
    if (p->faults != PROT_FAULT_NONE) {
        p->state = PROT_STATE_FAULT;
        hal_set_contactor(false);
    }
}

void protection_step(prot_t *p)
{
    prot_sample_t s;
    protection_read_sample(&s);
    protection_update(p, &s);
}

bool protection_clear(prot_t *p)
{
    prot_sample_t s;
    protection_read_sample(&s);
    if (protection_check(&p->limits, &s) != PROT_FAULT_NONE) {
        return false;
    }
    protection_init(p, &p->limits);
    return true;
}
