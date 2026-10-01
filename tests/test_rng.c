#include "unity.h"
#include "rng.h"

static sim_rng_t rng;

void setUp(void)
{
    sim_rng_seed(&rng, 42);
}

void tearDown(void) {}

static void test_same_seed_same_sequence(void)
{
    sim_rng_t other;
    sim_rng_seed(&other, 42);
    for (int i = 0; i < 100; i++) {
        TEST_ASSERT_EQUAL_UINT32(sim_rng_next(&other), sim_rng_next(&rng));
    }
}

static void test_different_seeds_differ(void)
{
    sim_rng_t other;
    sim_rng_seed(&other, 43);
    TEST_ASSERT_TRUE(sim_rng_next(&other) != sim_rng_next(&rng));
}

static void test_zero_seed_is_usable(void)
{
    sim_rng_seed(&rng, 0);
    TEST_ASSERT_TRUE(sim_rng_next(&rng) != 0);
}

static void test_uniform_range_and_mean(void)
{
    double sum = 0.0;
    for (int i = 0; i < 20000; i++) {
        double u = sim_rng_uniform(&rng);
        TEST_ASSERT_TRUE(u >= 0.0 && u < 1.0);
        sum += u;
    }
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 0.5, sum / 20000);
}

static void test_gauss_mean_and_sigma(void)
{
    double sum = 0.0, sq = 0.0;
    const int n = 20000;
    for (int i = 0; i < n; i++) {
        double g = sim_rng_gauss(&rng);
        TEST_ASSERT_TRUE(g >= -6.0 && g <= 6.0);
        sum += g;
        sq += g * g;
    }
    double mean = sum / n;
    TEST_ASSERT_DOUBLE_WITHIN(0.03, 0.0, mean);
    TEST_ASSERT_DOUBLE_WITHIN(0.03, 1.0, sq / n - mean * mean);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_same_seed_same_sequence);
    RUN_TEST(test_different_seeds_differ);
    RUN_TEST(test_zero_seed_is_usable);
    RUN_TEST(test_uniform_range_and_mean);
    RUN_TEST(test_gauss_mean_and_sigma);
    return UNITY_END();
}
