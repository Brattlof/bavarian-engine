/**
 * @file platform.h
 * @brief Platform detection and configuration
 *
 * Purpose:
 *   Central header for platform detection macros and platform-specific configuration.
 *   Include this to determine the target platform at compile time.
 *
 * Constraints:
 *   - No dependencies on other engine headers except types.h
 *   - Must be includable from C and C++
 */

#ifndef BAV3D_PLATFORM_H
#define BAV3D_PLATFORM_H

#include <bavarian3d/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* =============================================================================
     * Platform Detection
     * ============================================================================= */

#if defined(_WIN32) || defined(_WIN64)
    #define BAV3D_PLATFORM_WINDOWS 1
    #define BAV3D_PLATFORM_NAME "Windows"
#elif defined(__APPLE__) && defined(__MACH__)
    #include <TargetConditionals.h>
    #if TARGET_OS_MAC
        #define BAV3D_PLATFORM_MACOS 1
        #define BAV3D_PLATFORM_NAME "macOS"
    #endif
#elif defined(__linux__)
    #define BAV3D_PLATFORM_LINUX 1
    #define BAV3D_PLATFORM_NAME "Linux"
#else
    #error "Unsupported platform"
#endif

    /* =============================================================================
     * Architecture Detection
     * ============================================================================= */

#if defined(__x86_64__) || defined(_M_X64)
    #define BAV3D_ARCH_X86_64 1
    #define BAV3D_ARCH_NAME "x86_64"
    #define BAV3D_CACHE_LINE_SIZE 64
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define BAV3D_ARCH_ARM64 1
    #define BAV3D_ARCH_NAME "ARM64"
    #define BAV3D_CACHE_LINE_SIZE 64
#else
    #error "Unsupported architecture"
#endif

    /* =============================================================================
     * Compiler Detection
     * ============================================================================= */

#if defined(_MSC_VER)
    #define BAV3D_COMPILER_MSVC 1
    #define BAV3D_COMPILER_NAME "MSVC"
#elif defined(__clang__)
    #define BAV3D_COMPILER_CLANG 1
    #define BAV3D_COMPILER_NAME "Clang"
#elif defined(__GNUC__)
    #define BAV3D_COMPILER_GCC 1
    #define BAV3D_COMPILER_NAME "GCC"
#else
    #define BAV3D_COMPILER_UNKNOWN 1
    #define BAV3D_COMPILER_NAME "Unknown"
#endif

    /* =============================================================================
     * Compiler Attributes
     * ============================================================================= */

#if defined(BAV3D_COMPILER_MSVC)
    #define BAV3D_INLINE __forceinline
    #define BAV3D_NOINLINE __declspec(noinline)
    #define BAV3D_NORETURN __declspec(noreturn)
    #define BAV3D_DEPRECATED __declspec(deprecated)
    #define BAV3D_LIKELY(x) (x)
    #define BAV3D_UNLIKELY(x) (x)
    #define BAV3D_RESTRICT __restrict
#else
    #define BAV3D_INLINE inline __attribute__((always_inline))
    #define BAV3D_NOINLINE __attribute__((noinline))
    #define BAV3D_NORETURN __attribute__((noreturn))
    #define BAV3D_DEPRECATED __attribute__((deprecated))
    #define BAV3D_LIKELY(x) __builtin_expect(!!(x), 1)
    #define BAV3D_UNLIKELY(x) __builtin_expect(!!(x), 0)
    #define BAV3D_RESTRICT __restrict__
#endif

    /* =============================================================================
     * Debug/Release Detection
     * ============================================================================= */

#if defined(NDEBUG)
    #define BAV3D_RELEASE 1
#else
    #define BAV3D_DEBUG 1
#endif

/* =============================================================================
 * Endianness
 * ============================================================================= */

/* x86_64 and ARM64 in standard configuration are little-endian */
#define BAV3D_LITTLE_ENDIAN 1

    /* =============================================================================
     * Platform Initialization
     * ============================================================================= */

    /**
     * Initialize the platform layer.
     * Must be called before any other platform functions.
     *
     * @return RESULT_OK on success
     */
    Result platform_init(void);

    /**
     * Shutdown the platform layer.
     * Releases all platform resources.
     */
    void platform_shutdown(void);

    /**
     * Get platform information string.
     */
    const char* platform_get_info(void);

#ifdef __cplusplus
}
#endif

#endif /* BAV3D_PLATFORM_H */
