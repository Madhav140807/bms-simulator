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
    return UNITY_END();
}
