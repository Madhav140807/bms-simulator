#include "unity.h"
#include "hal_sim.h"
#include "protection.h"

static sim_pack_t pack;
static prot_t prot;
static prot_sample_t sample;
static const prot_limits_t *lim = &PROT_DEFAULT_LIMITS;

static void set_nominal_sample(void)
{
    for (uint8_t i = 0; i < HAL_NUM_CELLS; i++) {
        sample.cell_mv[i] = 3700;
        sample.cell_temp_dc[i] = 250;
    }
    sample.current_ma = 1000;
}

void setUp(void)
{
    sim_pack_init(&pack, 3.0, 0.5, 0.02);
    hal_sim_attach(&pack);
    protection_init(&prot, NULL);
    set_nominal_sample();
}

void tearDown(void)
{
    hal_sim_set_sense_open(1, false);
    hal_sim_attach(NULL);
}

static void step_n(int n)
{
    for (int i = 0; i < n; i++) {
        protection_step(&prot);
    }
}

/* ---- protection_check: pure threshold logic ---- */

static void test_check_nominal_is_clear(void)
{
    TEST_ASSERT_EQUAL_HEX8(PROT_FAULT_NONE, protection_check(lim, &sample));
}

static void test_check_over_voltage(void)
{
    sample.cell_mv[2] = lim->ov_mv;
    TEST_ASSERT_EQUAL_HEX8(PROT_FAULT_NONE, protection_check(lim, &sample));
    sample.cell_mv[2] = lim->ov_mv + 1;
    TEST_ASSERT_EQUAL_HEX8(PROT_FAULT_OV, protection_check(lim, &sample));
}

static void test_check_under_voltage(void)
{
    sample.cell_mv[0] = lim->uv_mv;
    TEST_ASSERT_EQUAL_HEX8(PROT_FAULT_NONE, protection_check(lim, &sample));
    sample.cell_mv[0] = lim->uv_mv - 1;
    TEST_ASSERT_EQUAL_HEX8(PROT_FAULT_UV, protection_check(lim, &sample));
}

static void test_check_over_current_discharge(void)
{
    sample.current_ma = lim->oc_dsg_ma + 1;
    TEST_ASSERT_EQUAL_HEX8(PROT_FAULT_OC_DSG, protection_check(lim, &sample));
}

static void test_check_over_current_charge(void)
{
    sample.current_ma = -lim->oc_chg_ma;
    TEST_ASSERT_EQUAL_HEX8(PROT_FAULT_NONE, protection_check(lim, &sample));
    sample.current_ma = -lim->oc_chg_ma - 1;
    TEST_ASSERT_EQUAL_HEX8(PROT_FAULT_OC_CHG, protection_check(lim, &sample));
}

static void test_check_over_temperature(void)
{
    sample.cell_temp_dc[3] = lim->ot_dc + 1;
    TEST_ASSERT_EQUAL_HEX8(PROT_FAULT_OT, protection_check(lim, &sample));
}

static void test_check_reports_multiple_faults(void)
{
    sample.cell_mv[0] = lim->ov_mv + 1;
    sample.cell_mv[1] = lim->uv_mv - 1;
    sample.cell_temp_dc[2] = lim->ot_dc + 1;
    TEST_ASSERT_EQUAL_HEX8(PROT_FAULT_OV | PROT_FAULT_UV | PROT_FAULT_OT,
                           protection_check(lim, &sample));
}

/* ---- state machine via the sim backed HAL ---- */

static void test_init_closes_contactor_and_is_ok(void)
{
    hal_set_contactor(false);
    protection_init(&prot, NULL);
    TEST_ASSERT_TRUE(hal_get_contactor());
    TEST_ASSERT_EQUAL(PROT_STATE_OK, prot.state);
    TEST_ASSERT_EQUAL_HEX8(PROT_FAULT_NONE, prot.faults);
}

static void test_healthy_pack_stays_ok(void)
{
    sim_pack_set_current(&pack, 3.0);
    step_n(10);
    TEST_ASSERT_EQUAL(PROT_STATE_OK, prot.state);
    TEST_ASSERT_TRUE(hal_get_contactor());
}

static void test_fault_trips_only_after_debounce(void)
{
    sim_pack_set_current(&pack, 15.0);
    step_n(lim->debounce - 1);
    TEST_ASSERT_EQUAL(PROT_STATE_OK, prot.state);
    TEST_ASSERT_TRUE(hal_get_contactor());
    protection_step(&prot);
    TEST_ASSERT_EQUAL(PROT_STATE_FAULT, prot.state);
    TEST_ASSERT_EQUAL_HEX8(PROT_FAULT_OC_DSG, prot.faults);
    TEST_ASSERT_FALSE(hal_get_contactor());
}

static void test_glitch_resets_debounce(void)
{
    sim_pack_set_current(&pack, 15.0);
    step_n(lim->debounce - 1);
    sim_pack_set_current(&pack, 1.0);
    protection_step(&prot);
    sim_pack_set_current(&pack, 15.0);
    step_n(lim->debounce - 1);
    TEST_ASSERT_EQUAL(PROT_STATE_OK, prot.state);
}

static void test_fault_latches_after_condition_clears(void)
{
    sim_pack_set_current(&pack, 15.0);
    step_n(lim->debounce);
    /* Contactor is open so measured current is now zero. */
    TEST_ASSERT_EQUAL_INT32(0, hal_read_pack_current_ma());
    step_n(10);
    TEST_ASSERT_EQUAL(PROT_STATE_FAULT, prot.state);
    TEST_ASSERT_FALSE(hal_get_contactor());
}

static void test_over_temperature_trips(void)
{
    pack.cells[1].temp_c = 65.0;
    step_n(lim->debounce);
    TEST_ASSERT_EQUAL_HEX8(PROT_FAULT_OT, prot.faults);
    TEST_ASSERT_FALSE(hal_get_contactor());
}

static void test_under_voltage_trips_near_empty(void)
{
    pack.cells[0].soc = 0.0;
    sim_pack_set_current(&pack, 3.0);
    sim_pack_step(&pack, 0.0);
    step_n(lim->debounce);
    TEST_ASSERT_EQUAL_HEX8(PROT_FAULT_UV, prot.faults);
}

static void test_clear_refused_while_condition_present(void)
{
    pack.cells[1].temp_c = 65.0;
    step_n(lim->debounce);
    TEST_ASSERT_FALSE(protection_clear(&prot));
    TEST_ASSERT_EQUAL(PROT_STATE_FAULT, prot.state);
    TEST_ASSERT_FALSE(hal_get_contactor());
}

static void test_clear_recovers_when_condition_gone(void)
{
    sim_pack_set_current(&pack, 15.0);
    step_n(lim->debounce);
    TEST_ASSERT_TRUE(protection_clear(&prot));
    TEST_ASSERT_EQUAL(PROT_STATE_OK, prot.state);
    TEST_ASSERT_EQUAL_HEX8(PROT_FAULT_NONE, prot.faults);
    TEST_ASSERT_TRUE(hal_get_contactor());
}

static void test_custom_limits(void)
{
    prot_limits_t custom = PROT_DEFAULT_LIMITS;
    custom.oc_dsg_ma = 2000;
    custom.debounce = 1;
    protection_init(&prot, &custom);
    sim_pack_set_current(&pack, 2.5);
    protection_step(&prot);
    TEST_ASSERT_EQUAL_HEX8(PROT_FAULT_OC_DSG, prot.faults);
}

static void test_step_keeps_last_sample(void)
{
    pack.cells[3].soc = 0.55;
    protection_step(&prot);
    TEST_ASSERT_EQUAL_UINT16(3780, prot.last.cell_mv[3]);
    TEST_ASSERT_EQUAL_UINT16(3740, prot.last.cell_mv[0]);
    TEST_ASSERT_EQUAL_INT16(250, prot.last.cell_temp_dc[0]);
}

static void test_zero_mv_is_sensor_fault_not_uv(void)
{
    sample.cell_mv[1] = 0;
    TEST_ASSERT_EQUAL_HEX8(PROT_FAULT_SENSOR, protection_check(lim, &sample));
}

static void test_over_range_mv_is_sensor_fault_not_ov(void)
{
    sample.cell_mv[0] = 5001;
    TEST_ASSERT_EQUAL_HEX8(PROT_FAULT_SENSOR, protection_check(lim, &sample));
    sample.cell_mv[0] = 4999;
    TEST_ASSERT_EQUAL_HEX8(PROT_FAULT_OV, protection_check(lim, &sample));
}

static void test_low_but_plausible_mv_is_uv(void)
{
    sample.cell_mv[2] = 1000;
    TEST_ASSERT_EQUAL_HEX8(PROT_FAULT_UV, protection_check(lim, &sample));
}

static void test_open_thermistor_is_sensor_fault(void)
{
    sample.cell_temp_dc[3] = -401;
    TEST_ASSERT_EQUAL_HEX8(PROT_FAULT_SENSOR, protection_check(lim, &sample));
}

static void test_very_hot_reading_is_ot_not_sensor(void)
{
    sample.cell_temp_dc[3] = 1500;
    TEST_ASSERT_EQUAL_HEX8(PROT_FAULT_OT, protection_check(lim, &sample));
}

static void test_open_sense_wire_trips_and_blocks_clear(void)
{
    hal_sim_set_sense_open(1, true);
    step_n(3);
    TEST_ASSERT_EQUAL_HEX8(PROT_FAULT_SENSOR, prot.faults);
    TEST_ASSERT_FALSE(pack.contactor_closed);
    TEST_ASSERT_FALSE(protection_clear(&prot));
    hal_sim_set_sense_open(1, false);
    TEST_ASSERT_TRUE(protection_clear(&prot));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_check_nominal_is_clear);
    RUN_TEST(test_check_over_voltage);
    RUN_TEST(test_check_under_voltage);
    RUN_TEST(test_check_over_current_discharge);
    RUN_TEST(test_check_over_current_charge);
    RUN_TEST(test_check_over_temperature);
    RUN_TEST(test_check_reports_multiple_faults);
    RUN_TEST(test_init_closes_contactor_and_is_ok);
    RUN_TEST(test_healthy_pack_stays_ok);
    RUN_TEST(test_fault_trips_only_after_debounce);
    RUN_TEST(test_glitch_resets_debounce);
    RUN_TEST(test_fault_latches_after_condition_clears);
    RUN_TEST(test_over_temperature_trips);
    RUN_TEST(test_under_voltage_trips_near_empty);
    RUN_TEST(test_clear_refused_while_condition_present);
    RUN_TEST(test_clear_recovers_when_condition_gone);
    RUN_TEST(test_custom_limits);
    RUN_TEST(test_step_keeps_last_sample);
    RUN_TEST(test_zero_mv_is_sensor_fault_not_uv);
    RUN_TEST(test_over_range_mv_is_sensor_fault_not_ov);
    RUN_TEST(test_low_but_plausible_mv_is_uv);
    RUN_TEST(test_open_thermistor_is_sensor_fault);
    RUN_TEST(test_very_hot_reading_is_ot_not_sensor);
    RUN_TEST(test_open_sense_wire_trips_and_blocks_clear);
    return UNITY_END();
}
