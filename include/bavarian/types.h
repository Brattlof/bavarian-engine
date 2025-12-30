/**
 * @file types.h
 * @brief Fundamental type definitions for Bavarian Engine
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

#ifndef BAV_TYPES_H
#define BAV_TYPES_H

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
    typedef struct BavHandle
    {
        u64 value;
    } BavHandle;

#define BAV_HANDLE_NULL ((BavHandle){0})

    static inline b8 bav_handle_valid(BavHandle h)
    {
        return h.value != 0;
    }
    static inline u32 bav_handle_index(BavHandle h)
    {
        return (u32)(h.value >> 32);
    }
    static inline u16 bav_handle_generation(BavHandle h)
    {
        return (u16)(h.value >> 16);
    }
    static inline u16 bav_handle_type(BavHandle h)
    {
        return (u16)(h.value);
    }

    static inline BavHandle bav_handle_make(u32 index, u16 generation, u16 type)
    {
        return (BavHandle){((u64)index << 32) | ((u64)generation << 16) | (u64)type};
    }

    /* =============================================================================
     * Result Type
     * ============================================================================= */

    /**
     * Standard result codes for operations that can fail.
     * Negative values indicate errors, zero is success, positive are warnings.
     */
    typedef enum BavResult
    {
        BAV_ERROR_UNKNOWN = -100,
        BAV_ERROR_OUT_OF_MEMORY = -10,
        BAV_ERROR_INVALID_ARG = -9,
        BAV_ERROR_NOT_FOUND = -8,
        BAV_ERROR_IO = -7,
        BAV_ERROR_TIMEOUT = -6,
        BAV_ERROR_OVERFLOW = -5,
        BAV_ERROR_UNSUPPORTED = -4,
        BAV_ERROR_INVALID_STATE = -3,
        BAV_ERROR_ALREADY_EXISTS = -2,
        BAV_ERROR_GENERAL = -1,

        BAV_OK = 0,

        BAV_WARN_TRUNCATED = 1,
        BAV_WARN_PARTIAL = 2,
    } BavResult;

#define BAV_FAILED(r) ((r) < 0)
#define BAV_SUCCEEDED(r) ((r) >= 0)

    /* =============================================================================
     * Alignment Helpers
     * ============================================================================= */

#if defined(_MSC_VER)
    #define BAV_ALIGN(n) __declspec(align(n))
#else
    #define BAV_ALIGN(n) __attribute__((aligned(n)))
#endif

#define BAV_ALIGN16 BAV_ALIGN(16)
#define BAV_ALIGN32 BAV_ALIGN(32)
#define BAV_ALIGN64 BAV_ALIGN(64)

    /* =============================================================================
     * Utility Macros
     * ============================================================================= */

#define BAV_ARRAY_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))
#define BAV_UNUSED(x) ((void)(x))
#define BAV_KILOBYTES(n) ((usize)(n) * 1024)
#define BAV_MEGABYTES(n) ((usize)(n) * 1024 * 1024)
#define BAV_GIGABYTES(n) ((usize)(n) * 1024 * 1024 * 1024)

#define BAV_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define BAV_MAX(a, b) (((a) > (b)) ? (a) : (b))
#define BAV_CLAMP(x, lo, hi) BAV_MIN(BAV_MAX(x, lo), hi)

    /* =============================================================================
     * Static Assertions
     * ============================================================================= */

    _Static_assert(sizeof(i8) == 1, "i8 must be 1 byte");
    _Static_assert(sizeof(i16) == 2, "i16 must be 2 bytes");
    _Static_assert(sizeof(i32) == 4, "i32 must be 4 bytes");
    _Static_assert(sizeof(i64) == 8, "i64 must be 8 bytes");
    _Static_assert(sizeof(f32) == 4, "f32 must be 4 bytes");
    _Static_assert(sizeof(f64) == 8, "f64 must be 8 bytes");
    _Static_assert(sizeof(BavHandle) == 8, "BavHandle must be 8 bytes");

#ifdef __cplusplus
}
#endif

#endif /* BAV_TYPES_H */
