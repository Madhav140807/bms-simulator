#include "unity.h"
#include "api.h"
#include "protection.h"

void setUp(void)
{
    api_reset(0.5);
}

void tearDown(void)
{
}

void test_reset_state(void)
{
    TEST_ASSERT_EQUAL_DOUBLE(0.0, api_time_s());
    TEST_ASSERT_EQUAL_INT(1, api_contactor());
    TEST_ASSERT_EQUAL_INT(0, api_faults());
    TEST_ASSERT_EQUAL_INT(4, api_num_cells());
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.5, api_cell_soc(0));
}

void test_pack_voltage_is_sum_of_cells(void)
{
    double sum = 0.0;
    for (int i = 0; i < api_num_cells(); i++) {
        sum += api_cell_v(i);
    }
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, sum, api_pack_v());
}

void test_step_advances_time(void)
{
    api_step(10);
    TEST_ASSERT_EQUAL_DOUBLE(10.0, api_time_s());
}

void test_discharge_lowers_soc(void)
{
    api_set_load(3.0);
    api_step(60);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 3.0, api_current_a());
    TEST_ASSERT_TRUE(api_cell_soc(0) < 0.5);
}

void test_overcurrent_trips_and_opens_contactor(void)
{
    api_set_load(15.0);
    api_step(5);
    TEST_ASSERT_TRUE(api_faults() & PROT_FAULT_OC_DSG);
    TEST_ASSERT_EQUAL_INT(0, api_contactor());
    TEST_ASSERT_EQUAL_DOUBLE(0.0, api_current_a());
    TEST_ASSERT_EQUAL_DOUBLE(15.0, api_load_a());
}

void test_clear_after_load_removed(void)
{
    api_set_load(15.0);
    api_step(5);
    api_set_load(0.0);
    TEST_ASSERT_EQUAL_INT(1, api_clear_faults());
    TEST_ASSERT_EQUAL_INT(0, api_faults());
    TEST_ASSERT_EQUAL_INT(1, api_contactor());
}

void test_low_cell_trips_uv(void)
{
    api_set_cell_soc(2, 0.0);
    api_set_load(3.0);
    api_step(5);
    TEST_ASSERT_TRUE(api_faults() & PROT_FAULT_UV);
}

void test_set_cell_soc_clamps(void)
{
    api_set_cell_soc(1, 1.5);
    api_set_cell_soc(2, -0.5);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, api_cell_soc(1));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, api_cell_soc(2));
}

void test_bad_cell_index_reads_zero(void)
{
    api_set_cell_soc(9, 0.1);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, api_cell_v(-1));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, api_cell_soc(4));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, api_cell_temp_c(4));
}

void test_hot_ambient_trips_ot(void)
{
    api_set_ambient(80.0);
    api_set_load(1.0);
    api_step(3600);
    TEST_ASSERT_TRUE(api_cell_temp_c(0) > 60.0);
    TEST_ASSERT_TRUE(api_faults() & PROT_FAULT_OT);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_reset_state);
    RUN_TEST(test_pack_voltage_is_sum_of_cells);
    RUN_TEST(test_step_advances_time);
    RUN_TEST(test_discharge_lowers_soc);
    RUN_TEST(test_overcurrent_trips_and_opens_contactor);
    RUN_TEST(test_clear_after_load_removed);
    RUN_TEST(test_low_cell_trips_uv);
    RUN_TEST(test_set_cell_soc_clamps);
    RUN_TEST(test_bad_cell_index_reads_zero);
    RUN_TEST(test_hot_ambient_trips_ot);
    return UNITY_END();
}
