// OTA brute-force guard and password policy (run on PC: pio test -e native)
#include <unity.h>

#include "OtaGuard.h"

using namespace gold;

void setUp() {}
void tearDown() {}

void test_lockout_after_max_failures() {
    OtaGuardPolicy p;
    p.maxFailures = 5;
    p.lockoutMs = 15UL * 60 * 1000;
    OtaGuard g(p);
    for (int i = 0; i < 4; ++i) TEST_ASSERT_FALSE(g.onAuthFailure(1000 + i));
    TEST_ASSERT_FALSE(g.locked());
    TEST_ASSERT_TRUE(g.onAuthFailure(2000));  // 5th wrong password
    TEST_ASSERT_TRUE(g.locked());
    TEST_ASSERT_FALSE(g.onAuthFailure(2100));  // already locked: no second "start"
    TEST_ASSERT_FALSE(g.shouldUnlock(2000 + 60000));
    TEST_ASSERT_EQUAL_UINT32(15UL * 60 * 1000 - 60000, g.remainingMs(2000 + 60000));
    TEST_ASSERT_TRUE(g.shouldUnlock(2000 + 15UL * 60 * 1000));
    TEST_ASSERT_FALSE(g.locked());
    TEST_ASSERT_EQUAL_UINT8(0, g.failures());
    TEST_ASSERT_FALSE(g.shouldUnlock(2000 + 15UL * 60 * 1000 + 1));  // only once
}

void test_success_resets_counter() {
    OtaGuard g;
    for (int i = 0; i < 4; ++i) g.onAuthFailure(i);
    g.onSuccess();
    TEST_ASSERT_EQUAL_UINT8(0, g.failures());
    for (int i = 0; i < 4; ++i) TEST_ASSERT_FALSE(g.onAuthFailure(100 + i));
}

void test_lockout_survives_millis_rollover() {
    OtaGuardPolicy p;
    p.maxFailures = 1;
    p.lockoutMs = 10000;
    OtaGuard g(p);
    const uint32_t nearWrap = 0xFFFFFFFFu - 2000u;
    TEST_ASSERT_TRUE(g.onAuthFailure(nearWrap));
    TEST_ASSERT_FALSE(g.shouldUnlock(nearWrap + 5000u));
    TEST_ASSERT_TRUE(g.shouldUnlock(nearWrap + 10000u));
}

void test_password_policy() {
    TEST_ASSERT_NULL(otaPasswordProblem("Test-Ota-Pass-2026"));
    TEST_ASSERT_NULL(otaPasswordProblem("abcdefghij"));
    TEST_ASSERT_NOT_NULL(otaPasswordProblem(""));
    TEST_ASSERT_NOT_NULL(otaPasswordProblem(nullptr));
    TEST_ASSERT_NOT_NULL(otaPasswordProblem("short123"));
    TEST_ASSERT_NOT_NULL(otaPasswordProblem("goldled-ota"));   // old default
    TEST_ASSERT_NOT_NULL(otaPasswordProblem("GOLDLED-OTA"));   // case-insensitive
    TEST_ASSERT_NOT_NULL(otaPasswordProblem("1234567890"));
    TEST_ASSERT_NOT_NULL(otaPasswordProblem("has space in it"));
    TEST_ASSERT_NOT_NULL(otaPasswordProblem("quote\"inside1"));
    TEST_ASSERT_NOT_NULL(otaPasswordProblem("back\\slash123"));
    TEST_ASSERT_NOT_NULL(otaPasswordProblem("รหัสผ่านภาษาไทย"));
    TEST_ASSERT_NOT_NULL(
        otaPasswordProblem("a234567890123456789012345678901234567890123456789012345678901234567"));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_lockout_after_max_failures);
    RUN_TEST(test_success_resets_counter);
    RUN_TEST(test_lockout_survives_millis_rollover);
    RUN_TEST(test_password_policy);
    return UNITY_END();
}
