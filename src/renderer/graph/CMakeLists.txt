# Render Graph - Frame description, passes, dependencies
add_library(bavarian_renderer_graph OBJECT
    render_graph.c
    pass.c
    resource.c
)

target_include_directories(bavarian_renderer_graph PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)
