#ifndef OCV_H
#define OCV_H

#include <stdint.h>

/* Open circuit voltage curve of the NMC cell, shared by the SOC
 * estimators. Points at 0 %, 10 %, ... 100 % SOC, strictly increasing. */

#define OCV_POINTS 11u

/* Cell voltage readings outside this range cannot be real (open or shorted
 * sense wire, broken ADC); estimators skip them. */
#define CELL_MV_PLAUSIBLE_MIN 1000u
#define CELL_MV_PLAUSIBLE_MAX 5000u

static inline int cell_mv_plausible(uint16_t mv)
{
    return mv >= CELL_MV_PLAUSIBLE_MIN && mv <= CELL_MV_PLAUSIBLE_MAX;
}

extern const uint16_t OCV_TABLE_MV[OCV_POINTS];

/* SOC (0.01 %) from OCV (mV), clamped to the table ends. */
uint16_t ocv_soc_cpct(uint16_t mv);
/* OCV (mV) at SOC (fraction 0..1, clamped), linear between points. */
float    ocv_mv_at(float soc);
/* d(OCV)/d(SOC) in mV per unit SOC on the segment containing soc. */
float    ocv_slope_mv(float soc);

#endif
