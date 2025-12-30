/**
 * @file test_main.c
 * @brief Test runner entry point
 */

#include "test_framework.h"

/* External test functions */
extern void test_math_suite(void);
extern void test_memory_suite(void);
extern void test_arena_suite(void);

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    printf("Bavarian3D Test Suite\n");
    printf("========================================\n");

    test_math_suite();
    test_memory_suite();
    test_arena_suite();

    TEST_MAIN_END();
}
