cmake_minimum_required(VERSION 3.20)

project(bavarian-engine
    VERSION 0.1.0
    DESCRIPTION "High-performance game engine with ECS, Lua scripting, and assembly optimizations"
    LANGUAGES C CXX ASM
)

# ==============================================================================
# Build Configuration
# ==============================================================================

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Strict warning policy - we don't ship broken code
if(MSVC)
    add_compile_options(/W4 /WX /permissive-)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Werror -pedantic)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        add_compile_options(-Wno-unused-command-line-argument)
    endif()
endif()

# ==============================================================================
# Platform Detection
# ==============================================================================

if(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64")
    set(BAV_ARCH "x86_64")
    set(BAV_ARCH_X86_64 ON)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
    set(BAV_ARCH "arm64")
    set(BAV_ARCH_ARM64 ON)
else()
    message(FATAL_ERROR "Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

message(STATUS "Bavarian Engine Architecture: ${BAV_ARCH}")
message(STATUS "Bavarian Engine Platform: ${CMAKE_SYSTEM_NAME}")

# ==============================================================================
# Options
# ==============================================================================

option(BAV_BUILD_TESTS "Build test suite" ON)
option(BAV_BUILD_TOOLS "Build development tools" ON)
option(BAV_BUILD_EDITOR "Build the ImGui-based editor" ON)
option(BAV_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(BAV_ENABLE_VULKAN "Enable Vulkan backend" ON)
option(BAV_ENABLE_D3D12 "Enable D3D12 backend (Windows only)" ${WIN32})

# ==============================================================================
# Sanitizers
# ==============================================================================

if(BAV_ENABLE_ASAN AND NOT MSVC)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

# ==============================================================================
# Dependencies
# ==============================================================================

# Lua - we need this for scripting
find_package(Lua 5.4 QUIET)
if(NOT Lua_FOUND)
    message(STATUS "Lua not found, will use bundled version")
    set(BAV_USE_BUNDLED_LUA ON)
endif()

# Vulkan - primary rendering backend
if(BAV_ENABLE_VULKAN)
    find_package(Vulkan QUIET)
    if(Vulkan_FOUND)
        message(STATUS "Vulkan SDK found: ${Vulkan_INCLUDE_DIRS}")
    else()
        message(STATUS "Vulkan SDK not found, Vulkan backend disabled")
        set(BAV_ENABLE_VULKAN OFF)
    endif()
endif()

# ImGui - for the editor (fetched as submodule or downloaded)
if(BAV_BUILD_EDITOR)
    if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/imgui/imgui.h")
        message(STATUS "ImGui found in third_party")
        set(BAV_IMGUI_FOUND ON)
    else()
        message(STATUS "ImGui not found - editor will not be built")
        message(STATUS "  To enable editor, add ImGui to third_party/imgui/")
        set(BAV_BUILD_EDITOR OFF)
    endif()
endif()

# ==============================================================================
# Subdirectories - Core Engine
# ==============================================================================

add_subdirectory(src/core)
add_subdirectory(src/platform)
add_subdirectory(src/ecs)
add_subdirectory(src/renderer)
add_subdirectory(src/resources)
add_subdirectory(src/scene)
add_subdirectory(src/scripting)
add_subdirectory(src/asm)

# ==============================================================================
# Subdirectories - Optional
# ==============================================================================

if(BAV_BUILD_EDITOR)
    add_subdirectory(src/editor)
endif()

if(BAV_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(BAV_BUILD_TOOLS)
    add_subdirectory(tools)
endif()

# ==============================================================================
# Main Runtime Library
# ==============================================================================

add_library(bavarian-runtime STATIC
    $<TARGET_OBJECTS:bavarian_core>
    $<TARGET_OBJECTS:bavarian_platform>
    $<TARGET_OBJECTS:bavarian_ecs>
    $<TARGET_OBJECTS:bavarian_renderer>
    $<TARGET_OBJECTS:bavarian_resources>
    $<TARGET_OBJECTS:bavarian_scene>
    $<TARGET_OBJECTS:bavarian_scripting>
    $<TARGET_OBJECTS:bavarian_asm>
)

target_include_directories(bavarian-runtime PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(bavarian-runtime PUBLIC
    $<$<BOOL:${Lua_FOUND}>:${LUA_LIBRARIES}>
    $<$<BOOL:${BAV_ENABLE_VULKAN}>:Vulkan::Vulkan>
)

# Convenience alias
add_library(bavarian::runtime ALIAS bavarian-runtime)

# ==============================================================================
# Editor Application (if enabled)
# ==============================================================================

if(BAV_BUILD_EDITOR)
    add_executable(bavarian-editor
        src/editor/main.cpp
    )

    target_link_libraries(bavarian-editor PRIVATE
        bavarian-runtime
        bavarian_editor_lib
    )

    target_include_directories(bavarian-editor PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/imgui
    )
endif()

# ==============================================================================
# Installation
# ==============================================================================

install(TARGETS bavarian-runtime
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(DIRECTORY include/ DESTINATION include)

# ==============================================================================
# Summary
# ==============================================================================

message(STATUS "")
message(STATUS "=== Bavarian Engine Configuration ===")
message(STATUS "  Version:        ${PROJECT_VERSION}")
message(STATUS "  Architecture:   ${BAV_ARCH}")
message(STATUS "  Platform:       ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Build Type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "  Vulkan:         ${BAV_ENABLE_VULKAN}")
message(STATUS "  D3D12:          ${BAV_ENABLE_D3D12}")
message(STATUS "  Editor:         ${BAV_BUILD_EDITOR}")
message(STATUS "  Tests:          ${BAV_BUILD_TESTS}")
message(STATUS "  Tools:          ${BAV_BUILD_TOOLS}")
message(STATUS "======================================")
message(STATUS "")
