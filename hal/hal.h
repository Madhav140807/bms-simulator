#ifndef HAL_H
#define HAL_H

#include <stdbool.h>
#include <stdint.h>

/* Hardware abstraction layer: the only interface firmware uses to reach
 * the battery hardware. Units are fixed point integers:
 *   voltage     millivolts (mV)
 *   current     milliamps (mA), positive = discharge
 *   temperature tenths of a degree Celsius (0.1 C) */

#define HAL_NUM_CELLS 4

uint16_t hal_read_cell_mv(uint8_t cell);
int32_t  hal_read_pack_current_ma(void);
int16_t  hal_read_cell_temp_dc(uint8_t cell);

void hal_set_contactor(bool closed);
bool hal_get_contactor(void);

void hal_set_balance(uint8_t cell, bool on);
bool hal_get_balance(uint8_t cell);

/* CAN bus: classic frames, 11-bit identifiers. */
#define HAL_CAN_MAX_DLC 8u

typedef struct {
    uint16_t id;
    uint8_t  dlc;
    uint8_t  data[HAL_CAN_MAX_DLC];
} hal_can_frame_t;

/* Queues a frame for transmission; false if it was rejected. */
bool hal_can_send(const hal_can_frame_t *frame);

#endif
