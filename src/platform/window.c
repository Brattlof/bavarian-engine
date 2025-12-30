/**
 * @file window.c
 * @brief Window management - platform dispatch layer
 *
 * This file contains common window logic. Platform-specific implementations
 * are in win32/, macos/, linux/ subdirectories.
 */

#include <bavarian3d/platform.h>
#include <bavarian3d/window.h>

/* Platform-specific functions declared here, implemented in platform directories */

#if defined(BAV3D_PLATFORM_WINDOWS)
Window* win32_window_create(const WindowDesc* desc);
void win32_window_destroy(Window* window);
b8 win32_window_should_close(const Window* window);
void win32_window_set_should_close(Window* window, b8 should_close);
void win32_window_get_size(const Window* window, i32* w, i32* h);
void win32_window_get_framebuffer_size(const Window* window, i32* w, i32* h);
void win32_window_set_title(Window* window, const char* title);
void win32_window_set_visible(Window* window, b8 visible);
b8 win32_window_is_focused(const Window* window);
b8 win32_window_is_minimized(const Window* window);
void win32_window_poll_events(void);
void win32_window_wait_events(void);
b8 win32_window_get_event(Window* window, WindowEvent* event);
void* win32_window_get_native_handle(const Window* window);
void* win32_window_get_native_display(void);

    #define PLATFORM_IMPL(fn) win32_##fn
#elif defined(BAV3D_PLATFORM_MACOS)
    /* macOS implementations */
    #define PLATFORM_IMPL(fn) macos_##fn
#elif defined(BAV3D_PLATFORM_LINUX)
    /* Linux implementations */
    #define PLATFORM_IMPL(fn) linux_##fn
#endif

/* =============================================================================
 * Window API - Dispatch to platform
 * ============================================================================= */

Window* window_create(const WindowDesc* desc)
{
    return PLATFORM_IMPL(window_create)(desc);
}

void window_destroy(Window* window)
{
    PLATFORM_IMPL(window_destroy)(window);
}

b8 window_should_close(const Window* window)
{
    return PLATFORM_IMPL(window_should_close)(window);
}

void window_set_should_close(Window* window, b8 should_close)
{
    PLATFORM_IMPL(window_set_should_close)(window, should_close);
}

void window_get_size(const Window* window, i32* width, i32* height)
{
    PLATFORM_IMPL(window_get_size)(window, width, height);
}

void window_get_framebuffer_size(const Window* window, i32* width, i32* height)
{
    PLATFORM_IMPL(window_get_framebuffer_size)(window, width, height);
}

void window_set_title(Window* window, const char* title)
{
    PLATFORM_IMPL(window_set_title)(window, title);
}

void window_set_visible(Window* window, b8 visible)
{
    PLATFORM_IMPL(window_set_visible)(window, visible);
}

b8 window_is_focused(const Window* window)
{
    return PLATFORM_IMPL(window_is_focused)(window);
}

b8 window_is_minimized(const Window* window)
{
    return PLATFORM_IMPL(window_is_minimized)(window);
}

void window_poll_events(void)
{
    PLATFORM_IMPL(window_poll_events)();
}

void window_wait_events(void)
{
    PLATFORM_IMPL(window_wait_events)();
}

b8 window_get_event(Window* window, WindowEvent* event)
{
    return PLATFORM_IMPL(window_get_event)(window, event);
}

void* window_get_native_handle(const Window* window)
{
    return PLATFORM_IMPL(window_get_native_handle)(window);
}

void* window_get_native_display(void)
{
    return PLATFORM_IMPL(window_get_native_display)();
}
