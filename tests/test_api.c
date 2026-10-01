#include <string.h>
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
    TEST_ASSERT_DOUBLE_WITHIN(0.016, 0.5, api_cell_soc(0));   /* mismatch: 3 sigma */
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

/* The pack SOC the firmware estimates: its lowest (limiting) cell. */
static double min_cell_soc_pct(void)
{
    double min = 1.0;
    for (int i = 0; i < api_num_cells(); i++) {
        min = api_cell_soc(i) < min ? api_cell_soc(i) : min;
    }
    return min * 100.0;
}

void test_soc_estimate_seeded_from_ocv(void)
{
    TEST_ASSERT_DOUBLE_WITHIN(1.5, min_cell_soc_pct(), api_soc_est_pct());
}

void test_soc_estimate_tracks_discharge(void)
{
    api_reset(1.0);
    api_set_load(3.0);
    api_step(600);   /* 0.5 Ah of 3 Ah = 16.7 % */
    TEST_ASSERT_DOUBLE_WITHIN(2.0, 100.0 - 16.67, api_soc_est_pct());
    TEST_ASSERT_DOUBLE_WITHIN(1.5, min_cell_soc_pct(), api_soc_est_pct());
}

void test_soc_estimate_corrects_after_rest(void)
{
    api_set_cell_soc(1, 0.2);   /* estimate still thinks 50 % */
    api_step(10);
    TEST_ASSERT_TRUE(api_soc_est_pct() > 45.0);
    api_step(300);              /* 5 min rest triggers OCV correction */
    TEST_ASSERT_DOUBLE_WITHIN(1.5, 20.0, api_soc_est_pct());
}

static void set_all_cells(double soc)
{
    for (int i = 0; i < api_num_cells(); i++) {
        api_set_cell_soc(i, soc);
    }
}

void test_balancing_bleeds_high_cell_at_rest(void)
{
    set_all_cells(0.5);
    api_set_cell_soc(0, 0.6);
    api_step(2);
    TEST_ASSERT_EQUAL_INT(0x01, api_balance_mask());
    api_step(3 * 3600);
    TEST_ASSERT_EQUAL_INT(0x00, api_balance_mask());
    TEST_ASSERT_DOUBLE_WITHIN(0.02, 0.5, api_cell_soc(0));
}

void test_balancing_can_be_disabled(void)
{
    api_set_cell_soc(0, 0.6);
    api_set_balancing(0);
    api_step(10);
    TEST_ASSERT_EQUAL_INT(0x00, api_balance_mask());
}

void test_fault_stops_balancing(void)
{
    api_set_cell_soc(0, 0.6);
    api_step(2);
    api_set_load(-5.0);   /* over current while charging */
    api_step(5);
    TEST_ASSERT_TRUE(api_faults() & PROT_FAULT_OC_CHG);
    TEST_ASSERT_EQUAL_INT(0x00, api_balance_mask());
}

void test_scenario_names_and_bounds(void)
{
    TEST_ASSERT_TRUE(api_scenario_count() >= 6);
    TEST_ASSERT_EQUAL_STRING("discharge_1c", api_scenario_name(0));
    TEST_ASSERT_TRUE(strlen(api_scenario_desc(0)) > 0);
    TEST_ASSERT_EQUAL_STRING("", api_scenario_name(99));
    TEST_ASSERT_EQUAL_INT(0, api_start_scenario(99));
    TEST_ASSERT_EQUAL_INT(0, api_scenario_active());
}

void test_scenario_runs_and_stops_at_end(void)
{
    TEST_ASSERT_EQUAL_INT(1, api_start_scenario(0));
    TEST_ASSERT_EQUAL_INT(1, api_scenario_active());
    TEST_ASSERT_EQUAL_DOUBLE(3.0, api_load_a());
    api_step(10000);
    TEST_ASSERT_EQUAL_INT(1, api_scenario_done());
    TEST_ASSERT_EQUAL_DOUBLE(api_scenario_duration_s(), api_time_s());
    TEST_ASSERT_TRUE(api_faults() & PROT_FAULT_UV);
}

void test_reset_leaves_scenario_mode(void)
{
    api_start_scenario(0);
    api_reset(0.5);
    TEST_ASSERT_EQUAL_INT(0, api_scenario_active());
    TEST_ASSERT_EQUAL_INT(0, api_scenario_done());
    api_step(5);
    TEST_ASSERT_EQUAL_DOUBLE(5.0, api_time_s());
}

void test_corrupted_estimates_kalman_recovers_coulomb_does_not(void)
{
    api_reset(0.9);
    api_set_load(3.0);
    api_step(10);
    api_corrupt_estimates(50.0);
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 50.0, api_ekf_pct());
    api_step(30);
    TEST_ASSERT_DOUBLE_WITHIN(2.0, min_cell_soc_pct(), api_ekf_pct());
    TEST_ASSERT_TRUE(min_cell_soc_pct() - api_soc_est_pct() > 30.0);
    TEST_ASSERT_TRUE(api_ekf_sigma_pct() < 2.0);
}

void test_ekf_cell_estimates_follow_cells(void)
{
    api_set_cell_soc(1, 0.8);
    api_set_load(1.0);
    api_step(30);
    TEST_ASSERT_DOUBLE_WITHIN(2.0, api_cell_soc(1) * 100.0, api_ekf_cell_pct(1));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, api_ekf_cell_pct(9));
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
    RUN_TEST(test_soc_estimate_seeded_from_ocv);
    RUN_TEST(test_soc_estimate_tracks_discharge);
    RUN_TEST(test_soc_estimate_corrects_after_rest);
    RUN_TEST(test_balancing_bleeds_high_cell_at_rest);
    RUN_TEST(test_balancing_can_be_disabled);
    RUN_TEST(test_fault_stops_balancing);
    RUN_TEST(test_scenario_names_and_bounds);
    RUN_TEST(test_scenario_runs_and_stops_at_end);
    RUN_TEST(test_reset_leaves_scenario_mode);
    RUN_TEST(test_corrupted_estimates_kalman_recovers_coulomb_does_not);
    RUN_TEST(test_ekf_cell_estimates_follow_cells);
    return UNITY_END();
}
