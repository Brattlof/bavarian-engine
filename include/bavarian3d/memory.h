/**
 * @file memory.h
 * @brief Memory management primitives for BAV3D
 *
 * Purpose:
 *   Provides explicit, deterministic memory allocation interfaces.
 *   All allocations are trackable and have well-defined lifetimes.
 *
 * Constraints:
 *   - No hidden allocations
 *   - All allocations must specify alignment
 *   - Memory operations must be explicit (no RAII in C interface)
 */

#ifndef BAV3D_MEMORY_H
#define BAV3D_MEMORY_H

#include <bavarian3d/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* =============================================================================
     * Allocator Interface
     * ============================================================================= */

    /**
     * Function pointer types for custom allocators.
     * All allocators must support alignment specification.
     */
    typedef void* (*AllocFn)(void* ctx, usize size, usize align);
    typedef void* (*ReallocFn)(void* ctx, void* ptr, usize old_size, usize new_size, usize align);
    typedef void (*FreeFn)(void* ctx, void* ptr, usize size);

    /**
     * Generic allocator interface.
     * Allows dependency injection of allocation strategies.
     */
    typedef struct Allocator
    {
        void* ctx;
        AllocFn alloc;
        ReallocFn realloc;
        FreeFn free;
    } Allocator;

    /* =============================================================================
     * System Allocator
     * ============================================================================= */

    /**
     * Get the default system allocator (wraps platform malloc/free).
     * Thread-safe.
     */
    Allocator* mem_system_allocator(void);

    /* =============================================================================
     * Allocation Functions
     * ============================================================================= */

    /**
     * Allocate memory with specified alignment.
     *
     * @param allocator Allocator to use (NULL for system allocator)
     * @param size      Bytes to allocate
     * @param align     Alignment requirement (must be power of 2)
     * @return Pointer to allocated memory, or NULL on failure
     */
    void* mem_alloc(Allocator* allocator, usize size, usize align);

    /**
     * Allocate zeroed memory.
     *
     * @param allocator Allocator to use (NULL for system allocator)
     * @param size      Bytes to allocate
     * @param align     Alignment requirement (must be power of 2)
     * @return Pointer to zeroed memory, or NULL on failure
     */
    void* mem_alloc_zero(Allocator* allocator, usize size, usize align);

    /**
     * Reallocate memory.
     *
     * @param allocator Allocator to use (NULL for system allocator)
     * @param ptr       Existing allocation (NULL to allocate new)
     * @param old_size  Size of existing allocation (0 if ptr is NULL)
     * @param new_size  Desired new size
     * @param align     Alignment requirement
     * @return Pointer to reallocated memory, or NULL on failure
     */
    void* mem_realloc(Allocator* allocator, void* ptr, usize old_size, usize new_size, usize align);

    /**
     * Free memory.
     *
     * @param allocator Allocator to use (NULL for system allocator)
     * @param ptr       Pointer to free (NULL is safe)
     * @param size      Size of allocation (for tracking/debugging)
     */
    void mem_free(Allocator* allocator, void* ptr, usize size);

    /* =============================================================================
     * Memory Operations
     * ============================================================================= */

    /**
     * Copy memory. Source and destination must not overlap.
     */
    void mem_copy(void* dst, const void* src, usize size);

    /**
     * Move memory. Source and destination may overlap.
     */
    void mem_move(void* dst, const void* src, usize size);

    /**
     * Set memory to a byte value.
     */
    void mem_set(void* dst, u8 value, usize size);

    /**
     * Zero memory.
     */
    void mem_zero(void* dst, usize size);

    /**
     * Compare memory.
     * @return 0 if equal, negative if a < b, positive if a > b
     */
    i32 mem_compare(const void* a, const void* b, usize size);

    /* =============================================================================
     * Alignment Utilities
     * ============================================================================= */

    /**
     * Check if a value is a power of 2.
     */
    static inline b8 mem_is_power_of_two(usize x)
    {
        return x && !(x & (x - 1));
    }

    /**
     * Align a size up to the nearest alignment boundary.
     */
    static inline usize mem_align_up(usize size, usize align)
    {
        return (size + align - 1) & ~(align - 1);
    }

    /**
     * Align a pointer up to the nearest alignment boundary.
     */
    static inline void* mem_align_ptr(void* ptr, usize align)
    {
        return (void*)mem_align_up((usize)ptr, align);
    }

    /* =============================================================================
     * Type-Safe Allocation Macros
     * ============================================================================= */

#define MEM_ALLOC_TYPE(allocator, T) ((T*)mem_alloc((allocator), sizeof(T), _Alignof(T)))

#define MEM_ALLOC_ARRAY(allocator, T, count)                                                       \
    ((T*)mem_alloc((allocator), sizeof(T) * (count), _Alignof(T)))

#define MEM_ALLOC_TYPE_ZERO(allocator, T) ((T*)mem_alloc_zero((allocator), sizeof(T), _Alignof(T)))

#define MEM_ALLOC_ARRAY_ZERO(allocator, T, count)                                                  \
    ((T*)mem_alloc_zero((allocator), sizeof(T) * (count), _Alignof(T)))

#define MEM_FREE_TYPE(allocator, ptr, T) mem_free((allocator), (ptr), sizeof(T))

#define MEM_FREE_ARRAY(allocator, ptr, T, count) mem_free((allocator), (ptr), sizeof(T) * (count))

#ifdef __cplusplus
}
#endif

#endif /* BAV3D_MEMORY_H */
