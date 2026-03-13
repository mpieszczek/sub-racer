#include "unity.h"
#include <string.h>
#include <stdlib.h>

// Forward declarations from time_utils.h
void format_srt_time(double seconds, char* output);
void format_display_time(double seconds, char* output);
void format_display_time_short(double seconds, char* output);
double parse_srt_time(const char* timeStr);
double parse_display_time(const char* timeStr);

void setUp(void) {}
void tearDown(void) {}

// ============= format_display_time tests =============

void test_format_display_time_zero(void) {
    char output[32] = {0};
    format_display_time(0.0, output);
    TEST_ASSERT_EQUAL_STRING("00:00:00.000", output);
}

void test_format_display_time_hours(void) {
    char output[32] = {0};
    format_display_time(3661.5, output);  // 1h 1m 1s 500ms
    TEST_ASSERT_EQUAL_STRING("01:01:01.500", output);
}

void test_format_display_time_minutes(void) {
    char output[32] = {0};
    format_display_time(125.5, output);  // 2m 5s 500ms
    TEST_ASSERT_EQUAL_STRING("00:02:05.500", output);
}

void test_format_display_time_with_millis(void) {
    char output[32] = {0};
    format_display_time(10.123, output);
    TEST_ASSERT_EQUAL_STRING("00:00:10.123", output);
}

// ============= format_display_time_short tests =============

void test_format_display_time_short_zero(void) {
    char output[32] = {0};
    format_display_time_short(0.0, output);
    TEST_ASSERT_EQUAL_STRING("00:00:00", output);
}

void test_format_display_time_short_hours(void) {
    char output[32] = {0};
    format_display_time_short(3661.0, output);  // 1h 1m 1s
    TEST_ASSERT_EQUAL_STRING("01:01:01", output);
}

void test_format_display_time_short_minutes(void) {
    char output[32] = {0};
    format_display_time_short(125.0, output);  // 2m 5s
    TEST_ASSERT_EQUAL_STRING("00:02:05", output);
}

// ============= parse_display_time tests =============

void test_parse_display_time_zero(void) {
    double result = parse_display_time("00:00:00.000");
    TEST_ASSERT_FLOAT_WITHIN(0.001, 0.0, result);
}

void test_parse_display_time_hours(void) {
    double result = parse_display_time("01:01:01.500");
    TEST_ASSERT_FLOAT_WITHIN(0.001, 3661.5, result);
}

void test_parse_display_time_minutes(void) {
    double result = parse_display_time("00:02:05.500");
    TEST_ASSERT_FLOAT_WITHIN(0.001, 125.5, result);
}

void test_parse_display_time_with_millis(void) {
    double result = parse_display_time("00:00:10.123");
    TEST_ASSERT_FLOAT_WITHIN(0.001, 10.123, result);
}

// ============= Round-trip tests =============

void test_roundtrip_display_time(void) {
    char output[32] = {0};
    double input = 3661.5;
    
    format_display_time(input, output);
    double parsed = parse_display_time(output);
    
    TEST_ASSERT_FLOAT_WITHIN(0.001, input, parsed);
}

void test_roundtrip_display_time_short(void) {
    char output[32] = {0};
    double input = 3661.0;
    
    format_display_time_short(input, output);
    TEST_ASSERT_EQUAL_STRING("01:01:01", output);
}

int main(void) {
    UNITY_BEGIN();
    
    // format_display_time tests
    RUN_TEST(test_format_display_time_zero);
    RUN_TEST(test_format_display_time_hours);
    RUN_TEST(test_format_display_time_minutes);
    RUN_TEST(test_format_display_time_with_millis);
    
    // format_display_time_short tests
    RUN_TEST(test_format_display_time_short_zero);
    RUN_TEST(test_format_display_time_short_hours);
    RUN_TEST(test_format_display_time_short_minutes);
    
    // parse_display_time tests
    RUN_TEST(test_parse_display_time_zero);
    RUN_TEST(test_parse_display_time_hours);
    RUN_TEST(test_parse_display_time_minutes);
    RUN_TEST(test_parse_display_time_with_millis);
    
    // Round-trip tests
    RUN_TEST(test_roundtrip_display_time);
    RUN_TEST(test_roundtrip_display_time_short);
    
    return UNITY_END();
}
