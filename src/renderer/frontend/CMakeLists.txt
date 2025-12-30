# Renderer Frontend - High-level API, scene management
add_library(bavarian_renderer_frontend OBJECT
    scene.c
    camera.c
    mesh.c
    material.c
    commands.c
)

target_include_directories(bavarian_renderer_frontend PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)
