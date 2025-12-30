# ==============================================================================
# Bavarian Engine - Scene Management
# ==============================================================================
#
# Purpose:
#   Scene graph, prefabs, and serialization.
#
# Dependencies:
#   - core (types, memory)
#   - ecs (entity management)
#   - resources (asset loading)
#
# ==============================================================================

add_library(bavarian_scene OBJECT
    scene.c
    prefab.c
    serialization.c
)

target_include_directories(bavarian_scene PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_definitions(bavarian_scene PRIVATE
    $<$<CONFIG:Debug>:BAV_DEBUG=1>
)
