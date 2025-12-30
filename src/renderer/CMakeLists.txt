# ==============================================================================
# Renderer Module
# ==============================================================================
# Multi-backend 3D renderer with Vulkan/D3D12/Software support
# Dependencies: core, platform
# ==============================================================================

add_subdirectory(frontend)
add_subdirectory(graph)
add_subdirectory(backend)

add_library(bavarian_renderer OBJECT
    renderer.c
)

target_include_directories(bavarian_renderer PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(bavarian_renderer PRIVATE
    bavarian_core
    bavarian_platform
)

# D3D12 backend (Windows only)
if(BAV_ENABLE_D3D12 AND WIN32)
    target_compile_definitions(bavarian_renderer PRIVATE BAV3D_D3D12=1)
endif()
