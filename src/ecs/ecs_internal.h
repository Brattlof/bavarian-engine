/**
 * @file ecs_internal.h
 * @brief Internal ECS structures - shared between ECS source files
 *
 * This header exposes the internal layout of ECS structures for use
 * by optimized iteration code. NOT part of the public API.
 */

#ifndef BAV_ECS_INTERNAL_H
#define BAV_ECS_INTERNAL_H

#include <bavarian/ecs.h>

/* =============================================================================
 * Internal Constants
 * ============================================================================= */

#define INITIAL_ENTITY_CAPACITY 1024
#define INITIAL_ARCHETYPE_CAPACITY 64
#define MAX_DEFERRED_COMMANDS 4096

/* =============================================================================
 * Internal Structures
 * ============================================================================= */

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

/**
 * Entity Admin internal structure.
 * The central ECS manager containing all entity, component, and archetype data.
 */
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

#endif /* BAV_ECS_INTERNAL_H */
