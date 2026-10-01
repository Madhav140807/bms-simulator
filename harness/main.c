#include <stdio.h>
#include "hal_sim.h"
#include "pack.h"
#include "protection.h"
#include "soc.h"

#define CAPACITY_AH   3.0
#define START_SOC     1.0
#define CELL_R_OHM    0.02
#define LOAD_A        3.0
#define DT_S          1.0
#define DURATION_S    4200  /* 1C discharge, then rest after the UV trip */
#define LOG_EVERY_S   60

static void print_header(void)
{
    printf("time_s,current_a,pack_v");
    for (int i = 0; i < SIM_PACK_CELLS; i++) {
        printf(",cell%d_v,cell%d_soc,cell%d_temp_c", i, i, i);
    }
    printf(",contactor,faults,soc_est_pct\n");
}

static void print_row(int t, const sim_pack_t *pack, const prot_t *prot,
                      const soc_t *soc)
{
    printf("%d,%.3f,%.4f", t, sim_pack_current(pack), sim_pack_voltage(pack));
    for (int i = 0; i < SIM_PACK_CELLS; i++) {
        const sim_cell_t *c = &pack->cells[i];
        printf(",%.4f,%.4f,%.2f", sim_cell_voltage(c), c->soc, c->temp_c);
    }
    printf(",%d,0x%02x,%.2f\n", pack->contactor_closed, prot->faults,
           soc_get(soc) / 100.0);
}

int main(void)
{
    sim_pack_t pack;
    sim_pack_init(&pack, CAPACITY_AH, START_SOC, CELL_R_OHM);
    sim_pack_set_current(&pack, LOAD_A);
    hal_sim_attach(&pack);
    prot_t prot;
    protection_init(&prot, NULL);
    soc_t soc;
    soc_init(&soc, NULL);
    print_header();
    for (int t = 0; t <= DURATION_S; t++) {
        protection_step(&prot);
        soc_step(&soc, (uint32_t)(DT_S * 1000));
        if (t % LOG_EVERY_S == 0) {
            print_row(t, &pack, &prot, &soc);
        }
        sim_pack_step(&pack, DT_S);
    }
    return 0;
}
