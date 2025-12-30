/**
 * @file arena.c
 * @brief Arena allocator implementation
 *
 * Arenas are the workhorse of per-frame allocation. The idea is dead simple:
 * bump a pointer, never free individually, reset the whole thing at once.
 * Cache-friendly, fast as hell, and you can't leak because there's nothing
 * to leak. The only gotcha is you need to know your usage patterns.
 */

#include <bavarian3d/arena.h>
#include <bavarian3d/memory.h>

#include <string.h>

/* =============================================================================
 * Platform Virtual Memory
 *
 * Virtual memory lets us reserve a huge address range but only commit pages
 * as we actually use them. This is perfect for arenas where we don't know
 * the max size upfront but don't want to reallocate and copy.
 * ============================================================================= */

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>

static void* vm_reserve(usize size)
{
    return VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_NOACCESS);
}

static b8 vm_commit(void* addr, usize size)
{
    return VirtualAlloc(addr, size, MEM_COMMIT, PAGE_READWRITE) != NULL;
}

static void vm_release(void* addr, usize size)
{
    (void)size;
    VirtualFree(addr, 0, MEM_RELEASE);
}
#else
    #include <sys/mman.h>

static void* vm_reserve(usize size)
{
    void* ptr = mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return ptr == MAP_FAILED ? NULL : ptr;
}

static b8 vm_commit(void* addr, usize size)
{
    return mprotect(addr, size, PROT_READ | PROT_WRITE) == 0;
}

static void vm_release(void* addr, usize size)
{
    munmap(addr, size);
}
#endif

/* =============================================================================
 * Arena Lifecycle
 * ============================================================================= */

void arena_init(Arena* arena, void* memory, usize size)
{
    arena->base = (byte*)memory;
    arena->size = size;
    arena->offset = 0;
    arena->committed = size; /* Pre-allocated memory is fully committed */
}

Arena arena_create_virtual(usize reserve_size)
{
    Arena arena = {0};

    void* base = vm_reserve(reserve_size);
    if (base == NULL)
    {
        return arena;
    }

    arena.base = (byte*)base;
    arena.size = reserve_size;
    arena.offset = 0;
    arena.committed = 0;

    return arena;
}

void arena_destroy(Arena* arena)
{
    if (arena->base && arena->committed != arena->size)
    {
        /* Virtual memory arena */
        vm_release(arena->base, arena->size);
    }
    arena->base = NULL;
    arena->size = 0;
    arena->offset = 0;
    arena->committed = 0;
}

void arena_reset(Arena* arena)
{
    arena->offset = 0;
}

/* =============================================================================
 * Allocation Functions
 * ============================================================================= */

void* arena_alloc(Arena* arena, usize size, usize align)
{
    usize aligned_offset = mem_align_up(arena->offset, align);
    usize new_offset = aligned_offset + size;

    if (new_offset > arena->size)
    {
        return NULL; /* Arena exhausted */
    }

    /* Commit more memory if needed (for virtual memory arenas) */
    if (new_offset > arena->committed)
    {
        usize commit_size = mem_align_up(new_offset - arena->committed, 4096);
        if (!vm_commit(arena->base + arena->committed, commit_size))
        {
            return NULL; /* Commit failed */
        }
        arena->committed += commit_size;
    }

    void* ptr = arena->base + aligned_offset;
    arena->offset = new_offset;

    return ptr;
}

void* arena_alloc_zero(Arena* arena, usize size, usize align)
{
    void* ptr = arena_alloc(arena, size, align);
    if (ptr)
    {
        mem_zero(ptr, size);
    }
    return ptr;
}

void* arena_alloc_copy(Arena* arena, const void* src, usize size, usize align)
{
    void* ptr = arena_alloc(arena, size, align);
    if (ptr)
    {
        mem_copy(ptr, src, size);
    }
    return ptr;
}

char* arena_strdup(Arena* arena, const char* str)
{
    usize len = strlen(str);
    return arena_strndup(arena, str, len);
}

char* arena_strndup(Arena* arena, const char* str, usize len)
{
    char* dup = (char*)arena_alloc(arena, len + 1, 1);
    if (dup)
    {
        mem_copy(dup, str, len);
        dup[len] = '\0';
    }
    return dup;
}

/* =============================================================================
 * Temporary Allocations
 * ============================================================================= */

ArenaTemp arena_save(Arena* arena)
{
    ArenaTemp temp;
    temp.arena = arena;
    temp.offset = arena->offset;
    return temp;
}

void arena_restore(ArenaTemp temp)
{
    temp.arena->offset = temp.offset;
}
