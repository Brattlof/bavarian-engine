/**
 * @file window.h
 * @brief Window management interface
 *
 * Purpose:
 *   Platform-agnostic window creation and management.
 *   Handles window lifecycle, resizing, and native handle access.
 *
 * Constraints:
 *   - Single-threaded window operations
 *   - One window per handle (no aliasing)
 *   - Native handle lifetime tied to window lifetime
 */

#ifndef BAV3D_WINDOW_H
#define BAV3D_WINDOW_H

#include <bavarian3d/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* =============================================================================
     * Types
     * ============================================================================= */

    typedef struct Window Window;

    /**
     * Window creation parameters.
     */
    typedef struct WindowDesc
    {
        const char* title; /* Window title (UTF-8) */
        i32 width;         /* Initial width in pixels */
        i32 height;        /* Initial height in pixels */
        b8 resizable;      /* Allow user resizing */
        b8 fullscreen;     /* Start in fullscreen mode */
        b8 vsync;          /* Enable vertical sync */
        b8 hidden;         /* Create hidden initially */
    } WindowDesc;

    /**
     * Window event types.
     */
    typedef enum WindowEventType
    {
        WINDOW_EVENT_NONE = 0,
        WINDOW_EVENT_CLOSE,    /* User requested close */
        WINDOW_EVENT_RESIZE,   /* Window was resized */
        WINDOW_EVENT_FOCUS,    /* Window gained focus */
        WINDOW_EVENT_BLUR,     /* Window lost focus */
        WINDOW_EVENT_MINIMIZE, /* Window was minimized */
        WINDOW_EVENT_RESTORE,  /* Window was restored */
    } WindowEventType;

    /**
     * Window event data.
     */
    typedef struct WindowEvent
    {
        WindowEventType type;
        union
        {
            struct
            {
                i32 width, height;
            } resize;
        } data;
    } WindowEvent;

    /* =============================================================================
     * Window Lifecycle
     * ============================================================================= */

    /**
     * Create a new window.
     *
     * @param desc Window creation parameters
     * @return Window handle, or NULL on failure
     */
    Window* window_create(const WindowDesc* desc);

    /**
     * Destroy a window and release resources.
     */
    void window_destroy(Window* window);

    /* =============================================================================
     * Window State
     * ============================================================================= */

    /**
     * Check if window should close.
     */
    b8 window_should_close(const Window* window);

    /**
     * Set the should-close flag.
     */
    void window_set_should_close(Window* window, b8 should_close);

    /**
     * Get window dimensions.
     */
    void window_get_size(const Window* window, i32* width, i32* height);

    /**
     * Get framebuffer dimensions (may differ from window size on HiDPI).
     */
    void window_get_framebuffer_size(const Window* window, i32* width, i32* height);

    /**
     * Set window title.
     */
    void window_set_title(Window* window, const char* title);

    /**
     * Show or hide window.
     */
    void window_set_visible(Window* window, b8 visible);

    /**
     * Check if window has input focus.
     */
    b8 window_is_focused(const Window* window);

    /**
     * Check if window is minimized.
     */
    b8 window_is_minimized(const Window* window);

    /* =============================================================================
     * Event Processing
     * ============================================================================= */

    /**
     * Poll for window events.
     * Must be called regularly on the main thread.
     */
    void window_poll_events(void);

    /**
     * Wait for window events (blocks).
     */
    void window_wait_events(void);

    /**
     * Get next window event.
     *
     * @param window Window to query
     * @param event Output event data
     * @return true if event was retrieved, false if no events pending
     */
    b8 window_get_event(Window* window, WindowEvent* event);

    /* =============================================================================
     * Message Handler Callback (for ImGui integration etc.)
     * ============================================================================= */

    /**
     * Platform-specific message handler callback.
     * On Windows, the signature matches LRESULT(HWND, UINT, WPARAM, LPARAM).
     * Return non-zero to indicate the message was handled.
     */
    typedef isize (*WindowMsgHandler)(void* hwnd, u32 msg, usize wparam, isize lparam);

    /**
     * Set external message handler callback.
     * This is called before the default handler processes messages.
     * Used for ImGui integration - pass ImGui_ImplWin32_WndProcHandler.
     */
    void window_set_msg_handler(WindowMsgHandler handler);

    /* =============================================================================
     * Native Handle Access
     * ============================================================================= */

    /**
     * Get platform-specific native window handle.
     *
     * Windows: HWND
     * macOS:   NSWindow*
     * Linux:   Window (X11)
     */
    void* window_get_native_handle(const Window* window);

    /**
     * Get platform-specific native display/instance.
     *
     * Windows: HINSTANCE
     * macOS:   NULL (not applicable)
     * Linux:   Display* (X11)
     */
    void* window_get_native_display(void);

#ifdef __cplusplus
}
#endif

#endif /* BAV3D_WINDOW_H */
