/* Unit tests for CAN frame encoding, fault events and the sim CAN bus. */
#include <string.h>
#include "unity.h"
#include "can_tx.h"
#include "csv.h"
#include "hal_sim.h"
#include "scenario.h"
#include "system.h"

static can_status_t st;
static can_tx_t tx;
static hal_can_frame_t f;

void setUp(void)
{
    const uint16_t mv[HAL_NUM_CELLS] = { 4000, 3990, 4010, 3995 };
    const int16_t dc[HAL_NUM_CELLS] = { 250, 255, -12, 300 };
    memset(&st, 0, sizeof st);
    memcpy(st.cell_mv, mv, sizeof mv);
    memcpy(st.cell_temp_dc, dc, sizeof dc);
    st.current_ma = -2345;
    st.soc_cpct = 8123;
    st.ekf_cpct = 8050;
    st.faults = PROT_FAULT_NONE;
    st.balance_mask = 0x05;
    st.contactor = true;
    can_tx_init(&tx);
    hal_sim_can_clear();
}

void tearDown(void) {}

static uint16_t u16_at(const hal_can_frame_t *fr, int i)
{
    return (uint16_t)(fr->data[i] | (fr->data[i + 1] << 8));
}

/* ---- encoders ---- */

static void test_pack_frame(void)
{
    can_encode_pack(&f, &st);
    TEST_ASSERT_EQUAL_HEX16(CAN_ID_PACK, f.id);
    TEST_ASSERT_EQUAL_UINT8(8, f.dlc);
    TEST_ASSERT_EQUAL_UINT16(1600, u16_at(&f, 0));            /* 15995 mV -> 10 mV units */
    TEST_ASSERT_EQUAL_INT16(-234, (int16_t)u16_at(&f, 2));    /* 10 mA units */
    TEST_ASSERT_EQUAL_UINT16(8050, u16_at(&f, 4));
    TEST_ASSERT_EQUAL_HEX8(0x01, f.data[6]);
    TEST_ASSERT_EQUAL_HEX8(0x00, f.data[7]);
}

static void test_cell_voltage_frame_is_little_endian(void)
{
    can_encode_cell_v(&f, &st);
    TEST_ASSERT_EQUAL_HEX16(CAN_ID_CELL_V, f.id);
    TEST_ASSERT_EQUAL_HEX8(0xA0, f.data[0]);   /* 4000 = 0x0FA0 */
    TEST_ASSERT_EQUAL_HEX8(0x0F, f.data[1]);
    TEST_ASSERT_EQUAL_UINT16(4010, u16_at(&f, 4));
    TEST_ASSERT_EQUAL_UINT16(3995, u16_at(&f, 6));
}

static void test_temperature_frame_is_signed(void)
{
    can_encode_cell_t(&f, &st);
    TEST_ASSERT_EQUAL_HEX16(CAN_ID_CELL_T, f.id);
    TEST_ASSERT_EQUAL_INT16(250, (int16_t)u16_at(&f, 0));
    TEST_ASSERT_EQUAL_INT16(-12, (int16_t)u16_at(&f, 4));
}

static void test_status_frame(void)
{
    st.faults = PROT_FAULT_OT;
    can_encode_status(&f, &st, 42);
    TEST_ASSERT_EQUAL_HEX16(CAN_ID_STATUS, f.id);
    TEST_ASSERT_EQUAL_UINT16(8123, u16_at(&f, 0));
    TEST_ASSERT_EQUAL_HEX8(0x05, f.data[2]);
    TEST_ASSERT_EQUAL_HEX8(PROT_FAULT_OT, f.data[3]);
    TEST_ASSERT_EQUAL_UINT8(42, f.data[4]);
    TEST_ASSERT_EQUAL_HEX8(0, f.data[7]);
}

static void test_current_is_clamped_to_int16(void)
{
    st.current_ma = 500000;
    can_encode_pack(&f, &st);
    TEST_ASSERT_EQUAL_INT16(INT16_MAX, (int16_t)u16_at(&f, 2));
}

/* ---- transmitter ---- */

static void test_step_sends_four_periodic_frames(void)
{
    TEST_ASSERT_EQUAL_UINT8(4, can_tx_step(&tx, &st));
    TEST_ASSERT_EQUAL_UINT32(4, hal_sim_can_total());
    uint32_t ms;
    const uint16_t ids[4] = { CAN_ID_PACK, CAN_ID_CELL_V, CAN_ID_CELL_T, CAN_ID_STATUS };
    for (uint32_t i = 0; i < 4; i++) {
        TEST_ASSERT_TRUE(hal_sim_can_get(i, &f, &ms));
        TEST_ASSERT_EQUAL_HEX16(ids[i], f.id);
    }
}

static void test_alive_counter_increments_and_wraps(void)
{
    uint32_t ms;
    for (int i = 0; i < 257; i++) {
        can_tx_step(&tx, &st);
    }
    TEST_ASSERT_TRUE(hal_sim_can_get(hal_sim_can_total() - 1, &f, &ms));
    TEST_ASSERT_EQUAL_UINT8(0, f.data[4]);   /* 257th frame: 256 wraps to 0 */
}

static void test_fault_event_sent_once_per_new_fault(void)
{
    uint32_t ms;
    st.faults = PROT_FAULT_UV;
    TEST_ASSERT_EQUAL_UINT8(5, can_tx_step(&tx, &st));
    TEST_ASSERT_TRUE(hal_sim_can_get(0, &f, &ms));
    TEST_ASSERT_EQUAL_HEX16(CAN_ID_FAULT_EVENT, f.id);
    TEST_ASSERT_EQUAL_UINT8(2, f.dlc);
    TEST_ASSERT_EQUAL_HEX8(PROT_FAULT_UV, f.data[0]);
    TEST_ASSERT_EQUAL_UINT8(4, can_tx_step(&tx, &st));   /* no repeat */
    st.faults = PROT_FAULT_UV | PROT_FAULT_OT;
    can_tx_step(&tx, &st);
    TEST_ASSERT_TRUE(hal_sim_can_get(9, &f, &ms));   /* after 5 + 4 frames */
    TEST_ASSERT_EQUAL_HEX8(PROT_FAULT_OT, f.data[0]);    /* only the new bit */
    TEST_ASSERT_EQUAL_HEX8(PROT_FAULT_UV | PROT_FAULT_OT, f.data[1]);
}

static void test_fault_reannounced_after_clear(void)
{
    st.faults = PROT_FAULT_OC_DSG;
    can_tx_step(&tx, &st);
    st.faults = PROT_FAULT_NONE;
    can_tx_step(&tx, &st);
    st.faults = PROT_FAULT_OC_DSG;
    TEST_ASSERT_EQUAL_UINT8(5, can_tx_step(&tx, &st));
}

/* ---- simulated bus ---- */

static void test_bus_rejects_bad_frames(void)
{
    can_encode_pack(&f, &st);
    f.dlc = 9;
    TEST_ASSERT_FALSE(hal_can_send(&f));
    f.dlc = 8;
    f.id = 0x800;
    TEST_ASSERT_FALSE(hal_can_send(&f));
    TEST_ASSERT_FALSE(hal_can_send(NULL));
    TEST_ASSERT_EQUAL_UINT32(0, hal_sim_can_total());
}

static void test_bus_ring_drops_oldest(void)
{
    uint32_t ms;
    can_encode_pack(&f, &st);
    for (uint32_t i = 0; i < HAL_SIM_CAN_RING + 10; i++) {
        hal_sim_can_set_time_ms(i);
        hal_can_send(&f);
    }
    TEST_ASSERT_FALSE(hal_sim_can_get(9, &f, &ms));
    TEST_ASSERT_TRUE(hal_sim_can_get(10, &f, &ms));
    TEST_ASSERT_EQUAL_UINT32(10, ms);
    TEST_ASSERT_FALSE(hal_sim_can_get(HAL_SIM_CAN_RING + 10, &f, &ms));
}

/* ---- system integration ---- */

static void test_system_sends_four_frames_per_second(void)
{
    bms_sys_t sys;
    uint32_t ms;
    sys_init(&sys, 0.5);
    TEST_ASSERT_EQUAL_UINT32(0, hal_sim_can_total());
    for (int i = 0; i < 10; i++) {
        sys_step(&sys);
    }
    TEST_ASSERT_EQUAL_UINT32(40, hal_sim_can_total());
    TEST_ASSERT_TRUE(hal_sim_can_get(39, &f, &ms));
    TEST_ASSERT_EQUAL_UINT32(9000, ms);
    TEST_ASSERT_TRUE(hal_sim_can_get(36, &f, &ms));
    TEST_ASSERT_EQUAL_HEX16(CAN_ID_PACK, f.id);
    TEST_ASSERT_UINT16_WITHIN(5, (uint16_t)(sim_pack_voltage(&sys.pack) * 100.0), u16_at(&f, 0));
}

static void test_system_reports_trip_on_the_bus(void)
{
    bms_sys_t sys;
    uint32_t ms;
    sys_init(&sys, 0.5);
    sim_pack_set_current(&sys.pack, 15.0);
    for (int i = 0; i < 5; i++) {
        sys_step(&sys);
    }
    int events = 0;
    for (uint32_t s = 0; s < hal_sim_can_total(); s++) {
        hal_sim_can_get(s, &f, &ms);
        if (f.id == CAN_ID_FAULT_EVENT) {
            events++;
            TEST_ASSERT_EQUAL_HEX8(PROT_FAULT_OC_DSG, f.data[0]);
        }
    }
    TEST_ASSERT_EQUAL_INT(1, events);
}

static void test_candump_output(void)
{
    FILE *out = tmpfile();
    TEST_ASSERT_NOT_NULL(out);
    int n = csv_run_can(out, scenario_find("overcurrent"));
    TEST_ASSERT_EQUAL_INT(600 * 4 + 1, n);   /* plus one fault event */
    rewind(out);
    char line[128];
    TEST_ASSERT_NOT_NULL(fgets(line, sizeof line, out));
    TEST_ASSERT_EQUAL_INT(0, strncmp(line, "(000000.000000) can0 100#", 25));
    TEST_ASSERT_EQUAL_INT(25 + 16 + 1, (int)strlen(line));
    int found = 0;
    while (fgets(line, sizeof line, out) != NULL) {
        found |= strstr(line, " can0 080#0404") != NULL;
    }
    fclose(out);
    TEST_ASSERT_TRUE(found);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_pack_frame);
    RUN_TEST(test_cell_voltage_frame_is_little_endian);
    RUN_TEST(test_temperature_frame_is_signed);
    RUN_TEST(test_status_frame);
    RUN_TEST(test_current_is_clamped_to_int16);
    RUN_TEST(test_step_sends_four_periodic_frames);
    RUN_TEST(test_alive_counter_increments_and_wraps);
    RUN_TEST(test_fault_event_sent_once_per_new_fault);
    RUN_TEST(test_fault_reannounced_after_clear);
    RUN_TEST(test_bus_rejects_bad_frames);
    RUN_TEST(test_bus_ring_drops_oldest);
    RUN_TEST(test_system_sends_four_frames_per_second);
    RUN_TEST(test_system_reports_trip_on_the_bus);
    RUN_TEST(test_candump_output);
    return UNITY_END();
}
