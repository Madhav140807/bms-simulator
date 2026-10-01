#include "csv.h"

void csv_header(FILE *out)
{
    fprintf(out, "time_s,current_a,pack_v");
    for (int i = 0; i < SIM_PACK_CELLS; i++) {
        fprintf(out, ",cell%d_v,cell%d_soc,cell%d_temp_c", i, i, i);
    }
    fprintf(out, ",contactor,faults,soc_est_pct,balance_mask,ekf_soc_pct\n");
}

void csv_row(FILE *out, const bms_sys_t *sys)
{
    const sim_pack_t *pack = &sys->pack;
    fprintf(out, "%u,%.3f,%.4f", (unsigned)sys->time_s, sim_pack_current(pack),
            sim_pack_voltage(pack));
    for (int i = 0; i < SIM_PACK_CELLS; i++) {
        const sim_cell_t *c = &pack->cells[i];
        fprintf(out, ",%.4f,%.4f,%.2f", sim_cell_voltage(c), c->soc, c->temp_c);
    }
    fprintf(out, ",%d,0x%02x,%.2f,0x%02x,%.2f\n", pack->contactor_closed,
            sys->prot.faults, soc_get(&sys->soc) / 100.0, sys->bal.mask,
            ekf_pack_cpct(&sys->ekf) / 100.0);
}

int csv_run(FILE *out, const scenario_t *scn, uint32_t every_s)
{
    bms_sys_t sys;
    scn_run_t run;
    int rows = 0;
    if (every_s == 0) {
        every_s = 1;
    }
    scenario_start(&run, scn, &sys);
    csv_header(out);
    for (;;) {
        bool done = scenario_done(&run, &sys);
        if (done || sys.time_s % every_s == 0) {
            csv_row(out, &sys);
            rows++;
        }
        if (done) {
            return rows;
        }
        sys_step(&sys);
        scenario_apply(&run, &sys);
    }
}
