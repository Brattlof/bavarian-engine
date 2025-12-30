# Renderer Backend - GPU API abstraction and implementations
add_library(bavarian_renderer_backend OBJECT
    device.c
    buffer.c
    texture.c
    pipeline.c
    commands.c
)

# Vulkan backend
if(BAV_ENABLE_VULKAN)
    target_sources(bavarian_renderer_backend PRIVATE
        vulkan/vk_device.c
        vulkan/vk_buffer.c
        vulkan/vk_texture.c
        vulkan/vk_pipeline.c
        vulkan/vk_commands.c
    )
    target_compile_definitions(bavarian_renderer_backend PRIVATE BAV_VULKAN=1)
endif()

# D3D12 backend (Windows only) - not yet implemented
# if(BAV_ENABLE_D3D12 AND WIN32)
#     target_sources(bavarian_renderer_backend PRIVATE
#         d3d12/d3d12_device.c
#         d3d12/d3d12_buffer.c
#         d3d12/d3d12_texture.c
#         d3d12/d3d12_pipeline.c
#         d3d12/d3d12_commands.c
#     )
#     target_compile_definitions(bavarian_renderer_backend PRIVATE BAV_D3D12=1)
# endif()

target_include_directories(bavarian_renderer_backend PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

if(BAV_ENABLE_VULKAN)
    target_link_libraries(bavarian_renderer_backend PRIVATE Vulkan::Vulkan)
endif()
