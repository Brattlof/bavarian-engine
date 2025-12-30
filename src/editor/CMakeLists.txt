# ==============================================================================
# Bavarian Engine - Editor (ImGui)
# ==============================================================================
#
# Purpose:
#   ImGui-based editor application.
#   NOT shipped with games - development tool only.
#
# Dependencies:
#   - All runtime libraries
#   - ImGui (third_party)
#
# ==============================================================================

# Only build if ImGui is available
if(NOT BAV_BUILD_EDITOR)
    return()
endif()

# ImGui sources (docking branch recommended)
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)

add_library(bavarian_editor_lib OBJECT
    editor.cpp
    panels/hierarchy_panel.cpp
    panels/viewport_panel.cpp
    panels/inspector_panel.cpp
    panels/asset_browser_panel.cpp
    panels/console_panel.cpp
    panels/ecs_inspector_panel.cpp
    panels/performance_panel.cpp
    widgets/gizmo.cpp
    widgets/property_editor.cpp
    command.cpp
    selection.cpp
    theme.cpp
)

# ImGui integration
if(EXISTS ${IMGUI_DIR}/imgui.cpp)
    target_sources(bavarian_editor_lib PRIVATE
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
    )

    # Platform backends
    if(WIN32)
        target_sources(bavarian_editor_lib PRIVATE
            ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
        )
    endif()

    # Renderer backends
    if(BAV_ENABLE_VULKAN)
        target_sources(bavarian_editor_lib PRIVATE
            ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
        )
    endif()
endif()

target_include_directories(bavarian_editor_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_compile_definitions(bavarian_editor_lib PRIVATE
    BAV_EDITOR=1
    $<$<CONFIG:Debug>:BAV_DEBUG=1>
    # Enable ImGui docking
    IMGUI_ENABLE_DOCKING
)

target_link_libraries(bavarian_editor_lib PUBLIC
    $<$<BOOL:${BAV_ENABLE_VULKAN}>:Vulkan::Vulkan>
)
