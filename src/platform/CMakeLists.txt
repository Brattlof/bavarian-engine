# ==============================================================================
# Platform Module
# ==============================================================================
# OS abstraction: window management, input, threading, timing, file I/O
# Dependencies: core
# ==============================================================================

set(PLATFORM_SOURCES
    platform.c
    window.c
    input.c
    time.c
    file.c
    thread.c
)

# Platform-specific sources
if(WIN32)
    list(APPEND PLATFORM_SOURCES
        win32/win32_window.c
        win32/win32_input.c
        win32/win32_time.c
        win32/win32_file.c
        win32/win32_thread.c
    )
elseif(APPLE)
    list(APPEND PLATFORM_SOURCES
        macos/macos_window.m
        macos/macos_input.m
        macos/macos_time.c
        macos/macos_file.c
        macos/macos_thread.c
    )
else()
    list(APPEND PLATFORM_SOURCES
        linux/linux_window.c
        linux/linux_input.c
        linux/linux_time.c
        linux/linux_file.c
        linux/linux_thread.c
    )
endif()

add_library(bavarian_platform OBJECT ${PLATFORM_SOURCES})

target_include_directories(bavarian_platform PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(bavarian_platform PRIVATE bavarian_core)

# Platform-specific link libraries
if(WIN32)
    target_link_libraries(bavarian_platform PRIVATE user32 gdi32 winmm)
elseif(APPLE)
    target_link_libraries(bavarian_platform PRIVATE
        "-framework Cocoa"
        "-framework QuartzCore"
    )
else()
    find_package(X11 REQUIRED)
    target_link_libraries(bavarian_platform PRIVATE ${X11_LIBRARIES} pthread)
endif()
