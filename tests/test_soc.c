#include "unity.h"
#include "hal_sim.h"
#include "soc.h"

static sim_pack_t pack;
static soc_t soc;

void setUp(void)
{
    sim_pack_init(&pack, 3.0, 0.5, 0.02);
    hal_sim_attach(&pack);
    soc_init(&soc, NULL);
}

void tearDown(void)
{
    hal_sim_attach(NULL);
}

static void run_seconds(uint32_t seconds)
{
    for (uint32_t t = 0; t < seconds; t++) {
        soc_step(&soc, 1000);
        sim_pack_step(&pack, 1.0);
    }
}

/* ---- OCV lookup ---- */

static void test_ocv_table_points(void)
{
    TEST_ASSERT_EQUAL_UINT16(0, soc_from_ocv_mv(3000));
    TEST_ASSERT_EQUAL_UINT16(5000, soc_from_ocv_mv(3740));
    TEST_ASSERT_EQUAL_UINT16(10000, soc_from_ocv_mv(4200));
}

static void test_ocv_interpolates(void)
{
    TEST_ASSERT_EQUAL_UINT16(500, soc_from_ocv_mv(3225));
    TEST_ASSERT_EQUAL_UINT16(9500, soc_from_ocv_mv(4135));
}

static void test_ocv_clamps_out_of_range(void)
{
    TEST_ASSERT_EQUAL_UINT16(0, soc_from_ocv_mv(2500));
    TEST_ASSERT_EQUAL_UINT16(10000, soc_from_ocv_mv(4300));
}

static void test_ocv_is_monotonic(void)
{
    uint16_t prev = 0;
    for (uint16_t mv = 2900; mv <= 4300; mv++) {
        uint16_t s = soc_from_ocv_mv(mv);
        TEST_ASSERT_TRUE(s >= prev);
        prev = s;
    }
}

/* ---- init, set, get ---- */

static void test_init_seeds_from_ocv(void)
{
    TEST_ASSERT_UINT16_WITHIN(5, 5000, soc_get(&soc));
}

static void test_init_uses_lowest_cell(void)
{
    pack.cells[2].soc = 0.2;
    soc_init(&soc, NULL);
    TEST_ASSERT_UINT16_WITHIN(5, 2000, soc_get(&soc));
}

static void test_set_clamps_above_full(void)
{
    soc_set(&soc, 12000);
    TEST_ASSERT_EQUAL_UINT16(SOC_FULL_CPCT, soc_get(&soc));
}

/* ---- coulomb counting ---- */

static void test_discharge_decreases_soc(void)
{
    soc_set(&soc, 5000);
    /* 3 A for 360 s = 0.3 Ah = 10 % of 3 Ah */
    soc_update(&soc, 3000, 3700, 360000);
    TEST_ASSERT_EQUAL_UINT16(4000, soc_get(&soc));
}

static void test_charge_increases_soc(void)
{
    soc_set(&soc, 5000);
    soc_update(&soc, -1500, 3700, 360000);
    TEST_ASSERT_EQUAL_UINT16(5500, soc_get(&soc));
}

static void test_soc_clamps_at_empty_and_full(void)
{
    soc_set(&soc, 100);
    soc_update(&soc, 3000, 3700, 3600000);
    TEST_ASSERT_EQUAL_UINT16(0, soc_get(&soc));
    soc_set(&soc, 9900);
    soc_update(&soc, -3000, 3700, 3600000);
    TEST_ASSERT_EQUAL_UINT16(SOC_FULL_CPCT, soc_get(&soc));
}

static void test_tracks_sim_during_discharge(void)
{
    sim_pack_set_current(&pack, 3.0);
    run_seconds(1200);
    uint16_t truth = (uint16_t)(pack.cells[0].soc * SOC_FULL_CPCT + 0.5);
    TEST_ASSERT_UINT16_WITHIN(5, truth, soc_get(&soc));
}

/* ---- OCV correction at rest ---- */

static void test_no_correction_under_load(void)
{
    soc_set(&soc, 8000);
    soc_update(&soc, 1000, 3740, SOC_DEFAULT_CONFIG.rest_ms);
    TEST_ASSERT_UINT16_WITHIN(5, 7722, soc_get(&soc));
}

static void test_correction_after_rest(void)
{
    soc_set(&soc, 8000);
    soc_update(&soc, 0, 3740, SOC_DEFAULT_CONFIG.rest_ms - 1000);
    TEST_ASSERT_EQUAL_UINT16(8000, soc_get(&soc));
    soc_update(&soc, 0, 3740, 1000);
    TEST_ASSERT_EQUAL_UINT16(5000, soc_get(&soc));
}

static void test_load_resets_rest_timer(void)
{
    soc_set(&soc, 8000);
    soc_update(&soc, 0, 3740, SOC_DEFAULT_CONFIG.rest_ms - 1000);
    soc_update(&soc, 1000, 3740, 1);
    soc_update(&soc, 0, 3740, 1000);
    TEST_ASSERT_UINT16_WITHIN(1, 8000, soc_get(&soc));
}

static void test_small_current_counts_as_rest(void)
{
    soc_set(&soc, 8000);
    soc_update(&soc, (int32_t)SOC_DEFAULT_CONFIG.rest_ma, 3740,
               SOC_DEFAULT_CONFIG.rest_ms);
    TEST_ASSERT_EQUAL_UINT16(5000, soc_get(&soc));
}

static void test_rest_corrects_drifted_estimate(void)
{
    soc_set(&soc, 9000);  /* wrong: sim is at 50 % */
    run_seconds(SOC_DEFAULT_CONFIG.rest_ms / 1000);
    TEST_ASSERT_UINT16_WITHIN(5, 5000, soc_get(&soc));
}

static void test_custom_config(void)
{
    soc_config_t cfg = SOC_DEFAULT_CONFIG;
    cfg.capacity_mah = 1000;
    soc_init(&soc, &cfg);
    soc_set(&soc, 5000);
    soc_update(&soc, 1000, 3700, 360000);
    TEST_ASSERT_EQUAL_UINT16(4000, soc_get(&soc));
}

static void test_min_cell_ignores_open_sense_wire(void)
{
    hal_sim_set_sense_open(0, true);
    TEST_ASSERT_TRUE(soc_min_cell_mv() >= 1000);
    hal_sim_set_sense_open(0, false);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ocv_table_points);
    RUN_TEST(test_ocv_interpolates);
    RUN_TEST(test_ocv_clamps_out_of_range);
    RUN_TEST(test_ocv_is_monotonic);
    RUN_TEST(test_init_seeds_from_ocv);
    RUN_TEST(test_init_uses_lowest_cell);
    RUN_TEST(test_set_clamps_above_full);
    RUN_TEST(test_discharge_decreases_soc);
    RUN_TEST(test_charge_increases_soc);
    RUN_TEST(test_soc_clamps_at_empty_and_full);
    RUN_TEST(test_tracks_sim_during_discharge);
    RUN_TEST(test_no_correction_under_load);
    RUN_TEST(test_correction_after_rest);
    RUN_TEST(test_load_resets_rest_timer);
    RUN_TEST(test_small_current_counts_as_rest);
    RUN_TEST(test_rest_corrects_drifted_estimate);
    RUN_TEST(test_custom_config);
    RUN_TEST(test_min_cell_ignores_open_sense_wire);
    return UNITY_END();
}
