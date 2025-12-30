/**
 * @file memory.c
 * @brief Memory management implementation
 */

#include <bavarian3d/memory.h>

#include <stdlib.h>
#include <string.h>

/* =============================================================================
 * Platform-Specific Aligned Allocation
 *
 * Windows just had to be different. Microsoft's aligned_alloc equivalent
 * is _aligned_malloc but the parameter order is swapped because of course
 * it is. Also you need _aligned_free instead of regular free or you'll
 * corrupt the heap. Ask me how I know.
 * ============================================================================= */

#if defined(_WIN32)
    #include <malloc.h>
    #define aligned_alloc(align, size) _aligned_malloc((size), (align))
    #define aligned_free(ptr) _aligned_free(ptr)
#else
    #define aligned_free(ptr) free(ptr)
#endif

/* =============================================================================
 * System Allocator Implementation
 * ============================================================================= */

static void* system_alloc(void* ctx, usize size, usize align)
{
    (void)ctx;
    if (size == 0)
        return NULL;
    if (align < sizeof(void*))
        align = sizeof(void*);
    return aligned_alloc(align, size);
}

static void* system_realloc(void* ctx, void* ptr, usize old_size, usize new_size, usize align)
{
    (void)ctx;
    (void)old_size;

    if (new_size == 0)
    {
        aligned_free(ptr);
        return NULL;
    }

    if (ptr == NULL)
    {
        return system_alloc(ctx, new_size, align);
    }

    /*
     * This sucks but standard realloc doesn't guarantee alignment preservation.
     * So we have to alloc new, copy, free old. The performance hit is real but
     * correctness wins. If this shows up in profiles we can add a fast path for
     * common alignments where realloc is known to work (spoiler: it won't help).
     */
    void* new_ptr = system_alloc(ctx, new_size, align);
    if (new_ptr && ptr)
    {
        usize copy_size = old_size < new_size ? old_size : new_size;
        memcpy(new_ptr, ptr, copy_size);
        aligned_free(ptr);
    }
    return new_ptr;
}

static void system_free(void* ctx, void* ptr, usize size)
{
    (void)ctx;
    (void)size;
    aligned_free(ptr);
}

static Allocator g_system_allocator = {
    .ctx = NULL,
    .alloc = system_alloc,
    .realloc = system_realloc,
    .free = system_free,
};

Allocator* mem_system_allocator(void)
{
    return &g_system_allocator;
}

/* =============================================================================
 * Allocation Functions
 * ============================================================================= */

void* mem_alloc(Allocator* allocator, usize size, usize align)
{
    if (allocator == NULL)
        allocator = &g_system_allocator;
    return allocator->alloc(allocator->ctx, size, align);
}

void* mem_alloc_zero(Allocator* allocator, usize size, usize align)
{
    void* ptr = mem_alloc(allocator, size, align);
    if (ptr)
    {
        memset(ptr, 0, size);
    }
    return ptr;
}

void* mem_realloc(Allocator* allocator, void* ptr, usize old_size, usize new_size, usize align)
{
    if (allocator == NULL)
        allocator = &g_system_allocator;
    return allocator->realloc(allocator->ctx, ptr, old_size, new_size, align);
}

void mem_free(Allocator* allocator, void* ptr, usize size)
{
    if (ptr == NULL)
        return;
    if (allocator == NULL)
        allocator = &g_system_allocator;
    allocator->free(allocator->ctx, ptr, size);
}

/* =============================================================================
 * Memory Operations
 * ============================================================================= */

void mem_copy(void* dst, const void* src, usize size)
{
    memcpy(dst, src, size);
}

void mem_move(void* dst, const void* src, usize size)
{
    memmove(dst, src, size);
}

void mem_set(void* dst, u8 value, usize size)
{
    memset(dst, value, size);
}

void mem_zero(void* dst, usize size)
{
    memset(dst, 0, size);
}

i32 mem_compare(const void* a, const void* b, usize size)
{
    return memcmp(a, b, (size_t)size);
}
