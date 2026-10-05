/* Unit tests for the series pack model. */
#include "unity.h"
#include "pack.h"

static sim_pack_t pack;

void setUp(void)
{
    sim_pack_init(&pack, 3.0, 0.5, 0.02);
}

void tearDown(void) {}

static void test_init_sets_all_cells(void)
{
    for (int i = 0; i < SIM_PACK_CELLS; i++) {
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.5, pack.cells[i].soc);
    }
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, sim_pack_current(&pack));
    TEST_ASSERT_TRUE(pack.contactor_closed);
}

static void test_pack_voltage_is_sum_of_cells(void)
{
    double expected = SIM_PACK_CELLS * sim_ocv_from_soc(0.5);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, expected, sim_pack_voltage(&pack));
}

static void test_series_current_drains_every_cell_equally(void)
{
    sim_pack_set_current(&pack, 3.0);
    sim_pack_step(&pack, 360.0);  /* 0.3 Ah out */
    for (int i = 0; i < SIM_PACK_CELLS; i++) {
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.4, pack.cells[i].soc);
    }
}

static void test_unbalanced_cells_give_different_voltages(void)
{
    pack.cells[0].soc = 0.9;
    double v0 = sim_cell_voltage(&pack.cells[0]);
    double v1 = sim_cell_voltage(&pack.cells[1]);
    TEST_ASSERT_TRUE(v0 > v1);
}

static void test_pack_voltage_sags_under_load(void)
{
    double rest = sim_pack_voltage(&pack);
    sim_pack_set_current(&pack, 10.0);
    sim_pack_step(&pack, 0.0);
    double load = sim_pack_voltage(&pack);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, rest - SIM_PACK_CELLS * 0.2, load);
}

static void test_open_contactor_stops_load_current(void)
{
    sim_pack_set_current(&pack, 3.0);
    sim_pack_set_contactor(&pack, false);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, sim_pack_current(&pack));
    sim_pack_step(&pack, 360.0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.5, pack.cells[0].soc);
}

static void test_balance_bleeds_only_selected_cell(void)
{
    sim_pack_set_balance(&pack, 2, true);
    double bleed_a = sim_ocv_from_soc(0.5) / pack.balance_r_ohm;
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, bleed_a, sim_pack_balance_current(&pack, 2));
    sim_pack_step(&pack, 60.0);
    TEST_ASSERT_TRUE(pack.cells[2].soc < 0.5);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.5, pack.cells[1].soc);
}

static void test_balance_ignores_bad_cell_index(void)
{
    sim_pack_set_balance(&pack, -1, true);
    sim_pack_set_balance(&pack, SIM_PACK_CELLS, true);
    for (int i = 0; i < SIM_PACK_CELLS; i++) {
        TEST_ASSERT_FALSE(pack.balance_on[i]);
    }
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, sim_pack_balance_current(&pack, SIM_PACK_CELLS));
}

static void test_mismatch_changes_cells_within_3_sigma(void)
{
    sim_pack_apply_mismatch(&pack, &SIM_DEFAULT_MISMATCH, 1);
    int differ = 0;
    for (int i = 0; i < SIM_PACK_CELLS; i++) {
        const sim_cell_t *c = &pack.cells[i];
        TEST_ASSERT_DOUBLE_WITHIN(3.0 * 0.015 * 3.0 + 1e-9, 3.0, c->capacity_ah);
        TEST_ASSERT_DOUBLE_WITHIN(3.0 * 0.08 * 0.02 + 1e-9, 0.02, c->r_ohm);
        TEST_ASSERT_DOUBLE_WITHIN(3.0 * 0.005 + 1e-9, 0.5, c->soc);
        differ += (c->capacity_ah != 3.0);
    }
    TEST_ASSERT_EQUAL_INT(SIM_PACK_CELLS, differ);
}

static void test_mismatch_is_reproducible(void)
{
    sim_pack_t other;
    sim_pack_init(&other, 3.0, 0.5, 0.02);
    sim_pack_apply_mismatch(&pack, &SIM_DEFAULT_MISMATCH, 9);
    sim_pack_apply_mismatch(&other, &SIM_DEFAULT_MISMATCH, 9);
    for (int i = 0; i < SIM_PACK_CELLS; i++) {
        TEST_ASSERT_EQUAL_DOUBLE(other.cells[i].r_ohm, pack.cells[i].r_ohm);
    }
}

static void test_mismatched_cells_drift_apart_under_load(void)
{
    sim_pack_apply_mismatch(&pack, &SIM_DEFAULT_MISMATCH, 1);
    double before = pack.cells[0].soc - pack.cells[1].soc;
    sim_pack_set_current(&pack, 3.0);
    for (int t = 0; t < 1800; t++) {
        sim_pack_step(&pack, 1.0);
    }
    double after = pack.cells[0].soc - pack.cells[1].soc;
    TEST_ASSERT_TRUE(after != before);
}

static void test_heater_warms_only_its_cell(void)
{
    sim_pack_set_heater(&pack, 1, 3.0);
    for (int t = 0; t < 600; t++) {
        sim_pack_step(&pack, 1.0);
    }
    TEST_ASSERT_TRUE(pack.cells[1].temp_c > 45.0);
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 25.0, pack.cells[0].temp_c);
}

static void test_short_drains_and_heats_its_cell(void)
{
    sim_pack_set_short(&pack, 2, 2.0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.74 / 2.0, sim_pack_short_current(&pack, 2));
    for (int t = 0; t < 600; t++) {
        sim_pack_step(&pack, 1.0);
    }
    TEST_ASSERT_TRUE(pack.cells[2].soc < 0.42);
    TEST_ASSERT_TRUE(pack.cells[2].temp_c > 60.0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.5, pack.cells[0].soc);
}

static void test_open_contactor_does_not_stop_short(void)
{
    sim_pack_set_contactor(&pack, false);
    sim_pack_set_short(&pack, 0, 2.0);
    sim_pack_step(&pack, 60.0);
    TEST_ASSERT_TRUE(pack.cells[0].soc < 0.5);
}

static void test_injection_ignores_bad_cell_and_clears(void)
{
    sim_pack_set_heater(&pack, 7, 3.0);
    sim_pack_set_short(&pack, -1, 2.0);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, sim_pack_short_current(&pack, -1));
    sim_pack_set_short(&pack, 0, 2.0);
    sim_pack_set_short(&pack, 0, 0.0);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, sim_pack_short_current(&pack, 0));
}

static void test_load_change_sags_voltage_immediately(void)
{
    double rest = sim_cell_voltage(&pack.cells[0]);
    sim_pack_set_current(&pack, 3.0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, rest - 3.0 * 0.02, sim_cell_voltage(&pack.cells[0]));
    sim_pack_set_contactor(&pack, false);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, rest, sim_cell_voltage(&pack.cells[0]));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_sets_all_cells);
    RUN_TEST(test_pack_voltage_is_sum_of_cells);
    RUN_TEST(test_series_current_drains_every_cell_equally);
    RUN_TEST(test_unbalanced_cells_give_different_voltages);
    RUN_TEST(test_pack_voltage_sags_under_load);
    RUN_TEST(test_open_contactor_stops_load_current);
    RUN_TEST(test_balance_bleeds_only_selected_cell);
    RUN_TEST(test_balance_ignores_bad_cell_index);
    RUN_TEST(test_mismatch_changes_cells_within_3_sigma);
    RUN_TEST(test_mismatch_is_reproducible);
    RUN_TEST(test_mismatched_cells_drift_apart_under_load);
    RUN_TEST(test_heater_warms_only_its_cell);
    RUN_TEST(test_short_drains_and_heats_its_cell);
    RUN_TEST(test_open_contactor_does_not_stop_short);
    RUN_TEST(test_injection_ignores_bad_cell_and_clears);
    RUN_TEST(test_load_change_sags_voltage_immediately);
    return UNITY_END();
}
