#include <string.h>
#include "unity.h"
#include "csv.h"
#include "scenario.h"
#include "system.h"

static bms_sys_t sys;
static scn_run_t run;

void setUp(void)
{
    sys_init(&sys, 0.5);
}

void tearDown(void)
{
}

/* Runs the named scenario to its end. */
static void run_to_end(const char *name)
{
    const scenario_t *scn = scenario_find(name);
    TEST_ASSERT_NOT_NULL(scn);
    scenario_start(&run, scn, &sys);
    while (!scenario_done(&run, &sys)) {
        sys_step(&sys);
        scenario_apply(&run, &sys);
    }
}

static int count_char(const char *s, char c)
{
    int n = 0;
    for (; *s != '\0'; s++) {
        n += (*s == c);
    }
    return n;
}

/* ---- system ---- */

static void test_sys_init_state(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, sys.time_s);
    TEST_ASSERT_TRUE(sys.balancing);
    TEST_ASSERT_TRUE(sys.pack.contactor_closed);
    TEST_ASSERT_DOUBLE_WITHIN(1.0, 50.0, soc_get(&sys.soc) / 100.0);
}

static void test_sys_step_advances_one_second(void)
{
    sim_pack_set_current(&sys.pack, 3.0);
    sys_step(&sys);
    TEST_ASSERT_EQUAL_UINT32(1, sys.time_s);
    TEST_ASSERT_TRUE(sys.pack.cells[0].soc < 0.5);
}

static void test_sys_spread(void)
{
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, sys_cell_spread_mv(&sys));
    sys_set_cell_soc(&sys, 1, 0.6);
    TEST_ASSERT_TRUE(sys_cell_spread_mv(&sys) > 30.0);
}

static void test_sys_disabled_balancing_stays_off(void)
{
    sys_set_cell_soc(&sys, 1, 0.6);
    sys.balancing = false;
    sys_step(&sys);
    TEST_ASSERT_EQUAL_HEX8(0x00, sys.bal.mask);
    sys.balancing = true;
    sys_step(&sys);
    TEST_ASSERT_EQUAL_HEX8(0x02, sys.bal.mask);
}

/* ---- scenario table ---- */

static void test_find_and_get(void)
{
    TEST_ASSERT_TRUE(scenario_count() >= 6);
    TEST_ASSERT_EQUAL_PTR(scenario_get(0), scenario_find(scenario_get(0)->name));
    TEST_ASSERT_NULL(scenario_find("nope"));
    TEST_ASSERT_NULL(scenario_find(NULL));
    TEST_ASSERT_NULL(scenario_get(-1));
    TEST_ASSERT_NULL(scenario_get(scenario_count()));
}

static void test_table_is_well_formed(void)
{
    for (int i = 0; i < scenario_count(); i++) {
        const scenario_t *s = scenario_get(i);
        TEST_ASSERT_TRUE(s->duration_s > 0);
        TEST_ASSERT_TRUE(strlen(s->desc) > 0);
        for (int j = i + 1; j < scenario_count(); j++) {
            TEST_ASSERT_TRUE(strcmp(s->name, scenario_get(j)->name) != 0);
        }
        for (int e = 1; e < s->n_events; e++) {
            TEST_ASSERT_TRUE(s->events[e - 1].at_s <= s->events[e].at_s);
        }
    }
}

static void test_events_apply_on_time(void)
{
    scenario_start(&run, scenario_find("overcurrent"), &sys);
    TEST_ASSERT_EQUAL_DOUBLE(3.0, sys.pack.load_a);
    while (sys.time_s < 59) {
        sys_step(&sys);
        scenario_apply(&run, &sys);
    }
    TEST_ASSERT_EQUAL_DOUBLE(3.0, sys.pack.load_a);
    sys_step(&sys);
    scenario_apply(&run, &sys);
    TEST_ASSERT_EQUAL_DOUBLE(12.0, sys.pack.load_a);
}

static void test_start_seeds_soc_from_lowest_cell(void)
{
    scenario_start(&run, scenario_find("weak_cell"), &sys);
    TEST_ASSERT_DOUBLE_WITHIN(1.0, 30.0, soc_get(&sys.soc) / 100.0);
}

/* ---- scenario outcomes ---- */

static void test_discharge_ends_in_uv(void)
{
    run_to_end("discharge_1c");
    TEST_ASSERT_EQUAL_HEX8(PROT_FAULT_UV, sys.prot.faults);
    TEST_ASSERT_FALSE(sys.pack.contactor_closed);
}

static void test_charge_ends_in_ov(void)
{
    run_to_end("charge");
    TEST_ASSERT_EQUAL_HEX8(PROT_FAULT_OV, sys.prot.faults);
    TEST_ASSERT_TRUE(sys.pack.cells[0].soc > 0.95);
}

static void test_overcurrent_trips_then_recovers(void)
{
    const scenario_t *scn = scenario_find("overcurrent");
    scenario_start(&run, scn, &sys);
    bool tripped = false;
    while (!scenario_done(&run, &sys)) {
        sys_step(&sys);
        scenario_apply(&run, &sys);
        tripped |= (sys.prot.faults & PROT_FAULT_OC_DSG) != 0;
    }
    TEST_ASSERT_TRUE(tripped);
    TEST_ASSERT_EQUAL_HEX8(0x00, sys.prot.faults);
    TEST_ASSERT_TRUE(sys.pack.contactor_closed);
}

static void test_over_temp_trips(void)
{
    run_to_end("over_temp");
    TEST_ASSERT_EQUAL_HEX8(PROT_FAULT_OT, sys.prot.faults);
}

static void test_imbalance_converges(void)
{
    run_to_end("imbalance");
    TEST_ASSERT_TRUE(sys_cell_spread_mv(&sys) < 10.0);
    TEST_ASSERT_EQUAL_HEX8(0x00, sys.prot.faults);
}

static void test_weak_cell_limits_pack(void)
{
    run_to_end("weak_cell");
    TEST_ASSERT_EQUAL_HEX8(PROT_FAULT_UV, sys.prot.faults);
    TEST_ASSERT_TRUE(sys.pack.cells[0].soc > 0.6);
}

/* ---- CSV ---- */

static void test_csv_rows_and_columns(void)
{
    FILE *f = tmpfile();
    TEST_ASSERT_NOT_NULL(f);
    int rows = csv_run(f, scenario_find("overcurrent"), 60);
    TEST_ASSERT_EQUAL_INT(600 / 60 + 1, rows);
    rewind(f);
    char line[512];
    int lines = 0, cols = -1;
    while (fgets(line, sizeof line, f) != NULL) {
        int c = count_char(line, ',');
        TEST_ASSERT_TRUE(cols < 0 || c == cols);
        cols = c;
        lines++;
    }
    fclose(f);
    TEST_ASSERT_EQUAL_INT(rows + 1, lines);
    TEST_ASSERT_EQUAL_INT(3 + 3 * SIM_PACK_CELLS + 4 - 1, cols);
}

static void test_csv_includes_final_row(void)
{
    FILE *f = tmpfile();
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQUAL_INT(600 / 70 + 2, csv_run(f, scenario_find("overcurrent"), 70));
    fclose(f);
}

static void test_csv_header_starts_with_time(void)
{
    FILE *f = tmpfile();
    TEST_ASSERT_NOT_NULL(f);
    csv_header(f);
    rewind(f);
    char line[512];
    TEST_ASSERT_NOT_NULL(fgets(line, sizeof line, f));
    fclose(f);
    TEST_ASSERT_EQUAL_INT(0, strncmp(line, "time_s,current_a,pack_v,", 24));
    TEST_ASSERT_NOT_NULL(strstr(line, ",balance_mask\n"));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_sys_init_state);
    RUN_TEST(test_sys_step_advances_one_second);
    RUN_TEST(test_sys_spread);
    RUN_TEST(test_sys_disabled_balancing_stays_off);
    RUN_TEST(test_find_and_get);
    RUN_TEST(test_table_is_well_formed);
    RUN_TEST(test_events_apply_on_time);
    RUN_TEST(test_start_seeds_soc_from_lowest_cell);
    RUN_TEST(test_discharge_ends_in_uv);
    RUN_TEST(test_charge_ends_in_ov);
    RUN_TEST(test_overcurrent_trips_then_recovers);
    RUN_TEST(test_over_temp_trips);
    RUN_TEST(test_imbalance_converges);
    RUN_TEST(test_weak_cell_limits_pack);
    RUN_TEST(test_csv_rows_and_columns);
    RUN_TEST(test_csv_includes_final_row);
    RUN_TEST(test_csv_header_starts_with_time);
    return UNITY_END();
}
