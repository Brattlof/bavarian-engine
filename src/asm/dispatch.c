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
 * Public API
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

/*
 * Function pointer tables for runtime dispatch go here.
 * The idea is we have:
 *
 *   void (*vec4_add_impl)(Vec4*, Vec4*, Vec4*) = vec4_add_scalar;
 *
 * Then at startup (or on first call) we detect CPU features and point it at:
 *   - vec4_add_avx if AVX is available
 *   - vec4_add_sse if SSE is available
 *   - vec4_add_scalar otherwise
 *
 * This adds one indirection per call but it's usually worth it for the SIMD
 * speedup. For really hot inner loops you'd want to batch operations anyway.
 */
