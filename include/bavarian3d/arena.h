/**
 * @file arena.h
 * @brief Arena (linear) allocator for BAV3D
 *
 * Purpose:
 *   Provides fast, cache-friendly linear allocation with bulk free.
 *   Ideal for per-frame allocations and temporary scratch space.
 *
 * Constraints:
 *   - No individual frees (only bulk reset)
 *   - Thread-safety is caller's responsibility
 *   - Memory is not zeroed by default (use arena_alloc_zero if needed)
 */

#ifndef BAV3D_ARENA_H
#define BAV3D_ARENA_H

#include <bavarian3d/memory.h>
#include <bavarian3d/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* =============================================================================
     * Arena Type
     * ============================================================================= */

    /**
     * Linear arena allocator.
     * Allocations bump a pointer; frees are bulk-only via reset.
     */
    typedef struct Arena
    {
        byte* base;      /* Base address of arena memory */
        usize size;      /* Total arena size in bytes */
        usize offset;    /* Current allocation offset */
        usize committed; /* Currently committed memory (for virtual memory arenas) */
    } Arena;

    /**
     * Saved arena state for temporary allocations.
     * Use with arena_save/arena_restore for nested scratch usage.
     */
    typedef struct ArenaTemp
    {
        Arena* arena;
        usize offset;
    } ArenaTemp;

    /* =============================================================================
     * Arena Lifecycle
     * ============================================================================= */

    /**
     * Initialize an arena with pre-allocated memory.
     *
     * @param arena Arena to initialize
     * @param memory Pre-allocated memory block
     * @param size Size of memory block in bytes
     */
    void arena_init(Arena* arena, void* memory, usize size);

    /**
     * Create an arena backed by virtual memory.
     * Memory is reserved but only committed on demand.
     *
     * @param reserve_size Total virtual address space to reserve
     * @return Initialized arena, or {0} on failure
     */
    Arena arena_create_virtual(usize reserve_size);

    /**
     * Destroy a virtual memory arena.
     * No-op for stack-backed arenas.
     */
    void arena_destroy(Arena* arena);

    /**
     * Reset arena to empty state.
     * All prior allocations become invalid.
     */
    void arena_reset(Arena* arena);

    /* =============================================================================
     * Allocation Functions
     * ============================================================================= */

    /**
     * Allocate memory from arena.
     *
     * @param arena Arena to allocate from
     * @param size Bytes to allocate
     * @param align Alignment requirement (must be power of 2)
     * @return Pointer to allocated memory, or NULL if arena exhausted
     */
    void* arena_alloc(Arena* arena, usize size, usize align);

    /**
     * Allocate zeroed memory from arena.
     */
    void* arena_alloc_zero(Arena* arena, usize size, usize align);

    /**
     * Allocate memory and copy data into it.
     */
    void* arena_alloc_copy(Arena* arena, const void* src, usize size, usize align);

    /**
     * Duplicate a null-terminated string into arena.
     */
    char* arena_strdup(Arena* arena, const char* str);

    /**
     * Duplicate a string with explicit length.
     */
    char* arena_strndup(Arena* arena, const char* str, usize len);

    /* =============================================================================
     * Temporary Allocations
     * ============================================================================= */

    /**
     * Save current arena state.
     * Allocations after this point can be freed with arena_restore.
     */
    ArenaTemp arena_save(Arena* arena);

    /**
     * Restore arena to saved state.
     * Invalidates all allocations made after arena_save.
     */
    void arena_restore(ArenaTemp temp);

    /* =============================================================================
     * Queries
     * ============================================================================= */

    /**
     * Get remaining allocatable space in arena.
     */
    static inline usize arena_remaining(const Arena* arena)
    {
        return arena->size - arena->offset;
    }

    /**
     * Get total bytes allocated from arena.
     */
    static inline usize arena_used(const Arena* arena)
    {
        return arena->offset;
    }

    /**
     * Check if arena has enough space for an allocation.
     */
    static inline b8 arena_can_fit(const Arena* arena, usize size, usize align)
    {
        usize aligned_offset = mem_align_up(arena->offset, align);
        return aligned_offset + size <= arena->size;
    }

    /* =============================================================================
     * Type-Safe Allocation Macros
     * ============================================================================= */

#define ARENA_ALLOC_TYPE(arena, T) ((T*)arena_alloc((arena), sizeof(T), _Alignof(T)))

#define ARENA_ALLOC_ARRAY(arena, T, count)                                                         \
    ((T*)arena_alloc((arena), sizeof(T) * (count), _Alignof(T)))

#define ARENA_ALLOC_TYPE_ZERO(arena, T) ((T*)arena_alloc_zero((arena), sizeof(T), _Alignof(T)))

#define ARENA_ALLOC_ARRAY_ZERO(arena, T, count)                                                    \
    ((T*)arena_alloc_zero((arena), sizeof(T) * (count), _Alignof(T)))

#ifdef __cplusplus
}
#endif

#endif /* BAV3D_ARENA_H */
