/**
 * @file admin.c
 * @brief Entity Admin - Central ECS Manager
 *
 * This is the heart of the ECS. All entity, component, and archetype
 * management flows through here.
 */

#include <bavarian/ecs.h>
#include <bavarian/types.h>
#include <stdlib.h>
#include <string.h>

/* =============================================================================
 * Internal Structures
 * ============================================================================= */

#define INITIAL_ENTITY_CAPACITY 1024
#define INITIAL_ARCHETYPE_CAPACITY 64
#define MAX_DEFERRED_COMMANDS 4096

typedef enum CommandType
{
    CMD_CREATE_ENTITY,
    CMD_DESTROY_ENTITY,
    CMD_ADD_COMPONENT,
    CMD_REMOVE_COMPONENT,
} CommandType;

typedef struct DeferredCommand
{
    CommandType type;
    BavEntity entity;
    BavComponentId component;
    void* data;
    usize data_size;
} DeferredCommand;

typedef struct EntityRecord
{
    BavArchetype* archetype;
    u32 row; /* Index within archetype */
    u16 generation;
    b8 alive;
} EntityRecord;

struct BavEntityAdmin
{
    /* Component registry */
    BavComponentInfo components[BAV_MAX_COMPONENTS];
    u32 component_count;

    /* Entity storage */
    EntityRecord* entities;
    u32 entity_count;
    u32 entity_capacity;
    u32* free_list;
    u32 free_count;

    /* Archetype storage */
    BavArchetype** archetypes;
    u32 archetype_count;
    u32 archetype_capacity;

    /* Deferred command buffer */
    DeferredCommand commands[MAX_DEFERRED_COMMANDS];
    u32 command_count;

    /* Systems */
    BavSystemDef* systems;
    u32 system_count;
    u32 system_capacity;
};

/* =============================================================================
 * Entity Admin Lifecycle
 * ============================================================================= */

BavEntityAdmin* bav_entity_admin_create(const BavEntityAdminConfig* config)
{
    BavEntityAdmin* admin = calloc(1, sizeof(BavEntityAdmin));
    if (!admin)
        return NULL;

    u32 entity_cap = config ? config->initial_entity_capacity : INITIAL_ENTITY_CAPACITY;
    u32 arch_cap = config ? config->initial_archetype_capacity : INITIAL_ARCHETYPE_CAPACITY;

    admin->entities = calloc(entity_cap, sizeof(EntityRecord));
    admin->entity_capacity = entity_cap;

    admin->free_list = calloc(entity_cap, sizeof(u32));

    admin->archetypes = calloc(arch_cap, sizeof(BavArchetype*));
    admin->archetype_capacity = arch_cap;

    admin->systems = calloc(32, sizeof(BavSystemDef));
    admin->system_capacity = 32;

    if (!admin->entities || !admin->free_list || !admin->archetypes || !admin->systems)
    {
        bav_entity_admin_destroy(admin);
        return NULL;
    }

    return admin;
}

void bav_entity_admin_destroy(BavEntityAdmin* admin)
{
    if (!admin)
        return;

    /* Free all archetypes */
    for (u32 i = 0; i < admin->archetype_count; i++)
    {
        BavArchetype* arch = admin->archetypes[i];
        if (arch)
        {
            for (u32 j = 0; j < arch->component_count; j++)
            {
                free(arch->component_arrays[j]);
            }
            free(arch->component_arrays);
            free(arch->component_ids);
            free(arch->entities);
            free(arch);
        }
    }

    /* Free pending command data */
    for (u32 i = 0; i < admin->command_count; i++)
    {
        free(admin->commands[i].data);
    }

    free(admin->archetypes);
    free(admin->entities);
    free(admin->free_list);
    free(admin->systems);
    free(admin);
}

void bav_entity_admin_flush(BavEntityAdmin* admin)
{
    /* Process all deferred commands */
    for (u32 i = 0; i < admin->command_count; i++)
    {
        DeferredCommand* cmd = &admin->commands[i];

        switch (cmd->type)
        {
            case CMD_CREATE_ENTITY:
                /* Already allocated in bav_entity_create */
                break;

            case CMD_DESTROY_ENTITY:
                if (cmd->entity.index < admin->entity_capacity)
                {
                    EntityRecord* rec = &admin->entities[cmd->entity.index];
                    if (rec->alive && rec->generation == cmd->entity.generation)
                    {
                        rec->alive = 0;
                        /* Add to free list */
                        admin->free_list[admin->free_count++] = cmd->entity.index;
                        admin->entity_count--;
                        /* TODO: Remove from archetype */
                    }
                }
                break;

            case CMD_ADD_COMPONENT:
                /* TODO: Implement archetype migration */
                break;

            case CMD_REMOVE_COMPONENT:
                /* TODO: Implement archetype migration */
                break;
        }

        free(cmd->data);
    }

    admin->command_count = 0;
}

/* =============================================================================
 * Component Registration
 * ============================================================================= */

BavComponentId bav_component_register(BavEntityAdmin* admin, const char* name, usize size,
                                      usize alignment)
{
    if (!admin || admin->component_count >= BAV_MAX_COMPONENTS)
    {
        return BAV_COMPONENT_INVALID;
    }

    BavComponentId id = admin->component_count++;
    BavComponentInfo* info = &admin->components[id];

    info->name = name;
    info->size = size;
    info->alignment = alignment;
    info->id = id;

    return id;
}

const BavComponentInfo* bav_component_get_info(BavEntityAdmin* admin, BavComponentId id)
{
    if (!admin || id >= admin->component_count)
    {
        return NULL;
    }
    return &admin->components[id];
}

/* =============================================================================
 * Entity Operations
 * ============================================================================= */

BavEntity bav_entity_create(BavEntityAdmin* admin)
{
    if (!admin)
        return BAV_ENTITY_NULL;

    u32 index;
    u16 generation;

    if (admin->free_count > 0)
    {
        /* Reuse a freed slot */
        index = admin->free_list[--admin->free_count];
        generation = admin->entities[index].generation + 1;
    }
    else
    {
        /* Allocate new slot */
        if (admin->entity_count >= admin->entity_capacity)
        {
            u32 new_cap = admin->entity_capacity * 2;
            EntityRecord* new_entities = realloc(admin->entities, new_cap * sizeof(EntityRecord));
            u32* new_free = realloc(admin->free_list, new_cap * sizeof(u32));
            if (!new_entities || !new_free)
            {
                return BAV_ENTITY_NULL;
            }
            admin->entities = new_entities;
            admin->free_list = new_free;
            admin->entity_capacity = new_cap;
        }
        index = admin->entity_count;
        generation = 1;
    }

    admin->entities[index].generation = generation;
    admin->entities[index].alive = 1;
    admin->entities[index].archetype = NULL;
    admin->entities[index].row = 0;
    admin->entity_count++;

    return (BavEntity){index, generation, 0};
}

void bav_entity_destroy(BavEntityAdmin* admin, BavEntity entity)
{
    if (!admin || admin->command_count >= MAX_DEFERRED_COMMANDS)
        return;

    DeferredCommand* cmd = &admin->commands[admin->command_count++];
    cmd->type = CMD_DESTROY_ENTITY;
    cmd->entity = entity;
    cmd->component = BAV_COMPONENT_INVALID;
    cmd->data = NULL;
    cmd->data_size = 0;
}

b8 bav_entity_valid(BavEntityAdmin* admin, BavEntity entity)
{
    if (!admin || entity.index >= admin->entity_capacity)
    {
        return 0;
    }
    EntityRecord* rec = &admin->entities[entity.index];
    return rec->alive && rec->generation == entity.generation;
}

u32 bav_entity_count(BavEntityAdmin* admin)
{
    return admin ? admin->entity_count : 0;
}

/* =============================================================================
 * Component Operations
 * ============================================================================= */

void bav_entity_add_component(BavEntityAdmin* admin, BavEntity entity, BavComponentId component,
                              const void* data)
{
    if (!admin || admin->command_count >= MAX_DEFERRED_COMMANDS)
        return;

    const BavComponentInfo* info = bav_component_get_info(admin, component);
    if (!info)
        return;

    DeferredCommand* cmd = &admin->commands[admin->command_count++];
    cmd->type = CMD_ADD_COMPONENT;
    cmd->entity = entity;
    cmd->component = component;
    cmd->data_size = info->size;
    cmd->data = malloc(info->size);
    if (cmd->data && data)
    {
        memcpy(cmd->data, data, info->size);
    }
}

void bav_entity_remove_component(BavEntityAdmin* admin, BavEntity entity, BavComponentId component)
{
    if (!admin || admin->command_count >= MAX_DEFERRED_COMMANDS)
        return;

    DeferredCommand* cmd = &admin->commands[admin->command_count++];
    cmd->type = CMD_REMOVE_COMPONENT;
    cmd->entity = entity;
    cmd->component = component;
    cmd->data = NULL;
    cmd->data_size = 0;
}

b8 bav_entity_has_component(BavEntityAdmin* admin, BavEntity entity, BavComponentId component)
{
    if (!bav_entity_valid(admin, entity))
        return 0;

    EntityRecord* rec = &admin->entities[entity.index];
    if (!rec->archetype)
        return 0;

    return (rec->archetype->component_mask & (1ULL << component)) != 0;
}

void* bav_entity_get_component(BavEntityAdmin* admin, BavEntity entity, BavComponentId component)
{
    if (!bav_entity_valid(admin, entity))
        return NULL;

    EntityRecord* rec = &admin->entities[entity.index];
    if (!rec->archetype)
        return NULL;
    if (!(rec->archetype->component_mask & (1ULL << component)))
        return NULL;

    /* Find component array index */
    BavArchetype* arch = rec->archetype;
    for (u32 i = 0; i < arch->component_count; i++)
    {
        if (arch->component_ids[i] == component)
        {
            const BavComponentInfo* info = bav_component_get_info(admin, component);
            if (!info)
                return NULL;
            return (char*)arch->component_arrays[i] + (rec->row * info->size);
        }
    }

    return NULL;
}

void bav_entity_set_component(BavEntityAdmin* admin, BavEntity entity, BavComponentId component,
                              const void* data)
{
    void* ptr = bav_entity_get_component(admin, entity, component);
    if (ptr && data)
    {
        const BavComponentInfo* info = bav_component_get_info(admin, component);
        if (info)
        {
            memcpy(ptr, data, info->size);
        }
    }
}

/* =============================================================================
 * Query System
 * ============================================================================= */

BavQuery bav_query_require(const BavComponentId* component_ids, u32 count)
{
    BavQuery query = {0};
    for (u32 i = 0; i < count; i++)
    {
        query.required_mask |= (1ULL << component_ids[i]);
    }
    return query;
}

BavQuery bav_query_exclude(BavQuery query, const BavComponentId* component_ids, u32 count)
{
    for (u32 i = 0; i < count; i++)
    {
        query.excluded_mask |= (1ULL << component_ids[i]);
    }
    return query;
}

BavQuery bav_query_optional(BavQuery query, const BavComponentId* component_ids, u32 count)
{
    for (u32 i = 0; i < count; i++)
    {
        query.optional_mask |= (1ULL << component_ids[i]);
    }
    return query;
}

void bav_query_each(BavEntityAdmin* admin, const BavQuery* query, BavQueryCallback callback,
                    void* user_data)
{
    if (!admin || !query || !callback)
        return;

    /* Iterate all archetypes */
    for (u32 a = 0; a < admin->archetype_count; a++)
    {
        BavArchetype* arch = admin->archetypes[a];
        if (!arch)
            continue;

        /* Check if archetype matches query */
        if ((arch->component_mask & query->required_mask) != query->required_mask)
            continue;
        if (arch->component_mask & query->excluded_mask)
            continue;

        /* Build component pointer array for this archetype */
        /* TODO: This should be optimized with ASM for the hot path */
        void* components[BAV_MAX_COMPONENTS];
        u32 comp_idx = 0;

        for (u32 c = 0; c < 64; c++)
        {
            if (query->required_mask & (1ULL << c))
            {
                for (u32 i = 0; i < arch->component_count; i++)
                {
                    if (arch->component_ids[i] == c)
                    {
                        components[comp_idx++] = arch->component_arrays[i];
                        break;
                    }
                }
            }
        }

        /* Iterate entities in archetype */
        for (u32 e = 0; e < arch->entity_count; e++)
        {
            /* Update component pointers for this entity */
            void* entity_components[BAV_MAX_COMPONENTS];
            for (u32 c = 0; c < comp_idx; c++)
            {
                /* TODO: Get component size from registry */
                /* For now, assume fixed stride - this is a stub */
                entity_components[c] = components[c];
            }

            callback(arch->entities[e], entity_components, user_data);
        }
    }
}

void bav_query_batch(BavEntityAdmin* admin, const BavQuery* query, BavQueryBatchCallback callback,
                     void* user_data)
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

        callback(arch->entity_count, arch->component_arrays, user_data);
    }
}

u32 bav_query_count(BavEntityAdmin* admin, const BavQuery* query)
{
    if (!admin || !query)
        return 0;

    u32 count = 0;
    for (u32 a = 0; a < admin->archetype_count; a++)
    {
        BavArchetype* arch = admin->archetypes[a];
        if (!arch)
            continue;

        if ((arch->component_mask & query->required_mask) != query->required_mask)
            continue;
        if (arch->component_mask & query->excluded_mask)
            continue;

        count += arch->entity_count;
    }
    return count;
}

/* =============================================================================
 * System Registration
 * ============================================================================= */

i32 bav_system_register(BavEntityAdmin* admin, const BavSystemDef* def)
{
    if (!admin || !def)
        return -1;

    if (admin->system_count >= admin->system_capacity)
    {
        u32 new_cap = admin->system_capacity * 2;
        BavSystemDef* new_systems = realloc(admin->systems, new_cap * sizeof(BavSystemDef));
        if (!new_systems)
            return -1;
        admin->systems = new_systems;
        admin->system_capacity = new_cap;
    }

    i32 id = (i32)admin->system_count;
    admin->systems[admin->system_count++] = *def;
    return id;
}

/* Simple insertion sort by priority - not worth anything fancier for <100 systems */
static int compare_systems(const void* a, const void* b)
{
    return ((const BavSystemDef*)a)->priority - ((const BavSystemDef*)b)->priority;
}

void bav_systems_update(BavEntityAdmin* admin, f32 delta_time)
{
    if (!admin)
        return;

    /* Sort systems by priority (should be cached, but good enough for now) */
    qsort(admin->systems, admin->system_count, sizeof(BavSystemDef), compare_systems);

    for (u32 i = 0; i < admin->system_count; i++)
    {
        BavSystemDef* sys = &admin->systems[i];
        if (sys->update)
        {
            sys->update(admin, delta_time, sys->user_data);
        }
    }
}

/* =============================================================================
 * Debug
 * ============================================================================= */

u32 bav_archetype_count(BavEntityAdmin* admin)
{
    return admin ? admin->archetype_count : 0;
}

void bav_ecs_debug_dump(BavEntityAdmin* admin)
{
    if (!admin)
        return;

#ifdef BAV_DEBUG
    /* TODO: Implement debug dump */
#endif
}
