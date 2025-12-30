/**
 * @file win32_window.c
 * @brief Windows window implementation
 *
 * Win32 window management. This is pretty standard stuff - register a class,
 * create a window, pump messages. The fun part is getting HiDPI right, which
 * we're mostly ignoring for now because it's a rabbit hole.
 */

#include <bavarian3d/memory.h>
#include <bavarian3d/window.h>

#ifdef BAV3D_PLATFORM_WINDOWS

    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>

/* =============================================================================
 * Window State
 * ============================================================================= */

struct Window
{
    HWND hwnd;
    HINSTANCE hinstance;
    i32 width;
    i32 height;
    b8 should_close;
    b8 focused;
    b8 minimized;
};

static const wchar_t* WINDOW_CLASS_NAME = L"BAV3D_WindowClass";
static b8 g_class_registered = false;

/* =============================================================================
 * Window Procedure
 * ============================================================================= */

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    Window* window = (Window*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg)
    {
        case WM_CLOSE:
            if (window)
                window->should_close = true;
            return 0;

        case WM_SIZE:
            if (window)
            {
                window->width = LOWORD(lparam);
                window->height = HIWORD(lparam);
                window->minimized = (wparam == SIZE_MINIMIZED);
            }
            return 0;

        case WM_SETFOCUS:
            if (window)
                window->focused = true;
            return 0;

        case WM_KILLFOCUS:
            if (window)
                window->focused = false;
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

/* =============================================================================
 * Class Registration
 * ============================================================================= */

static b8 register_window_class(HINSTANCE hinstance)
{
    if (g_class_registered)
        return true;

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hinstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = WINDOW_CLASS_NAME;

    if (!RegisterClassExW(&wc))
    {
        return false;
    }

    g_class_registered = true;
    return true;
}

/* =============================================================================
 * Window Lifecycle
 * ============================================================================= */

Window* win32_window_create(const WindowDesc* desc)
{
    HINSTANCE hinstance = GetModuleHandleW(NULL);

    if (!register_window_class(hinstance))
    {
        return NULL;
    }

    Window* window = MEM_ALLOC_TYPE_ZERO(NULL, Window);
    if (!window)
        return NULL;

    window->hinstance = hinstance;
    window->width = desc->width;
    window->height = desc->height;

    /* Calculate window size to get desired client area */
    DWORD style = WS_OVERLAPPEDWINDOW;
    if (!desc->resizable)
    {
        style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    }

    RECT rect = {0, 0, desc->width, desc->height};
    AdjustWindowRect(&rect, style, FALSE);

    /* Convert title to wide string */
    int title_len = MultiByteToWideChar(CP_UTF8, 0, desc->title, -1, NULL, 0);
    wchar_t* title_wide = (wchar_t*)mem_alloc(NULL, title_len * sizeof(wchar_t), 2);
    MultiByteToWideChar(CP_UTF8, 0, desc->title, -1, title_wide, title_len);

    window->hwnd = CreateWindowExW(0, WINDOW_CLASS_NAME, title_wide, style, CW_USEDEFAULT,
                                   CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top,
                                   NULL, NULL, hinstance, NULL);

    mem_free(NULL, title_wide, title_len * sizeof(wchar_t));

    if (!window->hwnd)
    {
        MEM_FREE_TYPE(NULL, window, Window);
        return NULL;
    }

    SetWindowLongPtrW(window->hwnd, GWLP_USERDATA, (LONG_PTR)window);

    if (!desc->hidden)
    {
        ShowWindow(window->hwnd, SW_SHOW);
    }

    return window;
}

void win32_window_destroy(Window* window)
{
    if (!window)
        return;

    if (window->hwnd)
    {
        DestroyWindow(window->hwnd);
    }

    MEM_FREE_TYPE(NULL, window, Window);
}

/* =============================================================================
 * Window State
 * ============================================================================= */

b8 win32_window_should_close(const Window* window)
{
    return window ? window->should_close : true;
}

void win32_window_set_should_close(Window* window, b8 should_close)
{
    if (window)
        window->should_close = should_close;
}

void win32_window_get_size(const Window* window, i32* w, i32* h)
{
    if (w)
        *w = window ? window->width : 0;
    if (h)
        *h = window ? window->height : 0;
}

void win32_window_get_framebuffer_size(const Window* window, i32* w, i32* h)
{
    /* On Windows without DPI awareness, framebuffer = window size */
    /* TODO: Handle HiDPI properly */
    win32_window_get_size(window, w, h);
}

void win32_window_set_title(Window* window, const char* title)
{
    if (!window || !window->hwnd)
        return;

    int len = MultiByteToWideChar(CP_UTF8, 0, title, -1, NULL, 0);
    wchar_t* wide = (wchar_t*)mem_alloc(NULL, len * sizeof(wchar_t), 2);
    MultiByteToWideChar(CP_UTF8, 0, title, -1, wide, len);
    SetWindowTextW(window->hwnd, wide);
    mem_free(NULL, wide, len * sizeof(wchar_t));
}

void win32_window_set_visible(Window* window, b8 visible)
{
    if (!window || !window->hwnd)
        return;
    ShowWindow(window->hwnd, visible ? SW_SHOW : SW_HIDE);
}

b8 win32_window_is_focused(const Window* window)
{
    return window ? window->focused : false;
}

b8 win32_window_is_minimized(const Window* window)
{
    return window ? window->minimized : false;
}

/* =============================================================================
 * Event Processing
 * ============================================================================= */

void win32_window_poll_events(void)
{
    MSG msg;
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void win32_window_wait_events(void)
{
    WaitMessage();
    win32_window_poll_events();
}

b8 win32_window_get_event(Window* window, WindowEvent* event)
{
    /* Events are handled in WindowProc and stored in window state */
    /* This is a simplified implementation - real version would queue events */
    (void)window;
    (void)event;
    return false;
}

/* =============================================================================
 * Native Handle Access
 * ============================================================================= */

void* win32_window_get_native_handle(const Window* window)
{
    return window ? window->hwnd : NULL;
}

void* win32_window_get_native_display(void)
{
    return GetModuleHandleW(NULL);
}

#endif /* BAV3D_PLATFORM_WINDOWS */
