#include <stdio.h>
#include "pack.h"

#define CAPACITY_AH   3.0
#define START_SOC     1.0
#define CELL_R_OHM    0.02
#define LOAD_A        3.0
#define DT_S          1.0
#define DURATION_S    3600
#define LOG_EVERY_S   60

static void print_header(void)
{
    printf("time_s,current_a,pack_v");
    for (int i = 0; i < SIM_PACK_CELLS; i++) {
        printf(",cell%d_v,cell%d_soc,cell%d_temp_c", i, i, i);
    }
    printf("\n");
}

static void print_row(int t, const sim_pack_t *pack)
{
    printf("%d,%.3f,%.4f", t, pack->current_a, sim_pack_voltage(pack));
    for (int i = 0; i < SIM_PACK_CELLS; i++) {
        const sim_cell_t *c = &pack->cells[i];
        printf(",%.4f,%.4f,%.2f", sim_cell_voltage(c), c->soc, c->temp_c);
    }
    printf("\n");
}

int main(void)
{
    sim_pack_t pack;
    sim_pack_init(&pack, CAPACITY_AH, START_SOC, CELL_R_OHM);
    sim_pack_set_current(&pack, LOAD_A);
    print_header();
    for (int t = 0; t <= DURATION_S; t++) {
        if (t % LOG_EVERY_S == 0) {
            print_row(t, &pack);
        }
        sim_pack_step(&pack, DT_S);
    }
    return 0;
}
