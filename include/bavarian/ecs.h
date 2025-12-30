/**
 * @file ecs.h
 * @brief Entity Component System for Bavarian Engine
 *
 * Purpose:
 *   Provides a high-performance ECS implementation inspired by Fomalhaut.
 *   Uses archetype-based storage for cache-efficient iteration.
 *
 * Architecture:
 *   - Entity: 64-bit handle with index + generation for stale detection
 *   - Component: Plain data structs registered at startup
 *   - Archetype: Groups entities with identical component sets
 *   - Query: Iterates over entities matching component requirements
 *
 * Constraints:
 *   - Components must be POD (plain old data) - no pointers to other components
 *   - Component registration must happen before any entity creation
 *   - Entity creation/destruction is deferred via command buffer
 *   - Assembly-optimized hot paths for iteration
 */

#ifndef BAV_ECS_H
#define BAV_ECS_H

#include <bavarian/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* =============================================================================
     * Forward Declarations
     * ============================================================================= */

    typedef struct BavEntityAdmin BavEntityAdmin;
    typedef struct BavArchetype BavArchetype;
    typedef struct BavQuery BavQuery;

    /* =============================================================================
     * Entity Handle
     * ============================================================================= */

    /**
     * Entity handle - 64 bits total
     * Layout: [32-bit index][16-bit generation][16-bit flags]
     *
     * Generation is incremented when an entity slot is recycled, allowing
     * detection of stale handles that reference destroyed entities.
     */
    typedef struct BavEntity
    {
        u32 index;
        u16 generation;
        u16 flags;
    } BavEntity;

#define BAV_ENTITY_NULL ((BavEntity){0, 0, 0})

    static inline b8 bav_entity_is_null(BavEntity e)
    {
        return e.index == 0 && e.generation == 0;
    }

    static inline b8 bav_entity_equals(BavEntity a, BavEntity b)
    {
        return a.index == b.index && a.generation == b.generation;
    }

/* =============================================================================
 * Component Registration
 * ============================================================================= */

/**
 * Maximum number of component types that can be registered.
 * This determines the bitmask width for archetype matching.
 * 64 components should be plenty - if you need more, reconsider your design.
 */
#define BAV_MAX_COMPONENTS 64

    /**
     * Component type ID - assigned during registration.
     */
    typedef u32 BavComponentId;

#define BAV_COMPONENT_INVALID ((BavComponentId)0xFFFFFFFF)

    /**
     * Component type information.
     */
    typedef struct BavComponentInfo
    {
        const char* name;
        usize size;
        usize alignment;
        BavComponentId id;
    } BavComponentInfo;

    /**
     * Register a component type with the ECS.
     * Must be called before creating any entities.
     *
     * @param admin     Entity admin
     * @param name      Component name (for debugging)
     * @param size      Size of component in bytes
     * @param alignment Alignment requirement
     * @return Component ID, or BAV_COMPONENT_INVALID on failure
     */
    BavComponentId bav_component_register(BavEntityAdmin* admin, const char* name, usize size,
                                          usize alignment);

    /**
     * Get component info by ID.
     */
    const BavComponentInfo* bav_component_get_info(BavEntityAdmin* admin, BavComponentId id);

/**
 * Helper macro for registering components with automatic size/align.
 */
#define BAV_REGISTER_COMPONENT(admin, type)                                                        \
    bav_component_register((admin), #type, sizeof(type), _Alignof(type))

    /* =============================================================================
     * Archetype
     * ============================================================================= */

    /**
     * Archetype - stores all entities with the same component combination.
     * Components are stored as Structure of Arrays (SoA) for cache efficiency.
     *
     * This is an opaque type - use the provided functions.
     */
    struct BavArchetype
    {
        u64 component_mask;            /* Bitmask of component types present */
        u32 entity_count;              /* Current number of entities */
        u32 entity_capacity;           /* Allocated capacity */
        BavEntity* entities;           /* Entity handles in this archetype */
        void** component_arrays;       /* Array of pointers to component data */
        BavComponentId* component_ids; /* Which components are stored (sorted) */
        u32 component_count;           /* Number of component types */
    };

    /* =============================================================================
     * Entity Admin
     * ============================================================================= */

    /**
     * Configuration for entity admin creation.
     */
    typedef struct BavEntityAdminConfig
    {
        u32 initial_entity_capacity;    /* Pre-allocate this many entity slots */
        u32 initial_archetype_capacity; /* Pre-allocate archetype storage */
    } BavEntityAdminConfig;

    /**
     * Create an entity admin (the central ECS manager).
     *
     * @param config Configuration (NULL for defaults)
     * @return New entity admin, or NULL on failure
     */
    BavEntityAdmin* bav_entity_admin_create(const BavEntityAdminConfig* config);

    /**
     * Destroy entity admin and all associated data.
     */
    void bav_entity_admin_destroy(BavEntityAdmin* admin);

    /**
     * Process deferred operations (entity creation/destruction).
     * Call once per frame after all systems have run.
     */
    void bav_entity_admin_flush(BavEntityAdmin* admin);

    /* =============================================================================
     * Entity Operations
     * ============================================================================= */

    /**
     * Create a new entity (deferred until flush).
     *
     * @param admin Entity admin
     * @return Handle to the new entity
     */
    BavEntity bav_entity_create(BavEntityAdmin* admin);

    /**
     * Destroy an entity (deferred until flush).
     *
     * @param admin  Entity admin
     * @param entity Entity to destroy
     */
    void bav_entity_destroy(BavEntityAdmin* admin, BavEntity entity);

    /**
     * Check if an entity handle is still valid.
     *
     * @param admin  Entity admin
     * @param entity Entity to check
     * @return true if entity exists and handle is not stale
     */
    b8 bav_entity_valid(BavEntityAdmin* admin, BavEntity entity);

    /**
     * Get the number of live entities.
     */
    u32 bav_entity_count(BavEntityAdmin* admin);

    /* =============================================================================
     * Component Operations
     * ============================================================================= */

    /**
     * Add a component to an entity (deferred until flush).
     *
     * @param admin     Entity admin
     * @param entity    Target entity
     * @param component Component type ID
     * @param data      Initial component data (copied)
     */
    void bav_entity_add_component(BavEntityAdmin* admin, BavEntity entity, BavComponentId component,
                                  const void* data);

    /**
     * Remove a component from an entity (deferred until flush).
     *
     * @param admin     Entity admin
     * @param entity    Target entity
     * @param component Component type ID
     */
    void bav_entity_remove_component(BavEntityAdmin* admin, BavEntity entity,
                                     BavComponentId component);

    /**
     * Check if an entity has a component.
     *
     * @param admin     Entity admin
     * @param entity    Target entity
     * @param component Component type ID
     * @return true if entity has the component
     */
    b8 bav_entity_has_component(BavEntityAdmin* admin, BavEntity entity, BavComponentId component);

    /**
     * Get a component from an entity.
     *
     * @param admin     Entity admin
     * @param entity    Target entity
     * @param component Component type ID
     * @return Pointer to component data, or NULL if not present
     */
    void* bav_entity_get_component(BavEntityAdmin* admin, BavEntity entity,
                                   BavComponentId component);

    /**
     * Set component data for an entity.
     *
     * @param admin     Entity admin
     * @param entity    Target entity
     * @param component Component type ID
     * @param data      New component data (copied)
     */
    void bav_entity_set_component(BavEntityAdmin* admin, BavEntity entity, BavComponentId component,
                                  const void* data);

    /* =============================================================================
     * Query System
     * ============================================================================= */

    /**
     * Query definition - specifies which entities to iterate.
     */
    struct BavQuery
    {
        u64 required_mask; /* Components that MUST be present */
        u64 excluded_mask; /* Components that MUST NOT be present */
        u64 optional_mask; /* Components to include if present */
    };

    /**
     * Create a query requiring specific components.
     *
     * @param component_ids Array of required component IDs
     * @param count         Number of components
     * @return Query structure
     */
    BavQuery bav_query_require(const BavComponentId* component_ids, u32 count);

    /**
     * Add excluded components to a query.
     *
     * @param query         Query to modify
     * @param component_ids Array of excluded component IDs
     * @param count         Number of components
     * @return Modified query
     */
    BavQuery bav_query_exclude(BavQuery query, const BavComponentId* component_ids, u32 count);

    /**
     * Add optional components to a query.
     *
     * @param query         Query to modify
     * @param component_ids Array of optional component IDs
     * @param count         Number of components
     * @return Modified query
     */
    BavQuery bav_query_optional(BavQuery query, const BavComponentId* component_ids, u32 count);

    /**
     * Callback for per-entity iteration.
     *
     * @param entity     Current entity
     * @param components Array of component pointers (in query order)
     * @param user_data  User-provided context
     */
    typedef void (*BavQueryCallback)(BavEntity entity, void** components, void* user_data);

    /**
     * Iterate over all entities matching a query.
     * This is the primary iteration method - uses ASM-optimized hot path.
     *
     * @param admin     Entity admin
     * @param query     Query specification
     * @param callback  Function to call for each entity
     * @param user_data Context passed to callback
     */
    void bav_query_each(BavEntityAdmin* admin, const BavQuery* query, BavQueryCallback callback,
                        void* user_data);

    /**
     * Callback for batch iteration (SIMD-friendly).
     *
     * @param count            Number of entities in this batch
     * @param component_arrays Array of component array pointers
     * @param user_data        User-provided context
     */
    typedef void (*BavQueryBatchCallback)(u32 count, void** component_arrays, void* user_data);

    /**
     * Iterate in batches for SIMD processing.
     * Each callback receives contiguous arrays of component data.
     *
     * @param admin     Entity admin
     * @param query     Query specification
     * @param callback  Function to call for each archetype batch
     * @param user_data Context passed to callback
     */
    void bav_query_batch(BavEntityAdmin* admin, const BavQuery* query,
                         BavQueryBatchCallback callback, void* user_data);

    /**
     * Get count of entities matching a query (without iterating).
     *
     * @param admin Entity admin
     * @param query Query specification
     * @return Number of matching entities
     */
    u32 bav_query_count(BavEntityAdmin* admin, const BavQuery* query);

    /* =============================================================================
     * System Registration (Optional Higher-Level API)
     * ============================================================================= */

    /**
     * System update function signature.
     *
     * @param admin     Entity admin
     * @param delta_time Time since last update
     * @param user_data  System-specific context
     */
    typedef void (*BavSystemUpdateFn)(BavEntityAdmin* admin, f32 delta_time, void* user_data);

    /**
     * System definition.
     */
    typedef struct BavSystemDef
    {
        const char* name;
        BavQuery query;
        BavSystemUpdateFn update;
        void* user_data;
        i32 priority; /* Lower runs first */
    } BavSystemDef;

    /**
     * Register a system with the admin.
     *
     * @param admin Entity admin
     * @param def   System definition
     * @return System ID, or -1 on failure
     */
    i32 bav_system_register(BavEntityAdmin* admin, const BavSystemDef* def);

    /**
     * Run all registered systems in priority order.
     *
     * @param admin      Entity admin
     * @param delta_time Time since last update
     */
    void bav_systems_update(BavEntityAdmin* admin, f32 delta_time);

    /* =============================================================================
     * Debug / Inspection
     * ============================================================================= */

    /**
     * Get archetype count (for debugging/profiling).
     */
    u32 bav_archetype_count(BavEntityAdmin* admin);

    /**
     * Dump ECS state to log (debug builds only).
     */
    void bav_ecs_debug_dump(BavEntityAdmin* admin);

#ifdef __cplusplus
}
#endif

#endif /* BAV_ECS_H */
