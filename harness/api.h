#ifndef API_H
#define API_H

/* Flat C API over sim + HAL + firmware, exported to JavaScript by the
 * WebAssembly build. Owns one pack plus the firmware state. Each step
 * runs the firmware (protection, SOC, balancing) then advances the sim by
 * one second. Balancing only runs while enabled and no fault is latched. */

#define API_DT_S 1.0

void   api_reset(double soc);
void   api_set_load(double current_a);   /* positive = discharge */
void   api_set_ambient(double temp_c);
void   api_set_cell_soc(int cell, double soc);
void   api_step(int steps);
int    api_clear_faults(void);           /* 1 if cleared */
void   api_set_balancing(int enabled);   /* on after reset */

double api_time_s(void);
double api_load_a(void);
double api_current_a(void);
double api_pack_v(void);
double api_cell_v(int cell);
double api_cell_soc(int cell);
double api_cell_temp_c(int cell);
int    api_contactor(void);
int    api_faults(void);
double api_soc_est_pct(void);          /* firmware SOC estimate */
int    api_balance_mask(void);          /* bit i = cell i bleeding */
int    api_num_cells(void);

#endif
