#include "unity.h"
#include "hal_sim.h"
#include "balance.h"

static sim_pack_t pack;
static bal_t bal;
static bal_sample_t sample;
static const bal_config_t *cfg = &BAL_DEFAULT_CONFIG;

static void set_sample(uint16_t a, uint16_t b, uint16_t c, uint16_t d)
{
    const uint16_t mv[HAL_NUM_CELLS] = { a, b, c, d };
    for (uint8_t i = 0; i < HAL_NUM_CELLS; i++) {
        sample.cell_mv[i] = mv[i];
        sample.cell_temp_dc[i] = 250;
    }
    sample.current_ma = 0;
}

void setUp(void)
{
    sim_pack_init(&pack, 3.0, 0.5, 0.02);
    hal_sim_attach(&pack);
    balance_init(&bal, NULL);
    set_sample(3800, 3800, 3800, 3800);
}

void tearDown(void)
{
    hal_sim_attach(NULL);
}

static double cell_spread_mv(void)
{
    double lo = 1e9, hi = -1e9;
    for (int i = 0; i < SIM_PACK_CELLS; i++) {
        double v = sim_cell_voltage(&pack.cells[i]) * 1000.0;
        lo = v < lo ? v : lo;
        hi = v > hi ? v : hi;
    }
    return hi - lo;
}

/* ---- balance_select: pure logic ---- */

static void test_balanced_pack_bleeds_nothing(void)
{
    TEST_ASSERT_EQUAL_HEX8(0x00, balance_select(cfg, &sample, 0));
}

static void test_high_cells_start_bleeding(void)
{
    set_sample(3800, 3820, 3800, 3830);
    TEST_ASSERT_EQUAL_HEX8(0x0A, balance_select(cfg, &sample, 0));
}

static void test_lowest_cell_never_bleeds(void)
{
    set_sample(3850, 3850, 3800, 3850);
    TEST_ASSERT_EQUAL_HEX8(0x0B, balance_select(cfg, &sample, 0));
}

static void test_delta_at_start_threshold_does_not_start(void)
{
    set_sample(3800, 3815, 3800, 3800);
    TEST_ASSERT_EQUAL_HEX8(0x00, balance_select(cfg, &sample, 0));
}

static void test_hysteresis_keeps_bleeding_above_stop(void)
{
    set_sample(3800, 3810, 3800, 3800);
    TEST_ASSERT_EQUAL_HEX8(0x00, balance_select(cfg, &sample, 0x00));
    TEST_ASSERT_EQUAL_HEX8(0x02, balance_select(cfg, &sample, 0x02));
}

static void test_hysteresis_stops_within_stop_band(void)
{
    set_sample(3800, 3805, 3800, 3800);
    TEST_ASSERT_EQUAL_HEX8(0x00, balance_select(cfg, &sample, 0x02));
}

static void test_inhibited_while_discharging(void)
{
    set_sample(3800, 3850, 3800, 3800);
    sample.current_ma = 501;
    TEST_ASSERT_TRUE(balance_inhibited(cfg, &sample));
    TEST_ASSERT_EQUAL_HEX8(0x00, balance_select(cfg, &sample, 0x02));
}

static void test_allowed_while_charging_and_light_load(void)
{
    set_sample(3800, 3850, 3800, 3800);
    sample.current_ma = -1500;
    TEST_ASSERT_EQUAL_HEX8(0x02, balance_select(cfg, &sample, 0));
    sample.current_ma = 500;
    TEST_ASSERT_EQUAL_HEX8(0x02, balance_select(cfg, &sample, 0));
}

static void test_inhibited_when_lowest_cell_is_low(void)
{
    set_sample(3399, 3500, 3500, 3500);
    TEST_ASSERT_EQUAL_HEX8(0x00, balance_select(cfg, &sample, 0));
}

static void test_inhibited_when_hot(void)
{
    set_sample(3800, 3850, 3800, 3800);
    sample.cell_temp_dc[3] = 501;
    TEST_ASSERT_EQUAL_HEX8(0x00, balance_select(cfg, &sample, 0));
}

static void test_custom_config(void)
{
    bal_config_t c = BAL_DEFAULT_CONFIG;
    c.start_mv = 40;
    set_sample(3800, 3830, 3800, 3850);
    TEST_ASSERT_EQUAL_HEX8(0x08, balance_select(&c, &sample, 0));
}

/* ---- HAL integration ---- */

static void test_init_turns_bleeders_off(void)
{
    sim_pack_set_balance(&pack, 1, true);
    balance_init(&bal, NULL);
    TEST_ASSERT_FALSE(hal_get_balance(1));
    TEST_ASSERT_EQUAL_HEX8(0x00, bal.mask);
}

static void test_update_drives_hal(void)
{
    set_sample(3800, 3850, 3800, 3850);
    balance_update(&bal, &sample, true);
    TEST_ASSERT_EQUAL_HEX8(0x0A, bal.mask);
    TEST_ASSERT_FALSE(hal_get_balance(0));
    TEST_ASSERT_TRUE(hal_get_balance(1));
    TEST_ASSERT_TRUE(hal_get_balance(3));
}

static void test_not_allowed_turns_everything_off(void)
{
    set_sample(3800, 3850, 3800, 3850);
    balance_update(&bal, &sample, true);
    balance_update(&bal, &sample, false);
    TEST_ASSERT_EQUAL_HEX8(0x00, bal.mask);
    TEST_ASSERT_FALSE(hal_get_balance(1));
}

static void test_step_reads_sim(void)
{
    pack.cells[2].soc = 0.7;
    balance_step(&bal, true);
    TEST_ASSERT_EQUAL_HEX8(0x04, bal.mask);
    TEST_ASSERT_TRUE(pack.balance_on[2]);
}

static void test_imbalanced_pack_converges_at_rest(void)
{
    pack.cells[0].soc = 0.60;
    pack.cells[1].soc = 0.55;
    pack.cells[3].soc = 0.58;
    TEST_ASSERT_TRUE(cell_spread_mv() > 50.0);
    for (int t = 0; t < 6 * 3600; t++) {
        balance_step(&bal, true);
        sim_pack_step(&pack, 1.0);
    }
    TEST_ASSERT_TRUE(cell_spread_mv() < 8.0);
    TEST_ASSERT_DOUBLE_WITHIN(0.03, 0.5, pack.cells[0].soc);
}

static void test_discharge_does_not_balance(void)
{
    pack.cells[0].soc = 0.60;
    sim_pack_set_current(&pack, 3.0);
    for (int t = 0; t < 60; t++) {
        balance_step(&bal, true);
        sim_pack_step(&pack, 1.0);
    }
    TEST_ASSERT_EQUAL_HEX8(0x00, bal.mask);
}

static void test_filter_primes_with_first_sample(void)
{
    set_sample(3800, 3850, 3700, 3900);
    balance_filter(&bal, &sample);
    TEST_ASSERT_EQUAL_UINT16(3850, sample.cell_mv[1]);
    TEST_ASSERT_EQUAL_UINT16(3700, sample.cell_mv[2]);
}

static void test_filter_smooths_a_single_spike(void)
{
    set_sample(3800, 3800, 3800, 3800);
    balance_filter(&bal, &sample);
    set_sample(3800, 3880, 3800, 3800);   /* one 80 mV glitch */
    balance_filter(&bal, &sample);
    TEST_ASSERT_EQUAL_UINT16(3810, sample.cell_mv[1]);
}

static void test_filter_converges_to_a_step(void)
{
    set_sample(3800, 3800, 3800, 3800);
    balance_filter(&bal, &sample);
    for (int i = 0; i < 80; i++) {
        set_sample(3800, 3840, 3800, 3800);
        balance_filter(&bal, &sample);
    }
    TEST_ASSERT_UINT16_WITHIN(1, 3840, sample.cell_mv[1]);
}

static void test_noise_does_not_chatter_bleeders(void)
{
    pack.cells[0].soc = 0.60;
    hal_sim_set_noise(&HAL_DEFAULT_NOISE, 3);
    int changes = 0;
    uint8_t prev = 0;
    for (int t = 0; t < 6 * 3600; t++) {
        balance_step(&bal, true);
        sim_pack_step(&pack, 1.0);
        changes += (bal.mask != prev);
        prev = bal.mask;
    }
    hal_sim_set_noise(NULL, 0);
    TEST_ASSERT_TRUE(changes <= 6);
    TEST_ASSERT_TRUE(cell_spread_mv() < 8.0);
}

static void test_bleed_sag_is_compensated(void)
{
    /* Cell 1 bleeding, reads 4 mV above min: with 2 mV sag added back it is
     * 6 mV above, still over stop_mv, so it keeps bleeding. */
    set_sample(3800, 3820, 3800, 3800);
    balance_update(&bal, &sample, true);
    TEST_ASSERT_EQUAL_HEX8(0x02, bal.mask);
    for (int i = 0; i < 100; i++) {
        set_sample(3800, 3804, 3800, 3800);
        balance_update(&bal, &sample, true);
    }
    TEST_ASSERT_EQUAL_HEX8(0x02, bal.mask);
    for (int i = 0; i < 100; i++) {
        set_sample(3800, 3802, 3800, 3800);
        balance_update(&bal, &sample, true);
    }
    TEST_ASSERT_EQUAL_HEX8(0x00, bal.mask);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_balanced_pack_bleeds_nothing);
    RUN_TEST(test_high_cells_start_bleeding);
    RUN_TEST(test_lowest_cell_never_bleeds);
    RUN_TEST(test_delta_at_start_threshold_does_not_start);
    RUN_TEST(test_hysteresis_keeps_bleeding_above_stop);
    RUN_TEST(test_hysteresis_stops_within_stop_band);
    RUN_TEST(test_inhibited_while_discharging);
    RUN_TEST(test_allowed_while_charging_and_light_load);
    RUN_TEST(test_inhibited_when_lowest_cell_is_low);
    RUN_TEST(test_inhibited_when_hot);
    RUN_TEST(test_custom_config);
    RUN_TEST(test_init_turns_bleeders_off);
    RUN_TEST(test_update_drives_hal);
    RUN_TEST(test_not_allowed_turns_everything_off);
    RUN_TEST(test_step_reads_sim);
    RUN_TEST(test_imbalanced_pack_converges_at_rest);
    RUN_TEST(test_discharge_does_not_balance);
    RUN_TEST(test_filter_primes_with_first_sample);
    RUN_TEST(test_filter_smooths_a_single_spike);
    RUN_TEST(test_filter_converges_to_a_step);
    RUN_TEST(test_noise_does_not_chatter_bleeders);
    RUN_TEST(test_bleed_sag_is_compensated);
    return UNITY_END();
}
