/**
 * @file ecs_iterate_fast.c
 * @brief Optimized ECS iteration with prefetching and specialization
 *
 * This file provides optimized archetype iteration for the ECS.
 * Key optimizations:
 * - Specialized loops for 1-4 component queries (most common cases)
 * - Pointer advancement instead of per-entity multiplications
 * - Software prefetching to hide memory latency
 * - Cache line aware iteration
 */

#include "ecs_internal.h"
#include <bavarian3d/platform.h>

/* Prefetch distance tuned for L1 cache - prefetch 4 cache lines ahead */
#define PREFETCH_DISTANCE 16

/* =============================================================================
 * Platform-specific prefetch intrinsics
 * ============================================================================= */

#if defined(BAV3D_ARCH_X86_64)

    #if defined(_MSC_VER)
        #include <intrin.h>
        #define PREFETCH_READ(addr) _mm_prefetch((const char*)(addr), _MM_HINT_T0)
    #else
        #include <xmmintrin.h>
        #define PREFETCH_READ(addr) _mm_prefetch((const char*)(addr), _MM_HINT_T0)
    #endif

#elif defined(BAV3D_ARCH_ARM64)

    #if defined(__ARM_NEON) || defined(__aarch64__)
        #define PREFETCH_READ(addr) __builtin_prefetch((addr), 0, 3)
    #else
        #define PREFETCH_READ(addr) ((void)(addr))
    #endif

#else
    /* Fallback - no prefetch */
    #define PREFETCH_READ(addr) ((void)(addr))
#endif

/* =============================================================================
 * Specialized Iteration for 2-Component Queries
 * ============================================================================= */

/**
 * Fast iteration specialized for 2-component queries (most common case).
 * Avoids inner loops and uses pointer advancement.
 */
static void iterate_2comp(BavArchetype* arch, u32 idx0, u32 idx1, usize size0, usize size1,
                          BavQueryCallback callback, void* user_data)
{
    const u32 count = arch->entity_count;
    if (count == 0)
        return;

    char* ptr0 = (char*)arch->component_arrays[idx0];
    char* ptr1 = (char*)arch->component_arrays[idx1];
    BavEntity* entities = arch->entities;

    void* components[2];
    u32 e = 0;

    /* Main loop with prefetching */
    for (; e + PREFETCH_DISTANCE < count; e++)
    {
        /* Prefetch ahead */
        PREFETCH_READ(ptr0 + PREFETCH_DISTANCE * size0);
        PREFETCH_READ(ptr1 + PREFETCH_DISTANCE * size1);

        components[0] = ptr0;
        components[1] = ptr1;
        callback(entities[e], components, user_data);

        ptr0 += size0;
        ptr1 += size1;
    }

    /* Remaining entities */
    for (; e < count; e++)
    {
        components[0] = ptr0;
        components[1] = ptr1;
        callback(entities[e], components, user_data);

        ptr0 += size0;
        ptr1 += size1;
    }
}

/* =============================================================================
 * Specialized Iteration for 1-Component Queries
 * ============================================================================= */

static void iterate_1comp(BavArchetype* arch, u32 idx0, usize size0, BavQueryCallback callback,
                          void* user_data)
{
    const u32 count = arch->entity_count;
    if (count == 0)
        return;

    char* ptr0 = (char*)arch->component_arrays[idx0];
    BavEntity* entities = arch->entities;

    void* components[1];
    u32 e = 0;

    for (; e + PREFETCH_DISTANCE < count; e++)
    {
        PREFETCH_READ(ptr0 + PREFETCH_DISTANCE * size0);

        components[0] = ptr0;
        callback(entities[e], components, user_data);
        ptr0 += size0;
    }

    for (; e < count; e++)
    {
        components[0] = ptr0;
        callback(entities[e], components, user_data);
        ptr0 += size0;
    }
}

/* =============================================================================
 * Specialized Iteration for 3-Component Queries
 * ============================================================================= */

static void iterate_3comp(BavArchetype* arch, u32 idx0, u32 idx1, u32 idx2, usize size0, usize size1,
                          usize size2, BavQueryCallback callback, void* user_data)
{
    const u32 count = arch->entity_count;
    if (count == 0)
        return;

    char* ptr0 = (char*)arch->component_arrays[idx0];
    char* ptr1 = (char*)arch->component_arrays[idx1];
    char* ptr2 = (char*)arch->component_arrays[idx2];
    BavEntity* entities = arch->entities;

    void* components[3];
    u32 e = 0;

    for (; e + PREFETCH_DISTANCE < count; e++)
    {
        PREFETCH_READ(ptr0 + PREFETCH_DISTANCE * size0);
        PREFETCH_READ(ptr1 + PREFETCH_DISTANCE * size1);
        PREFETCH_READ(ptr2 + PREFETCH_DISTANCE * size2);

        components[0] = ptr0;
        components[1] = ptr1;
        components[2] = ptr2;
        callback(entities[e], components, user_data);

        ptr0 += size0;
        ptr1 += size1;
        ptr2 += size2;
    }

    for (; e < count; e++)
    {
        components[0] = ptr0;
        components[1] = ptr1;
        components[2] = ptr2;
        callback(entities[e], components, user_data);

        ptr0 += size0;
        ptr1 += size1;
        ptr2 += size2;
    }
}

/* =============================================================================
 * Generic Iteration (4+ components)
 * ============================================================================= */

void ecs_iterate_archetype_fast(BavArchetype* arch, const u32* arch_indices,
                                const usize* component_sizes, u32 query_comp_count,
                                BavQueryCallback callback, void* user_data)
{
    const u32 count = arch->entity_count;
    if (count == 0)
        return;

    /* Set up pointers for all components */
    char* ptrs[BAV_MAX_COMPONENTS];
    for (u32 c = 0; c < query_comp_count; c++)
    {
        ptrs[c] = (char*)arch->component_arrays[arch_indices[c]];
    }

    BavEntity* entities = arch->entities;
    void* components[BAV_MAX_COMPONENTS];
    u32 e = 0;

    /* Main loop with prefetching */
    for (; e + PREFETCH_DISTANCE < count; e++)
    {
        /* Prefetch ahead for all components */
        for (u32 c = 0; c < query_comp_count; c++)
        {
            PREFETCH_READ(ptrs[c] + PREFETCH_DISTANCE * component_sizes[c]);
            components[c] = ptrs[c];
            ptrs[c] += component_sizes[c];
        }

        callback(entities[e], components, user_data);
    }

    /* Remaining */
    for (; e < count; e++)
    {
        for (u32 c = 0; c < query_comp_count; c++)
        {
            components[c] = ptrs[c];
            ptrs[c] += component_sizes[c];
        }

        callback(entities[e], components, user_data);
    }
}

/* =============================================================================
 * Fast Query Entry Point
 * ============================================================================= */

void bav_query_each_fast(BavEntityAdmin* admin, const BavQuery* query, BavQueryCallback callback,
                         void* user_data)
{
    if (!admin || !query || !callback)
        return;

    for (u32 a = 0; a < admin->archetype_count; a++)
    {
        BavArchetype* arch = admin->archetypes[a];
        if (!arch || arch->entity_count == 0)
            continue;

        /* Check if archetype matches query */
        if ((arch->component_mask & query->required_mask) != query->required_mask)
            continue;
        if (arch->component_mask & query->excluded_mask)
            continue;

        /* Build component index map */
        u32 arch_indices[BAV_MAX_COMPONENTS];
        usize component_sizes[BAV_MAX_COMPONENTS];
        u32 query_comp_count = 0;

        for (u32 c = 0; c < 64; c++)
        {
            if (query->required_mask & (1ULL << c))
            {
                for (u32 i = 0; i < arch->component_count; i++)
                {
                    if (arch->component_ids[i] == c)
                    {
                        arch_indices[query_comp_count] = i;
                        component_sizes[query_comp_count] = bav_component_get_info(admin, c)->size;
                        query_comp_count++;
                        break;
                    }
                }
            }
        }

        /* Dispatch to specialized or generic iteration */
        switch (query_comp_count)
        {
            case 1:
                iterate_1comp(arch, arch_indices[0], component_sizes[0], callback, user_data);
                break;
            case 2:
                iterate_2comp(arch, arch_indices[0], arch_indices[1], component_sizes[0],
                              component_sizes[1], callback, user_data);
                break;
            case 3:
                iterate_3comp(arch, arch_indices[0], arch_indices[1], arch_indices[2],
                              component_sizes[0], component_sizes[1], component_sizes[2], callback,
                              user_data);
                break;
            default:
                ecs_iterate_archetype_fast(arch, arch_indices, component_sizes, query_comp_count,
                                           callback, user_data);
                break;
        }
    }
}

/**
 * Batch iteration with prefetching - processes entire component arrays.
 */
void bav_query_batch_fast(BavEntityAdmin* admin, const BavQuery* query,
                          BavQueryBatchCallback callback, void* user_data)
{
    if (!admin || !query || !callback)
        return;

    for (u32 a = 0; a < admin->archetype_count; a++)
    {
        BavArchetype* arch = admin->archetypes[a];
        if (!arch || arch->entity_count == 0)
            continue;

        if ((arch->component_mask & query->required_mask) != query->required_mask)
            continue;
        if (arch->component_mask & query->excluded_mask)
            continue;

        /* Prefetch all component arrays for this archetype */
        for (u32 c = 0; c < arch->component_count; c++)
        {
            PREFETCH_READ(arch->component_arrays[c]);
        }

        callback(arch->entity_count, arch->component_arrays, user_data);
    }
}
