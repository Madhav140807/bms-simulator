#include "ocv.h"

#define CPCT_FULL 10000u

const uint16_t OCV_TABLE_MV[OCV_POINTS] = {
    3000, 3450, 3550, 3620, 3680, 3740, 3820, 3900, 3980, 4070, 4200
};

uint16_t ocv_soc_cpct(uint16_t mv)
{
    if (mv <= OCV_TABLE_MV[0]) {
        return 0;
    }
    if (mv >= OCV_TABLE_MV[OCV_POINTS - 1]) {
        return CPCT_FULL;
    }
    uint8_t i = 0;
    while (mv >= OCV_TABLE_MV[i + 1]) {
        i++;
    }
    uint32_t step = CPCT_FULL / (OCV_POINTS - 1);
    uint32_t span = (uint32_t)(OCV_TABLE_MV[i + 1] - OCV_TABLE_MV[i]);
    uint32_t frac = (uint32_t)(mv - OCV_TABLE_MV[i]) * step / span;
    return (uint16_t)(i * step + frac);
}

/* Segment index for soc, 0 .. OCV_POINTS - 2. */
static uint8_t segment(float soc)
{
    if (soc <= 0.0f) {
        return 0;
    }
    float pos = soc * (float)(OCV_POINTS - 1);
    uint8_t i = (uint8_t)pos;
    return (i >= OCV_POINTS - 1) ? (uint8_t)(OCV_POINTS - 2) : i;
}

float ocv_slope_mv(float soc)
{
    uint8_t i = segment(soc);
    return (float)(OCV_TABLE_MV[i + 1] - OCV_TABLE_MV[i]) * (float)(OCV_POINTS - 1);
}

float ocv_mv_at(float soc)
{
    soc = soc < 0.0f ? 0.0f : (soc > 1.0f ? 1.0f : soc);
    uint8_t i = segment(soc);
    float frac = soc * (float)(OCV_POINTS - 1) - (float)i;
    return (float)OCV_TABLE_MV[i] + frac * (float)(OCV_TABLE_MV[i + 1] - OCV_TABLE_MV[i]);
}
