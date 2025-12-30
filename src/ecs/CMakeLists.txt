# ==============================================================================
# Bavarian Engine - Entity Component System
# ==============================================================================
#
# Purpose:
#   High-performance ECS with archetype-based storage.
#   Inspired by Fomalhaut, optimized for cache efficiency.
#
# Dependencies:
#   - core (types, memory, containers)
#
# ==============================================================================

add_library(bavarian_ecs OBJECT
    entity.c
    component.c
    archetype.c
    query.c
    system.c
    admin.c
    ecs_iterate_fast.c
)

target_include_directories(bavarian_ecs PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_definitions(bavarian_ecs PRIVATE
    $<$<CONFIG:Debug>:BAV_DEBUG=1>
)
