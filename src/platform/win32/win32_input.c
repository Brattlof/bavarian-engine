/**
 * @file win32_input.c
 * @brief Windows input implementation
 *
 * Keyboard and mouse input via Win32 messages. We track current and previous
 * frame state to detect press/release transitions. The window procedure calls
 * our internal functions to update state as messages come in.
 */

#include <bavarian3d/input.h>
#include <bavarian3d/platform.h>
#include <bavarian3d/types.h>

#ifdef BAV3D_PLATFORM_WINDOWS

    #define WIN32_LEAN_AND_MEAN
    #include <string.h>

    #include <windows.h>

/* =============================================================================
 * Input State
 * ============================================================================= */

typedef struct InputState
{
    /* Keyboard state - current and previous frame */
    b8 keys_current[KEY_COUNT];
    b8 keys_previous[KEY_COUNT];

    /* Mouse button state */
    b8 mouse_current[MOUSE_BUTTON_COUNT];
    b8 mouse_previous[MOUSE_BUTTON_COUNT];

    /* Mouse position */
    f32 mouse_x;
    f32 mouse_y;
    f32 mouse_x_prev;
    f32 mouse_y_prev;

    /* Scroll wheel accumulator */
    f32 scroll_x;
    f32 scroll_y;
    f32 scroll_x_delta;
    f32 scroll_y_delta;

    /* Text input */
    b8 text_input_enabled;
    u32 text_buffer[64];
    u32 text_buffer_head;
    u32 text_buffer_tail;

    /* Cursor state */
    b8 cursor_visible;
    b8 cursor_locked;
    HWND lock_window;

    b8 initialized;
} InputState;

static InputState g_input = {0};

/* =============================================================================
 * Virtual Key to Key Mapping
 * ============================================================================= */

static Key vk_to_key(WPARAM vk, LPARAM lparam)
{
    /* Handle extended keys */
    b8 extended = (lparam >> 24) & 1;

    switch (vk)
    {
        /* Letters */
        case 'A':
            return KEY_A;
        case 'B':
            return KEY_B;
        case 'C':
            return KEY_C;
        case 'D':
            return KEY_D;
        case 'E':
            return KEY_E;
        case 'F':
            return KEY_F;
        case 'G':
            return KEY_G;
        case 'H':
            return KEY_H;
        case 'I':
            return KEY_I;
        case 'J':
            return KEY_J;
        case 'K':
            return KEY_K;
        case 'L':
            return KEY_L;
        case 'M':
            return KEY_M;
        case 'N':
            return KEY_N;
        case 'O':
            return KEY_O;
        case 'P':
            return KEY_P;
        case 'Q':
            return KEY_Q;
        case 'R':
            return KEY_R;
        case 'S':
            return KEY_S;
        case 'T':
            return KEY_T;
        case 'U':
            return KEY_U;
        case 'V':
            return KEY_V;
        case 'W':
            return KEY_W;
        case 'X':
            return KEY_X;
        case 'Y':
            return KEY_Y;
        case 'Z':
            return KEY_Z;

        /* Numbers */
        case '0':
            return KEY_0;
        case '1':
            return KEY_1;
        case '2':
            return KEY_2;
        case '3':
            return KEY_3;
        case '4':
            return KEY_4;
        case '5':
            return KEY_5;
        case '6':
            return KEY_6;
        case '7':
            return KEY_7;
        case '8':
            return KEY_8;
        case '9':
            return KEY_9;

        /* Function keys */
        case VK_F1:
            return KEY_F1;
        case VK_F2:
            return KEY_F2;
        case VK_F3:
            return KEY_F3;
        case VK_F4:
            return KEY_F4;
        case VK_F5:
            return KEY_F5;
        case VK_F6:
            return KEY_F6;
        case VK_F7:
            return KEY_F7;
        case VK_F8:
            return KEY_F8;
        case VK_F9:
            return KEY_F9;
        case VK_F10:
            return KEY_F10;
        case VK_F11:
            return KEY_F11;
        case VK_F12:
            return KEY_F12;

        /* Special keys */
        case VK_ESCAPE:
            return KEY_ESCAPE;
        case VK_RETURN:
            return extended ? KEY_KP_ENTER : KEY_ENTER;
        case VK_TAB:
            return KEY_TAB;
        case VK_BACK:
            return KEY_BACKSPACE;
        case VK_INSERT:
            return KEY_INSERT;
        case VK_DELETE:
            return KEY_DELETE;
        case VK_SPACE:
            return KEY_SPACE;

        /* Navigation */
        case VK_LEFT:
            return KEY_LEFT;
        case VK_RIGHT:
            return KEY_RIGHT;
        case VK_UP:
            return KEY_UP;
        case VK_DOWN:
            return KEY_DOWN;
        case VK_PRIOR:
            return KEY_PAGE_UP;
        case VK_NEXT:
            return KEY_PAGE_DOWN;
        case VK_HOME:
            return KEY_HOME;
        case VK_END:
            return KEY_END;

        /* Lock keys */
        case VK_CAPITAL:
            return KEY_CAPS_LOCK;
        case VK_SCROLL:
            return KEY_SCROLL_LOCK;
        case VK_NUMLOCK:
            return KEY_NUM_LOCK;
        case VK_SNAPSHOT:
            return KEY_PRINT_SCREEN;
        case VK_PAUSE:
            return KEY_PAUSE;

        /* Numpad */
        case VK_NUMPAD0:
            return KEY_KP_0;
        case VK_NUMPAD1:
            return KEY_KP_1;
        case VK_NUMPAD2:
            return KEY_KP_2;
        case VK_NUMPAD3:
            return KEY_KP_3;
        case VK_NUMPAD4:
            return KEY_KP_4;
        case VK_NUMPAD5:
            return KEY_KP_5;
        case VK_NUMPAD6:
            return KEY_KP_6;
        case VK_NUMPAD7:
            return KEY_KP_7;
        case VK_NUMPAD8:
            return KEY_KP_8;
        case VK_NUMPAD9:
            return KEY_KP_9;
        case VK_DECIMAL:
            return KEY_KP_DECIMAL;
        case VK_DIVIDE:
            return KEY_KP_DIVIDE;
        case VK_MULTIPLY:
            return KEY_KP_MULTIPLY;
        case VK_SUBTRACT:
            return KEY_KP_SUBTRACT;
        case VK_ADD:
            return KEY_KP_ADD;

        /* Modifiers */
        case VK_SHIFT:
            return (lparam & 0x00FF0000) == 0x002A0000 ? KEY_LEFT_SHIFT : KEY_RIGHT_SHIFT;
        case VK_CONTROL:
            return extended ? KEY_RIGHT_CONTROL : KEY_LEFT_CONTROL;
        case VK_MENU:
            return extended ? KEY_RIGHT_ALT : KEY_LEFT_ALT;
        case VK_LWIN:
            return KEY_LEFT_SUPER;
        case VK_RWIN:
            return KEY_RIGHT_SUPER;
        case VK_APPS:
            return KEY_MENU;

        /* Punctuation */
        case VK_OEM_1:
            return KEY_SEMICOLON;
        case VK_OEM_PLUS:
            return KEY_EQUAL;
        case VK_OEM_COMMA:
            return KEY_COMMA;
        case VK_OEM_MINUS:
            return KEY_MINUS;
        case VK_OEM_PERIOD:
            return KEY_PERIOD;
        case VK_OEM_2:
            return KEY_SLASH;
        case VK_OEM_3:
            return KEY_GRAVE_ACCENT;
        case VK_OEM_4:
            return KEY_LEFT_BRACKET;
        case VK_OEM_5:
            return KEY_BACKSLASH;
        case VK_OEM_6:
            return KEY_RIGHT_BRACKET;
        case VK_OEM_7:
            return KEY_APOSTROPHE;

        default:
            return KEY_UNKNOWN;
    }
}

/* =============================================================================
 * Internal Functions (called from window procedure)
 * ============================================================================= */

void win32_input_key_event(WPARAM vk, LPARAM lparam, b8 down)
{
    if (!g_input.initialized)
        return;

    Key key = vk_to_key(vk, lparam);
    if (key != KEY_UNKNOWN && key < KEY_COUNT)
    {
        g_input.keys_current[key] = down;
    }
}

void win32_input_mouse_button_event(MouseButton button, b8 down)
{
    if (!g_input.initialized)
        return;

    if (button < MOUSE_BUTTON_COUNT)
    {
        g_input.mouse_current[button] = down;
    }
}

void win32_input_mouse_move_event(i32 x, i32 y)
{
    if (!g_input.initialized)
        return;

    g_input.mouse_x = (f32)x;
    g_input.mouse_y = (f32)y;
}

void win32_input_mouse_wheel_event(f32 delta, b8 horizontal)
{
    if (!g_input.initialized)
        return;

    if (horizontal)
    {
        g_input.scroll_x += delta;
    }
    else
    {
        g_input.scroll_y += delta;
    }
}

void win32_input_char_event(u32 codepoint)
{
    if (!g_input.initialized || !g_input.text_input_enabled)
        return;

    /* Add to circular buffer if not full */
    u32 next = (g_input.text_buffer_head + 1) % 64;
    if (next != g_input.text_buffer_tail)
    {
        g_input.text_buffer[g_input.text_buffer_head] = codepoint;
        g_input.text_buffer_head = next;
    }
}

void win32_input_focus_lost(void)
{
    if (!g_input.initialized)
        return;

    /* Clear all key/button states when losing focus */
    memset(g_input.keys_current, 0, sizeof(g_input.keys_current));
    memset(g_input.mouse_current, 0, sizeof(g_input.mouse_current));
}

/* =============================================================================
 * Public API Implementation
 * ============================================================================= */

Result input_init(void)
{
    memset(&g_input, 0, sizeof(g_input));
    g_input.cursor_visible = true;
    g_input.initialized = true;
    return RESULT_OK;
}

void input_shutdown(void)
{
    if (g_input.cursor_locked)
    {
        ClipCursor(NULL);
    }
    memset(&g_input, 0, sizeof(g_input));
}

void input_update(void)
{
    if (!g_input.initialized)
        return;

    /* Copy current state to previous */
    memcpy(g_input.keys_previous, g_input.keys_current, sizeof(g_input.keys_current));
    memcpy(g_input.mouse_previous, g_input.mouse_current, sizeof(g_input.mouse_current));

    /* Update mouse delta */
    g_input.mouse_x_prev = g_input.mouse_x;
    g_input.mouse_y_prev = g_input.mouse_y;

    /* Latch and clear scroll accumulator */
    g_input.scroll_x_delta = g_input.scroll_x;
    g_input.scroll_y_delta = g_input.scroll_y;
    g_input.scroll_x = 0.0f;
    g_input.scroll_y = 0.0f;

    /* Handle cursor locking */
    if (g_input.cursor_locked && g_input.lock_window)
    {
        RECT rect;
        GetClientRect(g_input.lock_window, &rect);
        POINT center = {(rect.right - rect.left) / 2, (rect.bottom - rect.top) / 2};
        ClientToScreen(g_input.lock_window, &center);
        SetCursorPos(center.x, center.y);

        /* Convert back to client coords for mouse position */
        ScreenToClient(g_input.lock_window, &center);
        g_input.mouse_x = (f32)center.x;
        g_input.mouse_y = (f32)center.y;
    }
}

/* Keyboard */

b8 input_key_down(Key key)
{
    if (key >= KEY_COUNT)
        return false;
    return g_input.keys_current[key];
}

b8 input_key_pressed(Key key)
{
    if (key >= KEY_COUNT)
        return false;
    return g_input.keys_current[key] && !g_input.keys_previous[key];
}

b8 input_key_released(Key key)
{
    if (key >= KEY_COUNT)
        return false;
    return !g_input.keys_current[key] && g_input.keys_previous[key];
}

b8 input_shift_down(void)
{
    return g_input.keys_current[KEY_LEFT_SHIFT] || g_input.keys_current[KEY_RIGHT_SHIFT];
}

b8 input_ctrl_down(void)
{
    return g_input.keys_current[KEY_LEFT_CONTROL] || g_input.keys_current[KEY_RIGHT_CONTROL];
}

b8 input_alt_down(void)
{
    return g_input.keys_current[KEY_LEFT_ALT] || g_input.keys_current[KEY_RIGHT_ALT];
}

b8 input_super_down(void)
{
    return g_input.keys_current[KEY_LEFT_SUPER] || g_input.keys_current[KEY_RIGHT_SUPER];
}

/* Mouse */

b8 input_mouse_down(MouseButton button)
{
    if (button >= MOUSE_BUTTON_COUNT)
        return false;
    return g_input.mouse_current[button];
}

b8 input_mouse_pressed(MouseButton button)
{
    if (button >= MOUSE_BUTTON_COUNT)
        return false;
    return g_input.mouse_current[button] && !g_input.mouse_previous[button];
}

b8 input_mouse_released(MouseButton button)
{
    if (button >= MOUSE_BUTTON_COUNT)
        return false;
    return !g_input.mouse_current[button] && g_input.mouse_previous[button];
}

void input_mouse_position(f32* x, f32* y)
{
    if (x)
        *x = g_input.mouse_x;
    if (y)
        *y = g_input.mouse_y;
}

void input_mouse_delta(f32* dx, f32* dy)
{
    if (dx)
        *dx = g_input.mouse_x - g_input.mouse_x_prev;
    if (dy)
        *dy = g_input.mouse_y - g_input.mouse_y_prev;
}

void input_scroll_delta(f32* dx, f32* dy)
{
    if (dx)
        *dx = g_input.scroll_x_delta;
    if (dy)
        *dy = g_input.scroll_y_delta;
}

void input_set_cursor_visible(b8 visible)
{
    if (g_input.cursor_visible != visible)
    {
        g_input.cursor_visible = visible;
        ShowCursor(visible);
    }
}

void input_set_cursor_locked(b8 locked)
{
    g_input.cursor_locked = locked;

    if (locked)
    {
        /* Get the foreground window for locking */
        g_input.lock_window = GetForegroundWindow();
        if (g_input.lock_window)
        {
            RECT rect;
            GetClientRect(g_input.lock_window, &rect);
            MapWindowPoints(g_input.lock_window, NULL, (POINT*)&rect, 2);
            ClipCursor(&rect);
        }
    }
    else
    {
        ClipCursor(NULL);
        g_input.lock_window = NULL;
    }
}

/* Text Input */

void input_text_input_start(void)
{
    g_input.text_input_enabled = true;
}

void input_text_input_stop(void)
{
    g_input.text_input_enabled = false;
    /* Clear buffer */
    g_input.text_buffer_head = 0;
    g_input.text_buffer_tail = 0;
}

b8 input_text_input_get(u32* codepoint)
{
    if (!g_input.text_input_enabled)
        return false;

    if (g_input.text_buffer_head == g_input.text_buffer_tail)
        return false;

    if (codepoint)
    {
        *codepoint = g_input.text_buffer[g_input.text_buffer_tail];
    }
    g_input.text_buffer_tail = (g_input.text_buffer_tail + 1) % 64;
    return true;
}

#endif /* BAV3D_PLATFORM_WINDOWS */
