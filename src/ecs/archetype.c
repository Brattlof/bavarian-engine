/**
 * @file archetype.c
 * @brief Archetype management and storage
 *
 * Archetypes group entities with identical component sets for
 * cache-efficient iteration. This is where the real performance comes from.
 */

#include <bavarian/ecs.h>
#include <stdlib.h>
#include <string.h>

/* =============================================================================
 * Archetype Creation
 * ============================================================================= */

BavArchetype* bav_archetype_create(u64 component_mask, u32 initial_capacity)
{
    BavArchetype* arch = calloc(1, sizeof(BavArchetype));
    if (!arch)
        return NULL;

    arch->component_mask = component_mask;
    arch->entity_capacity = initial_capacity;

    /* Count components in mask */
    u32 count = 0;
    for (u32 i = 0; i < 64; i++)
    {
        if (component_mask & (1ULL << i))
            count++;
    }
    arch->component_count = count;

    arch->entities = calloc(initial_capacity, sizeof(BavEntity));
    arch->component_arrays = calloc(count, sizeof(void*));
    arch->component_ids = calloc(count, sizeof(BavComponentId));

    if (!arch->entities || !arch->component_arrays || !arch->component_ids)
    {
        free(arch->entities);
        free(arch->component_arrays);
        free(arch->component_ids);
        free(arch);
        return NULL;
    }

    /* Fill in component IDs (sorted order) */
    u32 idx = 0;
    for (u32 i = 0; i < 64 && idx < count; i++)
    {
        if (component_mask & (1ULL << i))
        {
            arch->component_ids[idx++] = i;
        }
    }

    return arch;
}

void bav_archetype_destroy(BavArchetype* arch)
{
    if (!arch)
        return;

    for (u32 i = 0; i < arch->component_count; i++)
    {
        free(arch->component_arrays[i]);
    }
    free(arch->component_arrays);
    free(arch->component_ids);
    free(arch->entities);
    free(arch);
}

/* =============================================================================
 * Archetype Operations
 * ============================================================================= */

static b8 archetype_grow(BavArchetype* arch, const usize* component_sizes)
{
    u32 new_cap = arch->entity_capacity * 2;
    if (new_cap == 0)
        new_cap = 16;

    BavEntity* new_entities = realloc(arch->entities, new_cap * sizeof(BavEntity));
    if (!new_entities)
        return 0;
    arch->entities = new_entities;

    for (u32 i = 0; i < arch->component_count; i++)
    {
        void* new_arr = realloc(arch->component_arrays[i], new_cap * component_sizes[i]);
        if (!new_arr)
            return 0;
        arch->component_arrays[i] = new_arr;
    }

    arch->entity_capacity = new_cap;
    return 1;
}

u32 bav_archetype_add_entity(BavArchetype* arch, BavEntity entity, const usize* component_sizes)
{
    if (!arch)
        return (u32)-1;

    if (arch->entity_count >= arch->entity_capacity)
    {
        if (!archetype_grow(arch, component_sizes))
        {
            return (u32)-1;
        }
    }

    u32 row = arch->entity_count++;
    arch->entities[row] = entity;
    return row;
}

void bav_archetype_remove_entity(BavArchetype* arch, u32 row, const usize* component_sizes)
{
    if (!arch || row >= arch->entity_count)
        return;

    u32 last = arch->entity_count - 1;
    if (row != last)
    {
        /* Swap with last entity */
        arch->entities[row] = arch->entities[last];
        for (u32 i = 0; i < arch->component_count; i++)
        {
            char* arr = arch->component_arrays[i];
            usize size = component_sizes[i];
            memcpy(arr + row * size, arr + last * size, size);
        }
    }

    arch->entity_count--;
}
