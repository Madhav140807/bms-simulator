#ifndef PROTECTION_H
#define PROTECTION_H

#include <stdbool.h>
#include <stdint.h>
#include "hal.h"

/* Pack protection: over/under voltage, over current (discharge and charge)
 * and over temperature. A condition must hold for `debounce` consecutive
 * steps before it trips. Tripped faults latch, open the main contactor and
 * keep it open until cleared with protection_clear(). */

#define PROT_FAULT_NONE     0x00u
#define PROT_FAULT_OV       0x01u  /* any cell above ov_mv */
#define PROT_FAULT_UV       0x02u  /* any cell below uv_mv */
#define PROT_FAULT_OC_DSG   0x04u  /* discharge current above oc_dsg_ma */
#define PROT_FAULT_OC_CHG   0x08u  /* charge current above oc_chg_ma */
#define PROT_FAULT_OT       0x10u  /* any cell above ot_dc */
#define PROT_NUM_FAULTS     5u

typedef enum {
    PROT_STATE_OK = 0,
    PROT_STATE_FAULT
} prot_state_t;

typedef struct {
    uint16_t ov_mv;
    uint16_t uv_mv;
    int32_t  oc_dsg_ma;  /* positive magnitude */
    int32_t  oc_chg_ma;  /* positive magnitude */
    int16_t  ot_dc;
    uint8_t  debounce;   /* consecutive steps to trip, 0 treated as 1 */
} prot_limits_t;

typedef struct {
    uint16_t cell_mv[HAL_NUM_CELLS];
    int16_t  cell_temp_dc[HAL_NUM_CELLS];
    int32_t  current_ma;
} prot_sample_t;

typedef struct {
    prot_limits_t limits;
    prot_state_t  state;
    uint8_t       faults;  /* latched PROT_FAULT_* bits */
    uint8_t       counts[PROT_NUM_FAULTS];
} prot_t;

extern const prot_limits_t PROT_DEFAULT_LIMITS;

/* limits may be NULL to use PROT_DEFAULT_LIMITS. Closes the contactor. */
void    protection_init(prot_t *p, const prot_limits_t *limits);
uint8_t protection_check(const prot_limits_t *limits, const prot_sample_t *s);
void    protection_read_sample(prot_sample_t *s);
void    protection_update(prot_t *p, const prot_sample_t *s);
void    protection_step(prot_t *p);
/* Clears latched faults and recloses the contactor, but only if no
 * condition is currently present. Returns true on success. */
bool    protection_clear(prot_t *p);

#endif
