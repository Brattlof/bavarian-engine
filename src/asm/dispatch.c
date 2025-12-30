/**
 * @file dispatch.c
 * @brief CPU feature detection and function dispatch
 *
 * This file detects CPU capabilities at runtime and sets up function pointers
 * to the fastest available implementation. The idea is that higher-level code
 * just calls vec3_add() or whatever, and we route it to SSE/AVX/NEON based
 * on what the CPU actually supports.
 *
 * We cache the detection results because CPUID is slow and we might be
 * calling these functions millions of times per frame.
 */

#include <bavarian3d/math.h>
#include <bavarian3d/platform.h>
#include <bavarian3d/types.h>

/* =============================================================================
 * CPU Feature Detection
 * ============================================================================= */

typedef struct CpuFeatures
{
    b8 sse;
    b8 sse2;
    b8 sse3;
    b8 ssse3;
    b8 sse41;
    b8 sse42;
    b8 avx;
    b8 avx2;
    b8 avx512f;
    b8 fma;
    b8 neon; /* ARM only */
} CpuFeatures;

static CpuFeatures g_cpu_features = {0};
static b8 g_features_detected = false;

#if defined(BAV3D_ARCH_X86_64)

    #if defined(_MSC_VER)
        #include <intrin.h>
static void cpuid(int info[4], int function_id)
{
    __cpuid(info, function_id);
}
static void cpuidex(int info[4], int function_id, int subfunction_id)
{
    __cpuidex(info, function_id, subfunction_id);
}
    #else
        #include <cpuid.h>
static void cpuid(int info[4], int function_id)
{
    __cpuid(function_id, info[0], info[1], info[2], info[3]);
}
static void cpuidex(int info[4], int function_id, int subfunction_id)
{
    __cpuid_count(function_id, subfunction_id, info[0], info[1], info[2], info[3]);
}
    #endif

static void detect_x86_features(void)
{
    int info[4];

    cpuid(info, 0);
    int max_function = info[0];

    if (max_function >= 1)
    {
        cpuid(info, 1);
        g_cpu_features.sse = (info[3] >> 25) & 1;
        g_cpu_features.sse2 = (info[3] >> 26) & 1;
        g_cpu_features.sse3 = (info[2] >> 0) & 1;
        g_cpu_features.ssse3 = (info[2] >> 9) & 1;
        g_cpu_features.sse41 = (info[2] >> 19) & 1;
        g_cpu_features.sse42 = (info[2] >> 20) & 1;
        g_cpu_features.avx = (info[2] >> 28) & 1;
        g_cpu_features.fma = (info[2] >> 12) & 1;
    }

    if (max_function >= 7)
    {
        cpuidex(info, 7, 0);
        g_cpu_features.avx2 = (info[1] >> 5) & 1;
        g_cpu_features.avx512f = (info[1] >> 16) & 1;
    }
}

#elif defined(BAV3D_ARCH_ARM64)

static void detect_arm_features(void)
{
    /* ARM64 always has NEON, it's part of the base spec */
    g_cpu_features.neon = true;
}

#endif

static void detect_cpu_features(void)
{
    if (g_features_detected)
        return;

#if defined(BAV3D_ARCH_X86_64)
    detect_x86_features();
#elif defined(BAV3D_ARCH_ARM64)
    detect_arm_features();
#endif

    g_features_detected = true;
}

/* =============================================================================
 * Public Feature Query API
 * ============================================================================= */

b8 cpu_has_sse(void)
{
    detect_cpu_features();
    return g_cpu_features.sse;
}
b8 cpu_has_sse2(void)
{
    detect_cpu_features();
    return g_cpu_features.sse2;
}
b8 cpu_has_sse41(void)
{
    detect_cpu_features();
    return g_cpu_features.sse41;
}
b8 cpu_has_avx(void)
{
    detect_cpu_features();
    return g_cpu_features.avx;
}
b8 cpu_has_avx2(void)
{
    detect_cpu_features();
    return g_cpu_features.avx2;
}
b8 cpu_has_avx512(void)
{
    detect_cpu_features();
    return g_cpu_features.avx512f;
}
b8 cpu_has_fma(void)
{
    detect_cpu_features();
    return g_cpu_features.fma;
}
b8 cpu_has_neon(void)
{
    detect_cpu_features();
    return g_cpu_features.neon;
}

/* =============================================================================
 * External ASM Function Declarations
 * ============================================================================= */

#if defined(BAV3D_ARCH_X86_64)

/* SSE implementations - defined in math_sse.S */
extern void vec4_add_sse(Vec4* result, const Vec4* a, const Vec4* b);
extern void vec4_sub_sse(Vec4* result, const Vec4* a, const Vec4* b);
extern void vec4_mul_sse(Vec4* result, const Vec4* a, const Vec4* b);
extern void vec4_scale_sse(Vec4* result, const Vec4* v, f32 s);
extern f32 vec4_dot_sse(const Vec4* a, const Vec4* b);
extern f32 vec4_length_sse(const Vec4* v);
extern void vec4_normalize_sse(Vec4* result, const Vec4* v);

extern void mat4_mul_sse(Mat4* result, const Mat4* a, const Mat4* b);
extern void mat4_mul_vec4_sse(Vec4* result, const Mat4* m, const Vec4* v);
extern void mat4_transpose_sse(Mat4* result, const Mat4* m);

/* AVX implementations - defined in math_avx.S */
/* TODO: Add AVX versions when needed */

#elif defined(BAV3D_ARCH_ARM64)

/* NEON implementations - defined in math_neon.S */
/* TODO: Add NEON function declarations */

#endif

/* =============================================================================
 * Optimized Math Wrappers
 *
 * These functions select the best implementation at runtime. The first call
 * triggers CPU feature detection, then we dispatch to the optimal path.
 *
 * For truly hot inner loops, consider calling the _sse/_avx functions directly
 * to avoid the dispatch overhead. But for typical use cases, the branch
 * predictor handles this well.
 * ============================================================================= */

#if defined(BAV3D_ARCH_X86_64)

void vec4_add_fast(Vec4* result, const Vec4* a, const Vec4* b)
{
    if (cpu_has_sse2())
    {
        vec4_add_sse(result, a, b);
    }
    else
    {
        *result = vec4_add(*a, *b);
    }
}

void vec4_sub_fast(Vec4* result, const Vec4* a, const Vec4* b)
{
    if (cpu_has_sse2())
    {
        vec4_sub_sse(result, a, b);
    }
    else
    {
        *result = vec4_sub(*a, *b);
    }
}

void vec4_mul_fast(Vec4* result, const Vec4* a, const Vec4* b)
{
    if (cpu_has_sse2())
    {
        vec4_mul_sse(result, a, b);
    }
    else
    {
        *result = vec4_mul(*a, *b);
    }
}

f32 vec4_dot_fast(const Vec4* a, const Vec4* b)
{
    if (cpu_has_sse2())
    {
        return vec4_dot_sse(a, b);
    }
    else
    {
        return vec4_dot(*a, *b);
    }
}

f32 vec4_length_fast(const Vec4* v)
{
    if (cpu_has_sse2())
    {
        return vec4_length_sse(v);
    }
    else
    {
        return vec4_length(*v);
    }
}

void vec4_normalize_fast(Vec4* result, const Vec4* v)
{
    if (cpu_has_sse2())
    {
        vec4_normalize_sse(result, v);
    }
    else
    {
        *result = vec4_normalize(*v);
    }
}

void mat4_mul_fast(Mat4* result, const Mat4* a, const Mat4* b)
{
    if (cpu_has_sse2())
    {
        mat4_mul_sse(result, a, b);
    }
    else
    {
        *result = mat4_mul(*a, *b);
    }
}

void mat4_mul_vec4_fast(Vec4* result, const Mat4* m, const Vec4* v)
{
    if (cpu_has_sse2())
    {
        mat4_mul_vec4_sse(result, m, v);
    }
    else
    {
        *result = mat4_mul_vec4(*m, *v);
    }
}

void mat4_transpose_fast(Mat4* result, const Mat4* m)
{
    if (cpu_has_sse2())
    {
        mat4_transpose_sse(result, m);
    }
    else
    {
        *result = mat4_transpose(*m);
    }
}

#else

/*
 * Fallback for non-x86 platforms. On ARM64 we'd use NEON, but for now
 * just use the scalar versions. Still fast enough for most use cases.
 */

void vec4_add_fast(Vec4* result, const Vec4* a, const Vec4* b)
{
    *result = vec4_add(*a, *b);
}

void vec4_sub_fast(Vec4* result, const Vec4* a, const Vec4* b)
{
    *result = vec4_sub(*a, *b);
}

void vec4_mul_fast(Vec4* result, const Vec4* a, const Vec4* b)
{
    *result = vec4_mul(*a, *b);
}

f32 vec4_dot_fast(const Vec4* a, const Vec4* b)
{
    return vec4_dot(*a, *b);
}

f32 vec4_length_fast(const Vec4* v)
{
    return vec4_length(*v);
}

void vec4_normalize_fast(Vec4* result, const Vec4* v)
{
    *result = vec4_normalize(*v);
}

void mat4_mul_fast(Mat4* result, const Mat4* a, const Mat4* b)
{
    *result = mat4_mul(*a, *b);
}

void mat4_mul_vec4_fast(Vec4* result, const Mat4* m, const Vec4* v)
{
    *result = mat4_mul_vec4(*m, *v);
}

void mat4_transpose_fast(Mat4* result, const Mat4* m)
{
    *result = mat4_transpose(*m);
}

#endif

/* =============================================================================
 * Initialization
 *
 * Call this early in engine startup to pre-warm the CPU detection.
 * Not strictly necessary since the first math call will trigger detection,
 * but it avoids any first-call latency in performance-sensitive code.
 * ============================================================================= */

void simd_init(void)
{
    detect_cpu_features();
}
