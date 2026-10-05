/* Encodes the BMS CAN frames (pack, cells, temps, status, fault event). */
#include <string.h>
#include "can_tx.h"

static void put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)(v >> 8);
}

static void start(hal_can_frame_t *f, uint16_t id)
{
    memset(f, 0, sizeof *f);
    f->id = id;
    f->dlc = HAL_CAN_MAX_DLC;
}

static int16_t clamp_i16(int32_t v)
{
    return (int16_t)(v > INT16_MAX ? INT16_MAX : (v < INT16_MIN ? INT16_MIN : v));
}

void can_tx_init(can_tx_t *tx)
{
    tx->alive = 0;
    tx->reported_faults = 0;
}

static uint32_t pack_mv(const can_status_t *st)
{
    uint32_t mv = 0;
    for (uint8_t i = 0; i < HAL_NUM_CELLS; i++) {
        mv += st->cell_mv[i];
    }
    return mv;
}

void can_collect(can_status_t *st, const prot_t *prot, const soc_t *soc,
                 const ekf_t *ekf, const bal_t *bal)
{
    for (uint8_t i = 0; i < HAL_NUM_CELLS; i++) {
        st->cell_mv[i] = prot->last.cell_mv[i];
        st->cell_temp_dc[i] = prot->last.cell_temp_dc[i];
    }
    st->current_ma = prot->last.current_ma;
    st->soc_cpct = soc_get(soc);
    st->ekf_cpct = ekf_pack_cpct(ekf);
    st->faults = prot->faults;
    st->balance_mask = bal->mask;
    st->contactor = hal_get_contactor();
}

void can_encode_pack(hal_can_frame_t *f, const can_status_t *st)
{
    start(f, CAN_ID_PACK);
    put_u16(&f->data[0], (uint16_t)((pack_mv(st) + 5u) / 10u));
    put_u16(&f->data[2], (uint16_t)clamp_i16(st->current_ma / 10));
    put_u16(&f->data[4], st->ekf_cpct);
    f->data[6] = st->contactor ? 1u : 0u;
    f->data[7] = st->faults;
}

void can_encode_cell_v(hal_can_frame_t *f, const can_status_t *st)
{
    start(f, CAN_ID_CELL_V);
    for (uint8_t i = 0; i < HAL_NUM_CELLS; i++) {
        put_u16(&f->data[2u * i], st->cell_mv[i]);
    }
}

void can_encode_cell_t(hal_can_frame_t *f, const can_status_t *st)
{
    start(f, CAN_ID_CELL_T);
    for (uint8_t i = 0; i < HAL_NUM_CELLS; i++) {
        put_u16(&f->data[2u * i], (uint16_t)st->cell_temp_dc[i]);
    }
}

void can_encode_status(hal_can_frame_t *f, const can_status_t *st, uint8_t alive)
{
    start(f, CAN_ID_STATUS);
    put_u16(&f->data[0], st->soc_cpct);
    f->data[2] = st->balance_mask;
    f->data[3] = st->faults;
    f->data[4] = alive;
}

void can_encode_fault_event(hal_can_frame_t *f, uint8_t new_bits, uint8_t all)
{
    start(f, CAN_ID_FAULT_EVENT);
    f->dlc = 2;
    f->data[0] = new_bits;
    f->data[1] = all;
}

uint8_t can_tx_step(can_tx_t *tx, const can_status_t *st)
{
    hal_can_frame_t f;
    uint8_t sent = 0;
    uint8_t fresh = (uint8_t)(st->faults & ~tx->reported_faults);
    if (fresh != 0) {
        can_encode_fault_event(&f, fresh, st->faults);
        sent += hal_can_send(&f);
    }
    tx->reported_faults = st->faults;   /* cleared faults can be re-announced */
    can_encode_pack(&f, st);
    sent += hal_can_send(&f);
    can_encode_cell_v(&f, st);
    sent += hal_can_send(&f);
    can_encode_cell_t(&f, st);
    sent += hal_can_send(&f);
    can_encode_status(&f, st, tx->alive++);
    sent += hal_can_send(&f);
    return sent;
}
