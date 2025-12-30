# ==============================================================================
# Tools
# ==============================================================================
# Build tools, asset processors, shader compilers, etc.
# These are development-time utilities, not shipped with the engine.
# ==============================================================================

# Demo Application - demonstrates platform layer and ECS
add_executable(bavarian-demo demo.c)
target_include_directories(bavarian-demo PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(bavarian-demo PRIVATE bavarian-runtime)

# Placeholder - will contain:
# - Shader compiler (GLSL/HLSL -> SPIR-V)
# - Asset converter (FBX/OBJ -> internal format)
# - Texture processor (compression, mipmap generation)
