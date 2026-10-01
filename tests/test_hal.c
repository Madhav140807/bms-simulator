#include "unity.h"
#include "hal.h"
#include "hal_sim.h"

static sim_pack_t pack;

void setUp(void)
{
    sim_pack_init(&pack, 3.0, 0.5, 0.02);
    hal_sim_attach(&pack);
}

void tearDown(void)
{
    hal_sim_attach(NULL);
}

static void test_cell_voltage_in_millivolts(void)
{
    /* OCV at 50% SOC is 3.74 V. */
    for (uint8_t i = 0; i < HAL_NUM_CELLS; i++) {
        TEST_ASSERT_EQUAL_UINT16(3740, hal_read_cell_mv(i));
    }
}

static void test_cell_voltage_rounds_to_nearest_mv(void)
{
    pack.cells[1].soc = 0.55;  /* 3.78 V at rest */
    sim_pack_set_current(&pack, 0.0123);
    sim_pack_step(&pack, 0.0);  /* sag 0.246 mV */
    TEST_ASSERT_EQUAL_UINT16(3780, hal_read_cell_mv(1));
}

static void test_cell_voltage_reflects_load_sag(void)
{
    sim_pack_set_current(&pack, 10.0);
    sim_pack_step(&pack, 0.0);  /* 10 A * 20 mOhm = 200 mV */
    TEST_ASSERT_EQUAL_UINT16(3540, hal_read_cell_mv(0));
}

static void test_current_in_milliamps_signed(void)
{
    sim_pack_set_current(&pack, 2.5);
    TEST_ASSERT_EQUAL_INT32(2500, hal_read_pack_current_ma());
    sim_pack_set_current(&pack, -1.25);
    TEST_ASSERT_EQUAL_INT32(-1250, hal_read_pack_current_ma());
}

static void test_temperature_in_tenths_of_degree(void)
{
    pack.cells[3].temp_c = 31.26;
    TEST_ASSERT_EQUAL_INT16(250, hal_read_cell_temp_dc(0));
    TEST_ASSERT_EQUAL_INT16(313, hal_read_cell_temp_dc(3));
    pack.cells[2].temp_c = -5.04;
    TEST_ASSERT_EQUAL_INT16(-50, hal_read_cell_temp_dc(2));
}

static void test_contactor_control(void)
{
    sim_pack_set_current(&pack, 3.0);
    hal_set_contactor(false);
    TEST_ASSERT_FALSE(hal_get_contactor());
    TEST_ASSERT_EQUAL_INT32(0, hal_read_pack_current_ma());
    hal_set_contactor(true);
    TEST_ASSERT_TRUE(hal_get_contactor());
    TEST_ASSERT_EQUAL_INT32(3000, hal_read_pack_current_ma());
}

static void test_balance_control(void)
{
    hal_set_balance(1, true);
    TEST_ASSERT_TRUE(hal_get_balance(1));
    TEST_ASSERT_FALSE(hal_get_balance(0));
    TEST_ASSERT_TRUE(pack.balance_on[1]);
    hal_set_balance(1, false);
    TEST_ASSERT_FALSE(hal_get_balance(1));
}

static void test_out_of_range_cell_is_safe(void)
{
    TEST_ASSERT_EQUAL_UINT16(0, hal_read_cell_mv(HAL_NUM_CELLS));
    TEST_ASSERT_EQUAL_INT16(0, hal_read_cell_temp_dc(HAL_NUM_CELLS));
    hal_set_balance(HAL_NUM_CELLS, true);
    TEST_ASSERT_FALSE(hal_get_balance(HAL_NUM_CELLS));
}

static void test_detached_hal_is_safe(void)
{
    sim_pack_set_current(&pack, 3.0);
    hal_sim_attach(NULL);
    TEST_ASSERT_EQUAL_UINT16(0, hal_read_cell_mv(0));
    TEST_ASSERT_EQUAL_INT32(0, hal_read_pack_current_ma());
    TEST_ASSERT_EQUAL_INT16(0, hal_read_cell_temp_dc(0));
    hal_set_contactor(false);
    TEST_ASSERT_FALSE(hal_get_contactor());
    TEST_ASSERT_TRUE(pack.contactor_closed);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_cell_voltage_in_millivolts);
    RUN_TEST(test_cell_voltage_rounds_to_nearest_mv);
    RUN_TEST(test_cell_voltage_reflects_load_sag);
    RUN_TEST(test_current_in_milliamps_signed);
    RUN_TEST(test_temperature_in_tenths_of_degree);
    RUN_TEST(test_contactor_control);
    RUN_TEST(test_balance_control);
    RUN_TEST(test_out_of_range_cell_is_safe);
    RUN_TEST(test_detached_hal_is_safe);
    return UNITY_END();
}
