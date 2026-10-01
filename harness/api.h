#ifndef API_H
#define API_H

/* Flat C API over the simulated system (see system.h), exported to
 * JavaScript by the WebAssembly build. Each step runs the firmware
 * (protection, SOC, balancing) then advances the sim by one second.
 * Balancing only runs while enabled and no fault is latched. */

void   api_reset(double soc);            /* free run, no scenario */
void   api_set_load(double current_a);   /* positive = discharge */
void   api_set_ambient(double temp_c);
void   api_set_cell_soc(int cell, double soc);
void   api_step(int steps);              /* stops at a scenario's end */
int    api_clear_faults(void);           /* 1 if cleared */
void   api_set_balancing(int enabled);   /* on after reset */
void   api_set_noise(int enabled);       /* sensor noise, on after reset */

/* Scenarios (same table as the CSV runner). */
int         api_scenario_count(void);
const char *api_scenario_name(int index);
const char *api_scenario_desc(int index);
int         api_start_scenario(int index);   /* 1 on success */
int         api_scenario_active(void);
int         api_scenario_done(void);
double      api_scenario_duration_s(void);

double api_time_s(void);
double api_load_a(void);
double api_ambient_c(void);
double api_current_a(void);
double api_pack_v(void);
double api_cell_v(int cell);
double api_cell_soc(int cell);
double api_cell_temp_c(int cell);
double api_cell_capacity_ah(int cell);   /* differs per cell (mismatch) */
double api_cell_r_mohm(int cell);
int    api_cell_meas_mv(int cell);       /* last HAL reading used by protection */
int    api_noise(void);
int    api_contactor(void);
int    api_faults(void);
double api_soc_est_pct(void);          /* coulomb counting estimate */
double api_ekf_pct(void);              /* Kalman estimate, lowest cell */
double api_ekf_sigma_pct(void);        /* its 1 sigma */
double api_ekf_cell_pct(int cell);
void   api_corrupt_estimates(double pct);   /* set both estimators to pct */

/* Fault injection, flags: 1 = external heater, 2 = internal short,
 * 4 = open voltage sense wire. */
void   api_inject(int cell, int flags, int on);
int    api_injected(int cell);
void   api_clear_injections(void);

/* CAN bus log. Frames are numbered from 0 since the last reset; only the
 * newest HAL_SIM_CAN_RING are kept (api_can_valid tells). */
int    api_can_total(void);
int    api_can_valid(int seq);
int    api_can_id(int seq);
int    api_can_dlc(int seq);
int    api_can_byte(int seq, int index);
double api_can_time_s(int seq);
int    api_balance_mask(void);          /* bit i = cell i bleeding */
int    api_balancing(void);
int    api_num_cells(void);

#endif
