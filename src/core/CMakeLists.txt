# ==============================================================================
# Core Module
# ==============================================================================
# Fundamental types, math, memory management, containers
# Dependencies: None (this is the foundation)
# ==============================================================================

add_library(bavarian_core OBJECT
    types.c
    memory.c
    math.c
    arena.c
    hash.c
)

target_include_directories(bavarian_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

# Core must compile with maximum strictness
if(MSVC)
    target_compile_options(bavarian_core PRIVATE /W4)
else()
    target_compile_options(bavarian_core PRIVATE -Wall -Wextra -Wconversion)
endif()
