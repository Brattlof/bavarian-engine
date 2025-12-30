/**
 * @file test_memory.c
 * @brief Memory module tests
 */

#include <bavarian3d/memory.h>

#include "test_framework.h"

static void test_alloc_free(void)
{
    void* ptr = mem_alloc(NULL, 1024, 16);
    ASSERT_NOT_NULL(ptr);

    /* Should be aligned */
    ASSERT_EQ(((usize)ptr % 16), 0);

    mem_free(NULL, ptr, 1024);
    TEST_PASS();
}

static void test_alloc_zero(void)
{
    u8* ptr = (u8*)mem_alloc_zero(NULL, 256, 8);
    ASSERT_NOT_NULL(ptr);

    /* All bytes should be zero */
    for (int i = 0; i < 256; i++)
    {
        ASSERT_EQ(ptr[i], 0);
    }

    mem_free(NULL, ptr, 256);
    TEST_PASS();
}

static void test_realloc(void)
{
    void* ptr = mem_alloc(NULL, 64, 16);
    ASSERT_NOT_NULL(ptr);

    ptr = mem_realloc(NULL, ptr, 64, 256, 16);
    ASSERT_NOT_NULL(ptr);
    ASSERT_EQ(((usize)ptr % 16), 0);

    mem_free(NULL, ptr, 256);
    TEST_PASS();
}

static void test_copy(void)
{
    u8 src[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    u8 dst[16] = {0};

    mem_copy(dst, src, 16);

    for (int i = 0; i < 16; i++)
    {
        ASSERT_EQ(dst[i], i);
    }
    TEST_PASS();
}

static void test_compare(void)
{
    u8 a[4] = {1, 2, 3, 4};
    u8 b[4] = {1, 2, 3, 4};
    u8 c[4] = {1, 2, 3, 5};

    ASSERT_EQ(mem_compare(a, b, 4), 0);
    ASSERT_TRUE(mem_compare(a, c, 4) < 0); /* a < c because 4 < 5 */
    TEST_PASS();
}

static void test_alignment_utils(void)
{
    ASSERT_TRUE(mem_is_power_of_two(1));
    ASSERT_TRUE(mem_is_power_of_two(16));
    ASSERT_TRUE(mem_is_power_of_two(4096));
    ASSERT_FALSE(mem_is_power_of_two(0));
    ASSERT_FALSE(mem_is_power_of_two(3));
    ASSERT_FALSE(mem_is_power_of_two(100));

    ASSERT_EQ(mem_align_up(0, 16), 0);
    ASSERT_EQ(mem_align_up(1, 16), 16);
    ASSERT_EQ(mem_align_up(15, 16), 16);
    ASSERT_EQ(mem_align_up(16, 16), 16);
    ASSERT_EQ(mem_align_up(17, 16), 32);
    TEST_PASS();
}

void test_memory_suite(void)
{
    TEST_SUITE_BEGIN("Memory Tests");

    RUN_TEST(test_alloc_free);
    RUN_TEST(test_alloc_zero);
    RUN_TEST(test_realloc);
    RUN_TEST(test_copy);
    RUN_TEST(test_compare);
    RUN_TEST(test_alignment_utils);

    TEST_SUITE_END();
}
