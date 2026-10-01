#ifndef API_H
#define API_H

/* Flat C API over sim + HAL + protection, exported to JavaScript by the
 * WebAssembly build. Owns one pack and one protection instance. Each step
 * runs protection then advances the sim by one second. */

#define API_DT_S 1.0

void   api_reset(double soc);
void   api_set_load(double current_a);   /* positive = discharge */
void   api_set_ambient(double temp_c);
void   api_set_cell_soc(int cell, double soc);
void   api_step(int steps);
int    api_clear_faults(void);           /* 1 if cleared */

double api_time_s(void);
double api_load_a(void);
double api_current_a(void);
double api_pack_v(void);
double api_cell_v(int cell);
double api_cell_soc(int cell);
double api_cell_temp_c(int cell);
int    api_contactor(void);
int    api_faults(void);
int    api_num_cells(void);

#endif
