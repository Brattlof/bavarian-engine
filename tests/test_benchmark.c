/**
 * @file test_benchmark.c
 * @brief Performance benchmarks for SIMD vs scalar math
 *
 * These benchmarks verify that SIMD implementations are actually faster
 * than scalar versions. If they're not, something's wrong.
 */

#include <bavarian3d/math.h>
#include <bavarian3d/platform.h>
#include <bavarian3d/types.h>

#include <stdio.h>

/* External SIMD dispatch functions */
extern void vec4_add_fast(Vec4* result, const Vec4* a, const Vec4* b);
extern void vec4_mul_fast(Vec4* result, const Vec4* a, const Vec4* b);
extern f32 vec4_dot_fast(const Vec4* a, const Vec4* b);
extern void vec4_normalize_fast(Vec4* result, const Vec4* v);
extern void mat4_mul_fast(Mat4* result, const Mat4* a, const Mat4* b);
extern void mat4_mul_vec4_fast(Vec4* result, const Mat4* m, const Vec4* v);
extern void simd_init(void);

/* Platform timing */
extern u64 time_get_ticks(void);
extern u64 time_get_frequency(void);

#define ITERATIONS 1000000
#define WARMUP_ITERATIONS 10000

/* Prevent compiler from optimizing away the computation */
static volatile f32 g_sink;
static volatile Vec4 g_sink_v;
static volatile Mat4 g_sink_m;

static void sink_f32(f32 x)
{
    g_sink = x;
}
static void sink_vec4(Vec4 v)
{
    g_sink_v = v;
}
static void sink_mat4(Mat4 m)
{
    g_sink_m = m;
}

static f64 ticks_to_ms(u64 ticks)
{
    return (f64)ticks / (f64)time_get_frequency() * 1000.0;
}

/* =============================================================================
 * Vec4 Benchmarks
 * ============================================================================= */

static void bench_vec4_add(void)
{
    BAV3D_ALIGN16 Vec4 a = {1.0f, 2.0f, 3.0f, 4.0f};
    BAV3D_ALIGN16 Vec4 b = {5.0f, 6.0f, 7.0f, 8.0f};
    BAV3D_ALIGN16 Vec4 result;

    /* Warmup */
    for (u32 i = 0; i < WARMUP_ITERATIONS; i++)
    {
        result = vec4_add(a, b);
        a.x += 0.001f;
    }
    sink_vec4(result);

    /* Benchmark scalar */
    u64 start = time_get_ticks();
    for (u32 i = 0; i < ITERATIONS; i++)
    {
        result = vec4_add(a, b);
        a.x = result.x; /* Data dependency to prevent optimization */
    }
    u64 scalar_time = time_get_ticks() - start;
    sink_vec4(result);

    /* Reset */
    a = (Vec4){1.0f, 2.0f, 3.0f, 4.0f};

    /* Warmup SIMD */
    for (u32 i = 0; i < WARMUP_ITERATIONS; i++)
    {
        vec4_add_fast(&result, &a, &b);
        a.x += 0.001f;
    }
    sink_vec4(result);

    /* Benchmark SIMD */
    start = time_get_ticks();
    for (u32 i = 0; i < ITERATIONS; i++)
    {
        vec4_add_fast(&result, &a, &b);
        a.x = result.x;
    }
    u64 simd_time = time_get_ticks() - start;
    sink_vec4(result);

    f64 scalar_ms = ticks_to_ms(scalar_time);
    f64 simd_ms = ticks_to_ms(simd_time);
    f64 speedup = scalar_ms / simd_ms;

    printf("  vec4_add:       scalar=%.2fms  simd=%.2fms  speedup=%.2fx %s\n", scalar_ms, simd_ms,
           speedup, speedup > 1.0 ? "[OK]" : "[SLOWER!]");
}

static void bench_vec4_dot(void)
{
    BAV3D_ALIGN16 Vec4 a = {1.0f, 2.0f, 3.0f, 4.0f};
    BAV3D_ALIGN16 Vec4 b = {5.0f, 6.0f, 7.0f, 8.0f};
    f32 result;

    /* Warmup */
    for (u32 i = 0; i < WARMUP_ITERATIONS; i++)
    {
        result = vec4_dot(a, b);
        a.x += result * 0.00001f;
    }
    sink_f32(result);

    /* Benchmark scalar */
    u64 start = time_get_ticks();
    for (u32 i = 0; i < ITERATIONS; i++)
    {
        result = vec4_dot(a, b);
        a.x = result * 0.00001f + 1.0f;
    }
    u64 scalar_time = time_get_ticks() - start;
    sink_f32(result);

    /* Reset */
    a = (Vec4){1.0f, 2.0f, 3.0f, 4.0f};

    /* Warmup SIMD */
    for (u32 i = 0; i < WARMUP_ITERATIONS; i++)
    {
        result = vec4_dot_fast(&a, &b);
        a.x += result * 0.00001f;
    }
    sink_f32(result);

    /* Benchmark SIMD */
    start = time_get_ticks();
    for (u32 i = 0; i < ITERATIONS; i++)
    {
        result = vec4_dot_fast(&a, &b);
        a.x = result * 0.00001f + 1.0f;
    }
    u64 simd_time = time_get_ticks() - start;
    sink_f32(result);

    f64 scalar_ms = ticks_to_ms(scalar_time);
    f64 simd_ms = ticks_to_ms(simd_time);
    f64 speedup = scalar_ms / simd_ms;

    printf("  vec4_dot:       scalar=%.2fms  simd=%.2fms  speedup=%.2fx %s\n", scalar_ms, simd_ms,
           speedup, speedup > 1.0 ? "[OK]" : "[SLOWER!]");
}

static void bench_vec4_normalize(void)
{
    BAV3D_ALIGN16 Vec4 v = {3.0f, 4.0f, 5.0f, 6.0f};
    BAV3D_ALIGN16 Vec4 result;

    /* Warmup */
    for (u32 i = 0; i < WARMUP_ITERATIONS; i++)
    {
        result = vec4_normalize(v);
        v.x += result.x * 0.001f;
    }
    sink_vec4(result);

    /* Benchmark scalar */
    v = (Vec4){3.0f, 4.0f, 5.0f, 6.0f};
    u64 start = time_get_ticks();
    for (u32 i = 0; i < ITERATIONS; i++)
    {
        result = vec4_normalize(v);
        v.x = result.x * 0.001f + 3.0f;
    }
    u64 scalar_time = time_get_ticks() - start;
    sink_vec4(result);

    /* Reset */
    v = (Vec4){3.0f, 4.0f, 5.0f, 6.0f};

    /* Warmup SIMD */
    for (u32 i = 0; i < WARMUP_ITERATIONS; i++)
    {
        vec4_normalize_fast(&result, &v);
        v.x += result.x * 0.001f;
    }
    sink_vec4(result);

    /* Benchmark SIMD */
    v = (Vec4){3.0f, 4.0f, 5.0f, 6.0f};
    start = time_get_ticks();
    for (u32 i = 0; i < ITERATIONS; i++)
    {
        vec4_normalize_fast(&result, &v);
        v.x = result.x * 0.001f + 3.0f;
    }
    u64 simd_time = time_get_ticks() - start;
    sink_vec4(result);

    f64 scalar_ms = ticks_to_ms(scalar_time);
    f64 simd_ms = ticks_to_ms(simd_time);
    f64 speedup = scalar_ms / simd_ms;

    printf("  vec4_normalize: scalar=%.2fms  simd=%.2fms  speedup=%.2fx %s\n", scalar_ms, simd_ms,
           speedup, speedup > 1.0 ? "[OK]" : "[SLOWER!]");
}

/* =============================================================================
 * Mat4 Benchmarks
 * ============================================================================= */

static void bench_mat4_mul(void)
{
    BAV3D_ALIGN16 Mat4 a = mat4_rotate_x(0.5f);
    BAV3D_ALIGN16 Mat4 b = mat4_rotate_y(0.3f);
    BAV3D_ALIGN16 Mat4 result;

    /* Warmup */
    for (u32 i = 0; i < WARMUP_ITERATIONS; i++)
    {
        result = mat4_mul(a, b);
        a.cols[0].x += result.cols[0].x * 0.00001f;
    }
    sink_mat4(result);

    /* Benchmark scalar */
    a = mat4_rotate_x(0.5f);
    u64 start = time_get_ticks();
    for (u32 i = 0; i < ITERATIONS; i++)
    {
        result = mat4_mul(a, b);
        a.cols[0].x = result.cols[0].x * 0.00001f + 1.0f;
    }
    u64 scalar_time = time_get_ticks() - start;
    sink_mat4(result);

    /* Reset */
    a = mat4_rotate_x(0.5f);

    /* Warmup SIMD */
    for (u32 i = 0; i < WARMUP_ITERATIONS; i++)
    {
        mat4_mul_fast(&result, &a, &b);
        a.cols[0].x += result.cols[0].x * 0.00001f;
    }
    sink_mat4(result);

    /* Benchmark SIMD */
    a = mat4_rotate_x(0.5f);
    start = time_get_ticks();
    for (u32 i = 0; i < ITERATIONS; i++)
    {
        mat4_mul_fast(&result, &a, &b);
        a.cols[0].x = result.cols[0].x * 0.00001f + 1.0f;
    }
    u64 simd_time = time_get_ticks() - start;
    sink_mat4(result);

    f64 scalar_ms = ticks_to_ms(scalar_time);
    f64 simd_ms = ticks_to_ms(simd_time);
    f64 speedup = scalar_ms / simd_ms;

    printf("  mat4_mul:       scalar=%.2fms  simd=%.2fms  speedup=%.2fx %s\n", scalar_ms, simd_ms,
           speedup, speedup > 1.0 ? "[OK]" : "[SLOWER!]");
}

static void bench_mat4_mul_vec4(void)
{
    BAV3D_ALIGN16 Mat4 m = mat4_perspective(1.0f, 1.777f, 0.1f, 100.0f);
    BAV3D_ALIGN16 Vec4 v = {1.0f, 2.0f, 3.0f, 1.0f};
    BAV3D_ALIGN16 Vec4 result;

    /* Warmup */
    for (u32 i = 0; i < WARMUP_ITERATIONS; i++)
    {
        result = mat4_mul_vec4(m, v);
        v.x += result.x * 0.00001f;
    }
    sink_vec4(result);

    /* Benchmark scalar */
    v = (Vec4){1.0f, 2.0f, 3.0f, 1.0f};
    u64 start = time_get_ticks();
    for (u32 i = 0; i < ITERATIONS; i++)
    {
        result = mat4_mul_vec4(m, v);
        v.x = result.x * 0.00001f + 1.0f;
    }
    u64 scalar_time = time_get_ticks() - start;
    sink_vec4(result);

    /* Reset */
    v = (Vec4){1.0f, 2.0f, 3.0f, 1.0f};

    /* Warmup SIMD */
    for (u32 i = 0; i < WARMUP_ITERATIONS; i++)
    {
        mat4_mul_vec4_fast(&result, &m, &v);
        v.x += result.x * 0.00001f;
    }
    sink_vec4(result);

    /* Benchmark SIMD */
    v = (Vec4){1.0f, 2.0f, 3.0f, 1.0f};
    start = time_get_ticks();
    for (u32 i = 0; i < ITERATIONS; i++)
    {
        mat4_mul_vec4_fast(&result, &m, &v);
        v.x = result.x * 0.00001f + 1.0f;
    }
    u64 simd_time = time_get_ticks() - start;
    sink_vec4(result);

    f64 scalar_ms = ticks_to_ms(scalar_time);
    f64 simd_ms = ticks_to_ms(simd_time);
    f64 speedup = scalar_ms / simd_ms;

    printf("  mat4_mul_vec4:  scalar=%.2fms  simd=%.2fms  speedup=%.2fx %s\n", scalar_ms, simd_ms,
           speedup, speedup > 1.0 ? "[OK]" : "[SLOWER!]");
}

/* =============================================================================
 * Main
 * ============================================================================= */

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    simd_init();

    printf("Bavarian3D SIMD Benchmark\n");
    printf("=========================\n");
    printf("Iterations: %d\n\n", ITERATIONS);

    printf("Vec4 Operations:\n");
    bench_vec4_add();
    bench_vec4_dot();
    bench_vec4_normalize();

    printf("\nMat4 Operations:\n");
    bench_mat4_mul();
    bench_mat4_mul_vec4();

    printf("\nBenchmark complete.\n");
    return 0;
}
