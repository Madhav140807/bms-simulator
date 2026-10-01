#ifndef CAN_TX_H
#define CAN_TX_H

#include <stdint.h>
#include "balance.h"
#include "ekf.h"
#include "hal.h"
#include "protection.h"
#include "soc.h"

/* BMS CAN messages. All multi-byte fields are little endian.
 *
 *   0x080 FAULT_EVENT  sent once when new fault bits latch
 *         [0] newly latched PROT_FAULT_* bits  [1] all latched bits
 *   0x100 PACK         every step
 *         [0-1] pack voltage, 10 mV (u16)   [2-3] current, 10 mA (i16, + = discharge)
 *         [4-5] SOC (Kalman), 0.01 % (u16)  [6] bit0 contactor closed  [7] faults
 *   0x101 CELL_V       every step: [0-7] cell 1..4 voltage, mV (u16)
 *   0x102 CELL_T       every step: [0-7] cell 1..4 temperature, 0.1 C (i16)
 *   0x103 STATUS       every step
 *         [0-1] SOC (coulomb count), 0.01 % (u16)  [2] balance mask
 *         [3] faults  [4] alive counter (wraps)  [5-7] 0 */

#define CAN_ID_FAULT_EVENT 0x080u
#define CAN_ID_PACK        0x100u
#define CAN_ID_CELL_V      0x101u
#define CAN_ID_CELL_T      0x102u
#define CAN_ID_STATUS      0x103u

/* Everything the messages carry, gathered from the firmware modules. */
typedef struct {
    uint16_t cell_mv[HAL_NUM_CELLS];
    int16_t  cell_temp_dc[HAL_NUM_CELLS];
    int32_t  current_ma;
    uint16_t soc_cpct;
    uint16_t ekf_cpct;
    uint8_t  faults;
    uint8_t  balance_mask;
    bool     contactor;
} can_status_t;

typedef struct {
    uint8_t alive;
    uint8_t reported_faults;  /* bits already announced by FAULT_EVENT */
} can_tx_t;

void can_tx_init(can_tx_t *tx);
void can_collect(can_status_t *st, const prot_t *prot, const soc_t *soc,
                 const ekf_t *ekf, const bal_t *bal);

/* Pure encoders. */
void can_encode_pack(hal_can_frame_t *f, const can_status_t *st);
void can_encode_cell_v(hal_can_frame_t *f, const can_status_t *st);
void can_encode_cell_t(hal_can_frame_t *f, const can_status_t *st);
void can_encode_status(hal_can_frame_t *f, const can_status_t *st, uint8_t alive);
void can_encode_fault_event(hal_can_frame_t *f, uint8_t new_bits, uint8_t all);

/* Sends FAULT_EVENT (if new faults latched) then the four periodic frames.
 * Returns the number of frames the HAL accepted. */
uint8_t can_tx_step(can_tx_t *tx, const can_status_t *st);

#endif
