#include "unity.h"
#include "ekf.h"
#include "hal_sim.h"
#include "ocv.h"

static sim_pack_t pack;
static ekf_t ekf;

void setUp(void)
{
    sim_pack_init(&pack, 3.0, 0.5, 0.02);
    hal_sim_attach(&pack);
    ekf_init(&ekf, NULL);
}

void tearDown(void)
{
    hal_sim_attach(NULL);
    hal_sim_set_noise(NULL, 0);
}

static double pct(uint16_t cpct)
{
    return cpct / 100.0;
}

/* Runs the sim and the filter together for `seconds` at `load_a`. */
static void run(double load_a, int seconds)
{
    sim_pack_set_current(&pack, load_a);
    for (int t = 0; t < seconds; t++) {
        ekf_step(&ekf, 1000);
        sim_pack_step(&pack, 1.0);
    }
}

/* ---- OCV helpers ---- */

static void test_ocv_mv_at_table_points(void)
{
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 3000.0f, ocv_mv_at(0.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 3740.0f, ocv_mv_at(0.5f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 4200.0f, ocv_mv_at(1.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 4200.0f, ocv_mv_at(1.5f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 3225.0f, ocv_mv_at(0.05f));
}

static void test_ocv_slope(void)
{
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 4500.0f, ocv_slope_mv(0.05f));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 600.0f, ocv_slope_mv(0.45f));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 1300.0f, ocv_slope_mv(1.0f));
}

static void test_ocv_round_trip(void)
{
    for (int c = 0; c <= 10000; c += 250) {
        uint16_t mv = (uint16_t)(ocv_mv_at(c / 10000.0f) + 0.5f);
        TEST_ASSERT_UINT16_WITHIN(25, c, ocv_soc_cpct(mv));
    }
}

/* ---- filter ---- */

static void test_init_seeds_each_cell_from_ocv(void)
{
    pack.cells[2].soc = 0.8;
    ekf_init(&ekf, NULL);
    TEST_ASSERT_DOUBLE_WITHIN(0.2, 50.0, pct(ekf_cell_cpct(&ekf, 0)));
    TEST_ASSERT_DOUBLE_WITHIN(0.2, 80.0, pct(ekf_cell_cpct(&ekf, 2)));
    TEST_ASSERT_EQUAL_UINT16(500, ekf_sigma_cpct(&ekf, 0));
}

static void test_predict_counts_charge(void)
{
    ekf_set(&ekf, 10000);
    ekf_predict(&ekf, 0, 3000, 1800000);   /* 1.5 Ah of 3 Ah */
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 50.0, pct(ekf_cell_cpct(&ekf, 0)));
    TEST_ASSERT_TRUE(ekf.var[0] > EKF_DEFAULT_CONFIG.p0);
}

static void test_correct_with_matching_voltage_keeps_estimate(void)
{
    ekf_set(&ekf, 5000);
    ekf_correct(&ekf, 1, 3740 - 60, 3000);   /* OCV - 3 A * 20 mOhm */
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 50.0, pct(ekf_cell_cpct(&ekf, 1)));
    TEST_ASSERT_TRUE(ekf.var[1] < EKF_DEFAULT_CONFIG.p0);
}

static void test_correct_moves_toward_measurement(void)
{
    ekf_set(&ekf, 5000);
    ekf_correct(&ekf, 0, 3820, 0);   /* reads like 60 % */
    TEST_ASSERT_TRUE(pct(ekf_cell_cpct(&ekf, 0)) > 55.0);
}

static void test_converges_from_wrong_start_under_load(void)
{
    pack.cells[0].soc = pack.cells[1].soc = pack.cells[2].soc = pack.cells[3].soc = 0.9;
    ekf_set(&ekf, 5000);
    run(3.0, 30);
    for (uint8_t i = 0; i < HAL_NUM_CELLS; i++) {
        TEST_ASSERT_DOUBLE_WITHIN(1.0, pack.cells[i].soc * 100.0, pct(ekf_cell_cpct(&ekf, i)));
    }
}

static void test_tracks_discharge_with_noise_and_mismatch(void)
{
    sim_pack_init(&pack, 3.0, 1.0, 0.02);
    sim_pack_apply_mismatch(&pack, &SIM_DEFAULT_MISMATCH, 4);
    hal_sim_set_noise(&HAL_DEFAULT_NOISE, 4);
    ekf_init(&ekf, NULL);
    double worst = 0.0;
    for (int m = 0; m < 50; m++) {   /* 50 minutes of 1C */
        run(3.0, 60);
        for (uint8_t i = 0; i < HAL_NUM_CELLS; i++) {
            double err = pct(ekf_cell_cpct(&ekf, i)) - pack.cells[i].soc * 100.0;
            worst = err < 0 ? (-err > worst ? -err : worst) : (err > worst ? err : worst);
        }
    }
    TEST_ASSERT_TRUE(worst < 3.0);
}

static void test_sigma_shrinks_with_measurements(void)
{
    uint16_t before = ekf_sigma_cpct(&ekf, 0);
    run(1.0, 10);
    TEST_ASSERT_TRUE(ekf_sigma_cpct(&ekf, 0) < before);
}

static void test_pack_is_lowest_cell(void)
{
    pack.cells[3].soc = 0.3;
    ekf_init(&ekf, NULL);
    TEST_ASSERT_EQUAL_UINT8(3, ekf_min_cell(&ekf));
    TEST_ASSERT_EQUAL_UINT16(ekf_cell_cpct(&ekf, 3), ekf_pack_cpct(&ekf));
}

static void test_estimate_clamps_to_range(void)
{
    ekf_set(&ekf, 100);
    ekf_predict(&ekf, 0, 10000, 3600000);
    TEST_ASSERT_EQUAL_UINT16(0, ekf_cell_cpct(&ekf, 0));
    ekf_set(&ekf, 12000);
    TEST_ASSERT_EQUAL_UINT16(10000, ekf_cell_cpct(&ekf, 0));
    TEST_ASSERT_EQUAL_UINT16(0, ekf_cell_cpct(&ekf, HAL_NUM_CELLS));
}

static void test_implausible_reading_is_skipped(void)
{
    ekf_set(&ekf, 5000);
    float var = ekf.var[0];
    ekf_correct(&ekf, 0, 0, 0);
    ekf_correct(&ekf, 0, 6000, 0);
    TEST_ASSERT_EQUAL_UINT16(5000, ekf_cell_cpct(&ekf, 0));
    TEST_ASSERT_EQUAL_FLOAT(var, ekf.var[0]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ocv_mv_at_table_points);
    RUN_TEST(test_ocv_slope);
    RUN_TEST(test_ocv_round_trip);
    RUN_TEST(test_init_seeds_each_cell_from_ocv);
    RUN_TEST(test_predict_counts_charge);
    RUN_TEST(test_correct_with_matching_voltage_keeps_estimate);
    RUN_TEST(test_correct_moves_toward_measurement);
    RUN_TEST(test_converges_from_wrong_start_under_load);
    RUN_TEST(test_tracks_discharge_with_noise_and_mismatch);
    RUN_TEST(test_sigma_shrinks_with_measurements);
    RUN_TEST(test_pack_is_lowest_cell);
    RUN_TEST(test_estimate_clamps_to_range);
    RUN_TEST(test_implausible_reading_is_skipped);
    return UNITY_END();
}
