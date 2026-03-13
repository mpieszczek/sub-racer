#include "subtitle.h"
#include "unity.h"
#include <string.h>
#include <stdlib.h>

void format_srt_time(double seconds, char* output);
void format_display_time(double seconds, char* output);
void format_display_time_short(double seconds, char* output);
double parse_srt_time(const char* timeStr);
double parse_display_time(const char* timeStr);

void setUp(void) {}
void tearDown(void) {}

void test_sublist_init_empty(void) {
    SubtitleList list;
    sublist_init(&list);
    
    TEST_ASSERT_EQUAL(0, list.count);
    TEST_ASSERT_EQUAL(0, list.capacity);
    TEST_ASSERT_EQUAL(-1, list.selectedIndex);
}

void test_sublist_init_items_null(void) {
    SubtitleList list;
    sublist_init(&list);
    
    TEST_ASSERT_NULL(list.items);
}

void test_sublist_add_first(void) {
    SubtitleList list;
    sublist_init(&list);
    
    sublist_add(&list, 1.0, 2.0, "Hello");
    
    TEST_ASSERT_EQUAL(1, list.count);
    TEST_ASSERT_EQUAL(8, list.capacity);
    TEST_ASSERT_EQUAL(1.0, list.items[0].startTime);
    TEST_ASSERT_EQUAL(2.0, list.items[0].endTime);
    TEST_ASSERT_EQUAL_STRING("Hello", list.items[0].text);
    
    sublist_free(&list);
}

void test_sublist_add_multiple(void) {
    SubtitleList list;
    sublist_init(&list);
    
    sublist_add(&list, 0.0, 1.0, "One");
    sublist_add(&list, 1.0, 2.0, "Two");
    sublist_add(&list, 2.0, 3.0, "Three");
    
    TEST_ASSERT_EQUAL(3, list.count);
    TEST_ASSERT_EQUAL_STRING("One", list.items[0].text);
    TEST_ASSERT_EQUAL_STRING("Two", list.items[1].text);
    TEST_ASSERT_EQUAL_STRING("Three", list.items[2].text);
    
    sublist_free(&list);
}

void test_sublist_add_grow(void) {
    SubtitleList list;
    sublist_init(&list);
    
    for (int i = 0; i < 10; i++) {
        sublist_add(&list, i, i + 1, "Text");
    }
    
    TEST_ASSERT_EQUAL(10, list.count);
    TEST_ASSERT_EQUAL(16, list.capacity);
    
    sublist_free(&list);
}

void test_sublist_add_text_copy(void) {
    SubtitleList list;
    sublist_init(&list);
    
    char text[] = "Original";
    sublist_add(&list, 0.0, 1.0, text);
    
    strcpy(text, "Modified");
    
    TEST_ASSERT_EQUAL_STRING("Original", list.items[0].text);
    
    sublist_free(&list);
}

void test_sublist_add_null_text(void) {
    SubtitleList list;
    sublist_init(&list);
    
    sublist_add(&list, 0.0, 1.0, NULL);
    
    TEST_ASSERT_EQUAL_STRING("", list.items[0].text);
    
    sublist_free(&list);
}

void test_sublist_add_times(void) {
    SubtitleList list;
    sublist_init(&list);
    
    sublist_add(&list, 100.5, 200.75, "Test");
    
    TEST_ASSERT_FLOAT_WITHIN(0.01, 100.5, list.items[0].startTime);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 200.75, list.items[0].endTime);
    
    sublist_free(&list);
}

void test_sublist_get_valid_index(void) {
    SubtitleList list;
    sublist_init(&list);
    
    sublist_add(&list, 0.0, 1.0, "First");
    sublist_add(&list, 1.0, 2.0, "Second");
    
    Subtitle* sub = sublist_get(&list, 1);
    
    TEST_ASSERT_NOT_NULL(sub);
    TEST_ASSERT_EQUAL_STRING("Second", sub->text);
    
    sublist_free(&list);
}

void test_sublist_get_negative_index(void) {
    SubtitleList list;
    sublist_init(&list);
    
    sublist_add(&list, 0.0, 1.0, "Test");
    
    Subtitle* sub = sublist_get(&list, -1);
    
    TEST_ASSERT_NULL(sub);
    
    sublist_free(&list);
}

void test_sublist_get_out_of_bounds(void) {
    SubtitleList list;
    sublist_init(&list);
    
    sublist_add(&list, 0.0, 1.0, "Test");
    
    Subtitle* sub = sublist_get(&list, 5);
    
    TEST_ASSERT_NULL(sub);
    
    sublist_free(&list);
}

void test_sublist_get_empty_list(void) {
    SubtitleList list;
    sublist_init(&list);
    
    Subtitle* sub = sublist_get(&list, 0);
    
    TEST_ASSERT_NULL(sub);
}

void test_sublist_get_at_time_exact(void) {
    SubtitleList list;
    sublist_init(&list);
    
    sublist_add(&list, 10.0, 20.0, "First");
    sublist_add(&list, 20.0, 30.0, "Second");
    
    Subtitle* sub = sublist_get_at_time(&list, 15.0);
    
    TEST_ASSERT_NOT_NULL(sub);
    TEST_ASSERT_EQUAL_STRING("First", sub->text);
    
    sublist_free(&list);
}

void test_sublist_get_at_time_middle(void) {
    SubtitleList list;
    sublist_init(&list);
    
    sublist_add(&list, 0.0, 10.0, "Test");
    
    Subtitle* sub = sublist_get_at_time(&list, 5.0);
    
    TEST_ASSERT_NOT_NULL(sub);
    TEST_ASSERT_EQUAL_STRING("Test", sub->text);
    
    sublist_free(&list);
}

void test_sublist_get_at_time_boundary_start(void) {
    SubtitleList list;
    sublist_init(&list);
    
    sublist_add(&list, 10.0, 20.0, "Test");
    
    Subtitle* sub = sublist_get_at_time(&list, 10.0);
    
    TEST_ASSERT_NOT_NULL(sub);
    TEST_ASSERT_EQUAL_STRING("Test", sub->text);
    
    sublist_free(&list);
}

void test_sublist_get_at_time_boundary_end(void) {
    SubtitleList list;
    sublist_init(&list);
    
    sublist_add(&list, 10.0, 20.0, "First");
    sublist_add(&list, 20.0, 30.0, "Second");
    
    Subtitle* sub = sublist_get_at_time(&list, 20.0);
    
    TEST_ASSERT_NOT_NULL(sub);
    TEST_ASSERT_EQUAL_STRING("First", sub->text);
    
    sublist_free(&list);
}

void test_sublist_get_at_time_not_found(void) {
    SubtitleList list;
    sublist_init(&list);
    
    sublist_add(&list, 10.0, 20.0, "Test");
    
    Subtitle* sub = sublist_get_at_time(&list, 5.0);
    
    TEST_ASSERT_NULL(sub);
    
    sublist_free(&list);
}

void test_sublist_get_at_time_empty(void) {
    SubtitleList list;
    sublist_init(&list);
    
    Subtitle* sub = sublist_get_at_time(&list, 10.0);
    
    TEST_ASSERT_NULL(sub);
}

void test_sublist_remove_first(void) {
    SubtitleList list;
    sublist_init(&list);
    
    sublist_add(&list, 0.0, 1.0, "First");
    sublist_add(&list, 1.0, 2.0, "Second");
    sublist_add(&list, 2.0, 3.0, "Third");
    
    sublist_remove(&list, 0);
    
    TEST_ASSERT_EQUAL(2, list.count);
    TEST_ASSERT_EQUAL_STRING("Second", list.items[0].text);
    TEST_ASSERT_EQUAL_STRING("Third", list.items[1].text);
    
    sublist_free(&list);
}

void test_sublist_remove_middle(void) {
    SubtitleList list;
    sublist_init(&list);
    
    sublist_add(&list, 0.0, 1.0, "First");
    sublist_add(&list, 1.0, 2.0, "Second");
    sublist_add(&list, 2.0, 3.0, "Third");
    
    sublist_remove(&list, 1);
    
    TEST_ASSERT_EQUAL(2, list.count);
    TEST_ASSERT_EQUAL_STRING("First", list.items[0].text);
    TEST_ASSERT_EQUAL_STRING("Third", list.items[1].text);
    
    sublist_free(&list);
}

void test_sublist_remove_last(void) {
    SubtitleList list;
    sublist_init(&list);
    
    sublist_add(&list, 0.0, 1.0, "First");
    sublist_add(&list, 1.0, 2.0, "Second");
    
    sublist_remove(&list, 1);
    
    TEST_ASSERT_EQUAL(1, list.count);
    TEST_ASSERT_EQUAL_STRING("First", list.items[0].text);
    
    sublist_free(&list);
}

void test_sublist_remove_out_of_bounds(void) {
    SubtitleList list;
    sublist_init(&list);
    
    sublist_add(&list, 0.0, 1.0, "First");
    
    sublist_remove(&list, 5);
    
    TEST_ASSERT_EQUAL(1, list.count);
    TEST_ASSERT_EQUAL_STRING("First", list.items[0].text);
    
    sublist_free(&list);
}

void test_sublist_remove_selected(void) {
    SubtitleList list;
    sublist_init(&list);
    list.selectedIndex = 0;
    
    sublist_add(&list, 0.0, 1.0, "First");
    sublist_add(&list, 1.0, 2.0, "Second");
    
    sublist_remove(&list, 0);
    
    TEST_ASSERT_EQUAL(-1, list.selectedIndex);
    
    sublist_free(&list);
}

void test_sublist_remove_shifts_indices(void) {
    SubtitleList list;
    sublist_init(&list);
    list.selectedIndex = 2;
    
    sublist_add(&list, 0.0, 1.0, "A");
    sublist_add(&list, 1.0, 2.0, "B");
    sublist_add(&list, 2.0, 3.0, "C");
    sublist_add(&list, 3.0, 4.0, "D");
    
    sublist_remove(&list, 0);
    
    TEST_ASSERT_EQUAL(1, list.selectedIndex);
    TEST_ASSERT_EQUAL_STRING("B", list.items[0].text);
    TEST_ASSERT_EQUAL_STRING("C", list.items[1].text);
    TEST_ASSERT_EQUAL_STRING("D", list.items[2].text);
    
    sublist_free(&list);
}

void test_sublist_clear_frees_all(void) {
    SubtitleList list;
    sublist_init(&list);
    
    sublist_add(&list, 0.0, 1.0, "Test");
    sublist_add(&list, 1.0, 2.0, "Test2");
    
    sublist_clear(&list);
    
    TEST_ASSERT_EQUAL(0, list.count);
    TEST_ASSERT_NULL(list.items);
}

void test_sublist_clear_on_empty(void) {
    SubtitleList list;
    sublist_init(&list);
    
    sublist_clear(&list);
    
    TEST_ASSERT_EQUAL(0, list.count);
    TEST_ASSERT_NULL(list.items);
}

void test_sublist_free_frees_memory(void) {
    SubtitleList list;
    sublist_init(&list);
    
    sublist_add(&list, 0.0, 1.0, "Test");
    
    sublist_free(&list);
    
    TEST_ASSERT_NULL(list.items);
    TEST_ASSERT_EQUAL(0, list.count);
    TEST_ASSERT_EQUAL(0, list.capacity);
    TEST_ASSERT_EQUAL(-1, list.selectedIndex);
}

void test_format_display_time_zero(void);
void test_format_display_time_hours(void);
void test_format_display_time_minutes(void);
void test_format_display_time_with_millis(void);
void test_format_display_time_short_zero(void);
void test_format_display_time_short_hours(void);
void test_format_display_time_short_minutes(void);
void test_parse_display_time_zero(void);
void test_parse_display_time_hours(void);
void test_parse_display_time_minutes(void);
void test_parse_display_time_with_millis(void);
void test_roundtrip_display_time(void);
void test_roundtrip_display_time_short(void);

void test_format_display_time_zero(void) {
    char output[32] = {0};
    format_display_time(0.0, output);
    TEST_ASSERT_EQUAL_STRING("00:00:00.000", output);
}

void test_format_display_time_hours(void) {
    char output[32] = {0};
    format_display_time(3661.5, output);
    TEST_ASSERT_EQUAL_STRING("01:01:01.500", output);
}

void test_format_display_time_minutes(void) {
    char output[32] = {0};
    format_display_time(125.5, output);
    TEST_ASSERT_EQUAL_STRING("00:02:05.500", output);
}

void test_format_display_time_with_millis(void) {
    char output[32] = {0};
    format_display_time(10.123, output);
    TEST_ASSERT_EQUAL_STRING("00:00:10.123", output);
}

void test_format_display_time_short_zero(void) {
    char output[32] = {0};
    format_display_time_short(0.0, output);
    TEST_ASSERT_EQUAL_STRING("00:00:00", output);
}

void test_format_display_time_short_hours(void) {
    char output[32] = {0};
    format_display_time_short(3661.0, output);
    TEST_ASSERT_EQUAL_STRING("01:01:01", output);
}

void test_format_display_time_short_minutes(void) {
    char output[32] = {0};
    format_display_time_short(125.0, output);
    TEST_ASSERT_EQUAL_STRING("00:02:05", output);
}

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
    
    RUN_TEST(test_sublist_init_empty);
    RUN_TEST(test_sublist_init_items_null);
    
    RUN_TEST(test_sublist_add_first);
    RUN_TEST(test_sublist_add_multiple);
    RUN_TEST(test_sublist_add_grow);
    RUN_TEST(test_sublist_add_text_copy);
    RUN_TEST(test_sublist_add_null_text);
    RUN_TEST(test_sublist_add_times);
    
    RUN_TEST(test_sublist_get_valid_index);
    RUN_TEST(test_sublist_get_negative_index);
    RUN_TEST(test_sublist_get_out_of_bounds);
    RUN_TEST(test_sublist_get_empty_list);
    
    RUN_TEST(test_sublist_get_at_time_exact);
    RUN_TEST(test_sublist_get_at_time_middle);
    RUN_TEST(test_sublist_get_at_time_boundary_start);
    RUN_TEST(test_sublist_get_at_time_boundary_end);
    RUN_TEST(test_sublist_get_at_time_not_found);
    RUN_TEST(test_sublist_get_at_time_empty);
    
    RUN_TEST(test_sublist_remove_first);
    RUN_TEST(test_sublist_remove_middle);
    RUN_TEST(test_sublist_remove_last);
    RUN_TEST(test_sublist_remove_out_of_bounds);
    RUN_TEST(test_sublist_remove_selected);
    RUN_TEST(test_sublist_remove_shifts_indices);
    
    RUN_TEST(test_sublist_clear_frees_all);
    RUN_TEST(test_sublist_clear_on_empty);
    
    RUN_TEST(test_sublist_free_frees_memory);
    
    RUN_TEST(test_format_display_time_zero);
    RUN_TEST(test_format_display_time_hours);
    RUN_TEST(test_format_display_time_minutes);
    RUN_TEST(test_format_display_time_with_millis);
    RUN_TEST(test_format_display_time_short_zero);
    RUN_TEST(test_format_display_time_short_hours);
    RUN_TEST(test_format_display_time_short_minutes);
    RUN_TEST(test_parse_display_time_zero);
    RUN_TEST(test_parse_display_time_hours);
    RUN_TEST(test_parse_display_time_minutes);
    RUN_TEST(test_parse_display_time_with_millis);
    RUN_TEST(test_roundtrip_display_time);
    RUN_TEST(test_roundtrip_display_time_short);
    
    return UNITY_END();
}
