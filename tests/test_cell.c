/* Unit tests for the single cell model. */
#include "unity.h"
#include "cell.h"

static sim_cell_t cell;

void setUp(void)
{
    sim_cell_init(&cell, 3.0, 0.5, 0.02);
}

void tearDown(void) {}

static void test_ocv_endpoints(void)
{
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.00, sim_ocv_from_soc(0.0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 4.20, sim_ocv_from_soc(1.0));
}

static void test_ocv_interpolates_between_points(void)
{
    /* Halfway between 50% (3.74) and 60% (3.82). */
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.78, sim_ocv_from_soc(0.55));
}

static void test_ocv_clamps_out_of_range(void)
{
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.00, sim_ocv_from_soc(-0.5));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 4.20, sim_ocv_from_soc(1.5));
}

static void test_ocv_is_monotonic(void)
{
    double prev = sim_ocv_from_soc(0.0);
    for (int i = 1; i <= 100; i++) {
        double v = sim_ocv_from_soc(i / 100.0);
        TEST_ASSERT_TRUE(v > prev);
        prev = v;
    }
}

static void test_discharge_one_hour_at_1c_empties_full_cell(void)
{
    cell.soc = 1.0;
    for (int i = 0; i < 3600; i++) {
        sim_cell_step(&cell, 3.0, 1.0, 25.0);
    }
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 0.0, cell.soc);
}

static void test_charge_raises_soc(void)
{
    sim_cell_step(&cell, -3.0, 360.0, 25.0);  /* 0.3 Ah in */
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.6, cell.soc);
}

static void test_soc_clamps_at_full_and_empty(void)
{
    sim_cell_step(&cell, -30.0, 3600.0, 25.0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, cell.soc);
    sim_cell_step(&cell, 30.0, 3600.0, 25.0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, cell.soc);
}

static void test_voltage_sags_under_load(void)
{
    sim_cell_step(&cell, 10.0, 0.0, 25.0);
    double ocv = sim_ocv_from_soc(cell.soc);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, ocv - 0.2, sim_cell_voltage(&cell));
}

static void test_voltage_rises_while_charging(void)
{
    sim_cell_step(&cell, -5.0, 0.0, 25.0);
    double ocv = sim_ocv_from_soc(cell.soc);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, ocv + 0.1, sim_cell_voltage(&cell));
}

static void test_cell_heats_under_load(void)
{
    for (int i = 0; i < 600; i++) {
        sim_cell_step(&cell, 10.0, 1.0, 25.0);
    }
    TEST_ASSERT_TRUE(cell.temp_c > 25.0);
}

static void test_hot_cell_cools_toward_ambient(void)
{
    cell.temp_c = 60.0;
    for (int i = 0; i < 600; i++) {
        sim_cell_step(&cell, 0.0, 1.0, 25.0);
    }
    TEST_ASSERT_TRUE(cell.temp_c < 60.0);
    TEST_ASSERT_TRUE(cell.temp_c > 25.0);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ocv_endpoints);
    RUN_TEST(test_ocv_interpolates_between_points);
    RUN_TEST(test_ocv_clamps_out_of_range);
    RUN_TEST(test_ocv_is_monotonic);
    RUN_TEST(test_discharge_one_hour_at_1c_empties_full_cell);
    RUN_TEST(test_charge_raises_soc);
    RUN_TEST(test_soc_clamps_at_full_and_empty);
    RUN_TEST(test_voltage_sags_under_load);
    RUN_TEST(test_voltage_rises_while_charging);
    RUN_TEST(test_cell_heats_under_load);
    RUN_TEST(test_hot_cell_cools_toward_ambient);
    return UNITY_END();
}
