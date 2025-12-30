# ==============================================================================
# Bavarian Engine - Resource Management
# ==============================================================================
#
# Purpose:
#   Asset loading, caching, and hot-reload support.
#
# Dependencies:
#   - core (types, memory)
#   - platform (file I/O)
#
# ==============================================================================

add_library(bavarian_resources OBJECT
    resource.c
    loader.c
    cache.c
    hot_reload.c
)

target_include_directories(bavarian_resources PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_definitions(bavarian_resources PRIVATE
    $<$<CONFIG:Debug>:BAV_DEBUG=1>
)
