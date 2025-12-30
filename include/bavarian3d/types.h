/**
 * @file types.h
 * @brief Fundamental type definitions for BAV3D
 *
 * Purpose:
 *   Provides portable, explicit type definitions used throughout the engine.
 *   All types are explicitly sized to prevent platform ambiguity.
 *
 * Constraints:
 *   - No dependencies on other engine headers
 *   - All types must have deterministic size and alignment
 *   - No implicit conversions that lose precision
 */

#ifndef BAV3D_TYPES_H
#define BAV3D_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* =============================================================================
     * Integer Types
     * ============================================================================= */

    typedef int8_t i8;
    typedef int16_t i16;
    typedef int32_t i32;
    typedef int64_t i64;

    typedef uint8_t u8;
    typedef uint16_t u16;
    typedef uint32_t u32;
    typedef uint64_t u64;

    typedef size_t usize;
    typedef ptrdiff_t isize;

    /* =============================================================================
     * Floating Point Types
     * ============================================================================= */

    typedef float f32;
    typedef double f64;

    /* =============================================================================
     * Boolean
     * ============================================================================= */

    typedef bool b8;
    typedef i32 b32; /* For alignment and ABI compatibility with some APIs */

    /* =============================================================================
     * Byte Type
     * ============================================================================= */

    typedef u8 byte;

    /* =============================================================================
     * Handle Types
     * ============================================================================= */

    /**
     * Generic handle type for opaque resource references.
     * Handles are 64-bit to accommodate index + generation for validation.
     *
     * Layout: [32-bit index][16-bit generation][16-bit type tag]
     */
    typedef struct Handle
    {
        u64 value;
    } Handle;

/* Compound literals work differently in C vs C++ */
#ifdef __cplusplus
#define HANDLE_NULL (Handle{0})
#else
#define HANDLE_NULL ((Handle){0})
#endif

    static inline b8 handle_valid(Handle h)
    {
        return h.value != 0;
    }
    static inline u32 handle_index(Handle h)
    {
        return (u32)(h.value >> 32);
    }
    static inline u16 handle_generation(Handle h)
    {
        return (u16)(h.value >> 16);
    }
    static inline u16 handle_type(Handle h)
    {
        return (u16)(h.value);
    }

    static inline Handle handle_make(u32 index, u16 generation, u16 type)
    {
        Handle h;
        h.value = ((u64)index << 32) | ((u64)generation << 16) | (u64)type;
        return h;
    }

    /* =============================================================================
     * Result Type
     * ============================================================================= */

    /**
     * Standard result codes for operations that can fail.
     * Negative values indicate errors, zero is success, positive are warnings.
     */
    typedef enum Result
    {
        RESULT_ERROR_UNKNOWN = -100,
        RESULT_ERROR_OUT_OF_MEMORY = -10,
        RESULT_ERROR_INVALID_ARG = -9,
        RESULT_ERROR_NOT_FOUND = -8,
        RESULT_ERROR_IO = -7,
        RESULT_ERROR_TIMEOUT = -6,
        RESULT_ERROR_OVERFLOW = -5,
        RESULT_ERROR_UNSUPPORTED = -4,
        RESULT_ERROR_INVALID_STATE = -3,
        RESULT_ERROR_ALREADY_EXISTS = -2,
        RESULT_ERROR_GENERAL = -1,

        RESULT_OK = 0,

        RESULT_WARN_TRUNCATED = 1,
        RESULT_WARN_PARTIAL = 2,
    } Result;

#define RESULT_FAILED(r) ((r) < 0)
#define RESULT_SUCCEEDED(r) ((r) >= 0)

    /* =============================================================================
     * Alignment Helpers
     * ============================================================================= */

#if defined(_MSC_VER)
    #define BAV3D_ALIGN(n) __declspec(align(n))
#else
    #define BAV3D_ALIGN(n) __attribute__((aligned(n)))
#endif

#define BAV3D_ALIGN16 BAV3D_ALIGN(16)
#define BAV3D_ALIGN32 BAV3D_ALIGN(32)
#define BAV3D_ALIGN64 BAV3D_ALIGN(64)

    /* =============================================================================
     * Utility Macros
     * ============================================================================= */

#define BAV3D_ARRAY_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))
#define BAV3D_UNUSED(x) ((void)(x))
#define BAV3D_KILOBYTES(n) ((usize)(n) * 1024)
#define BAV3D_MEGABYTES(n) ((usize)(n) * 1024 * 1024)
#define BAV3D_GIGABYTES(n) ((usize)(n) * 1024 * 1024 * 1024)

#define BAV3D_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define BAV3D_MAX(a, b) (((a) > (b)) ? (a) : (b))
#define BAV3D_CLAMP(x, lo, hi) BAV3D_MIN(BAV3D_MAX(x, lo), hi)

    /* =============================================================================
     * Static Assertions
     * ============================================================================= */

    /* C++ uses static_assert, C11 uses _Static_assert */
#ifdef __cplusplus
#define BAV3D_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#else
#define BAV3D_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#endif

    BAV3D_STATIC_ASSERT(sizeof(i8) == 1, "i8 must be 1 byte");
    BAV3D_STATIC_ASSERT(sizeof(i16) == 2, "i16 must be 2 bytes");
    BAV3D_STATIC_ASSERT(sizeof(i32) == 4, "i32 must be 4 bytes");
    BAV3D_STATIC_ASSERT(sizeof(i64) == 8, "i64 must be 8 bytes");
    BAV3D_STATIC_ASSERT(sizeof(f32) == 4, "f32 must be 4 bytes");
    BAV3D_STATIC_ASSERT(sizeof(f64) == 8, "f64 must be 8 bytes");
    BAV3D_STATIC_ASSERT(sizeof(Handle) == 8, "Handle must be 8 bytes");

#ifdef __cplusplus
}
#endif

#endif /* BAV3D_TYPES_H */
