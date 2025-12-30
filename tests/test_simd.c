/**
 * @file test_simd.c
 * @brief SIMD implementation tests
 *
 * Tests the assembly SIMD implementations against the scalar versions.
 * If these don't match, something's wrong with the assembly.
 */

#include <bavarian3d/math.h>
#include <bavarian3d/types.h>

#include "test_framework.h"

#define EPSILON 1e-4f /* Slightly looser for SIMD approximations */

/*
 * External declarations for the SIMD functions.
 * These are defined in the assembly files.
 */
#if defined(__x86_64__) || defined(_M_X64)

extern void vec4_add_sse(Vec4* result, const Vec4* a, const Vec4* b);
extern float vec4_dot_sse(const Vec4* a, const Vec4* b);
extern void vec4_normalize_sse(Vec4* result, const Vec4* v);
extern void mat4_mul_sse(Mat4* result, const Mat4* a, const Mat4* b);
extern void mat4_mul_vec4_sse(Vec4* result, const Mat4* m, const Vec4* v);

static void test_vec4_add_sse(void)
{
    BAV3D_ALIGN16 Vec4 a = {1.0f, 2.0f, 3.0f, 4.0f};
    BAV3D_ALIGN16 Vec4 b = {5.0f, 6.0f, 7.0f, 8.0f};
    BAV3D_ALIGN16 Vec4 result;

    vec4_add_sse(&result, &a, &b);

    ASSERT_FLOAT_EQ(result.x, 6.0f, EPSILON);
    ASSERT_FLOAT_EQ(result.y, 8.0f, EPSILON);
    ASSERT_FLOAT_EQ(result.z, 10.0f, EPSILON);
    ASSERT_FLOAT_EQ(result.w, 12.0f, EPSILON);
    TEST_PASS();
}

static void test_vec4_dot_sse(void)
{
    BAV3D_ALIGN16 Vec4 a = {1.0f, 2.0f, 3.0f, 4.0f};
    BAV3D_ALIGN16 Vec4 b = {5.0f, 6.0f, 7.0f, 8.0f};

    float result = vec4_dot_sse(&a, &b);
    float expected = 1 * 5 + 2 * 6 + 3 * 7 + 4 * 8; /* = 70 */

    ASSERT_FLOAT_EQ(result, expected, EPSILON);
    TEST_PASS();
}

static void test_vec4_normalize_sse(void)
{
    BAV3D_ALIGN16 Vec4 v = {3.0f, 4.0f, 0.0f, 0.0f}; /* Length = 5 */
    BAV3D_ALIGN16 Vec4 result;

    vec4_normalize_sse(&result, &v);

    ASSERT_FLOAT_EQ(result.x, 0.6f, EPSILON);
    ASSERT_FLOAT_EQ(result.y, 0.8f, EPSILON);
    ASSERT_FLOAT_EQ(result.z, 0.0f, EPSILON);

    /* Check length is ~1 */
    float len =
        result.x * result.x + result.y * result.y + result.z * result.z + result.w * result.w;
    ASSERT_FLOAT_EQ(len, 1.0f, EPSILON);
    TEST_PASS();
}

static void test_mat4_mul_sse(void)
{
    BAV3D_ALIGN16 Mat4 a = mat4_translate(vec3(1, 2, 3));
    BAV3D_ALIGN16 Mat4 b = mat4_scale(vec3(2, 2, 2));
    BAV3D_ALIGN16 Mat4 result_sse;
    Mat4 result_scalar = mat4_mul(a, b);

    mat4_mul_sse(&result_sse, &a, &b);

    /* Compare SSE result with scalar result */
    for (int col = 0; col < 4; col++)
    {
        ASSERT_FLOAT_EQ(result_sse.cols[col].x, result_scalar.cols[col].x, EPSILON);
        ASSERT_FLOAT_EQ(result_sse.cols[col].y, result_scalar.cols[col].y, EPSILON);
        ASSERT_FLOAT_EQ(result_sse.cols[col].z, result_scalar.cols[col].z, EPSILON);
        ASSERT_FLOAT_EQ(result_sse.cols[col].w, result_scalar.cols[col].w, EPSILON);
    }
    TEST_PASS();
}

static void test_mat4_mul_vec4_sse(void)
{
    BAV3D_ALIGN16 Mat4 m = mat4_translate(vec3(10, 20, 30));
    BAV3D_ALIGN16 Vec4 v = {1.0f, 2.0f, 3.0f, 1.0f}; /* w=1 for point */
    BAV3D_ALIGN16 Vec4 result_sse;
    Vec4 result_scalar = mat4_mul_vec4(m, v);

    mat4_mul_vec4_sse(&result_sse, &m, &v);

    ASSERT_FLOAT_EQ(result_sse.x, result_scalar.x, EPSILON);
    ASSERT_FLOAT_EQ(result_sse.y, result_scalar.y, EPSILON);
    ASSERT_FLOAT_EQ(result_sse.z, result_scalar.z, EPSILON);
    ASSERT_FLOAT_EQ(result_sse.w, result_scalar.w, EPSILON);
    TEST_PASS();
}

#endif /* x86_64 */

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    printf("Bavarian3D SIMD Test Suite\n");
    printf("========================================\n");

#if defined(__x86_64__) || defined(_M_X64)
    TEST_SUITE_BEGIN("x86_64 SSE Tests");

    RUN_TEST(test_vec4_add_sse);
    RUN_TEST(test_vec4_dot_sse);
    RUN_TEST(test_vec4_normalize_sse);
    RUN_TEST(test_mat4_mul_sse);
    RUN_TEST(test_mat4_mul_vec4_sse);

    TEST_SUITE_END();
#else
    printf("SIMD tests not available on this platform\n");
#endif

    TEST_MAIN_END();
}
