/**
 * @file input.c
 * @brief Input system - fallback implementation for non-Windows platforms
 *
 * On Windows, win32_input.c provides the full implementation.
 * This file provides a fallback for other platforms.
 */

#include <bavarian3d/input.h>
#include <bavarian3d/memory.h>
#include <bavarian3d/platform.h>

/* Windows uses win32_input.c instead */
#ifndef BAV3D_PLATFORM_WINDOWS

/* =============================================================================
 * Internal State
 * ============================================================================= */

    #define TEXT_INPUT_BUFFER_SIZE 64

typedef struct InputState
{
    /* Keyboard state */
    u8 keys_current[KEY_COUNT];
    u8 keys_previous[KEY_COUNT];

    /* Mouse state */
    u8 mouse_current[MOUSE_BUTTON_COUNT];
    u8 mouse_previous[MOUSE_BUTTON_COUNT];
    f32 mouse_x, mouse_y;
    f32 mouse_x_prev, mouse_y_prev;
    f32 scroll_x, scroll_y;

    /* Text input */
    b8 text_input_active;
    u32 text_buffer[TEXT_INPUT_BUFFER_SIZE];
    u32 text_buffer_head;
    u32 text_buffer_tail;
} InputState;

static InputState g_input = {0};

/* =============================================================================
 * Initialization
 * ============================================================================= */

Result input_init(void)
{
    mem_zero(&g_input, sizeof(g_input));
    return RESULT_OK;
}

void input_shutdown(void)
{
    mem_zero(&g_input, sizeof(g_input));
}

void input_update(void)
{
    /* Copy current state to previous */
    mem_copy(g_input.keys_previous, g_input.keys_current, sizeof(g_input.keys_current));
    mem_copy(g_input.mouse_previous, g_input.mouse_current, sizeof(g_input.mouse_current));

    g_input.mouse_x_prev = g_input.mouse_x;
    g_input.mouse_y_prev = g_input.mouse_y;

    /* Reset per-frame accumulators */
    g_input.scroll_x = 0;
    g_input.scroll_y = 0;
}

/* =============================================================================
 * Internal: Called by platform code
 * ============================================================================= */

void input_internal_set_key(Key key, b8 down)
{
    if (key >= 0 && key < KEY_COUNT)
    {
        g_input.keys_current[key] = down ? 1 : 0;
    }
}

void input_internal_set_mouse_button(MouseButton button, b8 down)
{
    if (button >= 0 && button < MOUSE_BUTTON_COUNT)
    {
        g_input.mouse_current[button] = down ? 1 : 0;
    }
}

void input_internal_set_mouse_position(f32 x, f32 y)
{
    g_input.mouse_x = x;
    g_input.mouse_y = y;
}

void input_internal_add_scroll(f32 dx, f32 dy)
{
    g_input.scroll_x += dx;
    g_input.scroll_y += dy;
}

void input_internal_add_text_char(u32 codepoint)
{
    if (!g_input.text_input_active)
        return;

    u32 next_head = (g_input.text_buffer_head + 1) % TEXT_INPUT_BUFFER_SIZE;
    if (next_head != g_input.text_buffer_tail)
    {
        g_input.text_buffer[g_input.text_buffer_head] = codepoint;
        g_input.text_buffer_head = next_head;
    }
}

/* =============================================================================
 * Keyboard Queries
 * ============================================================================= */

b8 input_key_down(Key key)
{
    if (key < 0 || key >= KEY_COUNT)
        return false;
    return g_input.keys_current[key] != 0;
}

b8 input_key_pressed(Key key)
{
    if (key < 0 || key >= KEY_COUNT)
        return false;
    return g_input.keys_current[key] && !g_input.keys_previous[key];
}

b8 input_key_released(Key key)
{
    if (key < 0 || key >= KEY_COUNT)
        return false;
    return !g_input.keys_current[key] && g_input.keys_previous[key];
}

b8 input_shift_down(void)
{
    return input_key_down(KEY_LEFT_SHIFT) || input_key_down(KEY_RIGHT_SHIFT);
}

b8 input_ctrl_down(void)
{
    return input_key_down(KEY_LEFT_CONTROL) || input_key_down(KEY_RIGHT_CONTROL);
}

b8 input_alt_down(void)
{
    return input_key_down(KEY_LEFT_ALT) || input_key_down(KEY_RIGHT_ALT);
}

b8 input_super_down(void)
{
    return input_key_down(KEY_LEFT_SUPER) || input_key_down(KEY_RIGHT_SUPER);
}

/* =============================================================================
 * Mouse Queries
 * ============================================================================= */

b8 input_mouse_down(MouseButton button)
{
    if (button < 0 || button >= MOUSE_BUTTON_COUNT)
        return false;
    return g_input.mouse_current[button] != 0;
}

b8 input_mouse_pressed(MouseButton button)
{
    if (button < 0 || button >= MOUSE_BUTTON_COUNT)
        return false;
    return g_input.mouse_current[button] && !g_input.mouse_previous[button];
}

b8 input_mouse_released(MouseButton button)
{
    if (button < 0 || button >= MOUSE_BUTTON_COUNT)
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
        *dx = g_input.scroll_x;
    if (dy)
        *dy = g_input.scroll_y;
}

/* Cursor control - stub implementations, platform code must handle */
void input_set_cursor_visible(b8 visible)
{
    (void)visible;
}
void input_set_cursor_locked(b8 locked)
{
    (void)locked;
}

/* =============================================================================
 * Text Input
 * ============================================================================= */

void input_text_input_start(void)
{
    g_input.text_input_active = true;
    g_input.text_buffer_head = 0;
    g_input.text_buffer_tail = 0;
}

void input_text_input_stop(void)
{
    g_input.text_input_active = false;
}

b8 input_text_input_get(u32* codepoint)
{
    if (g_input.text_buffer_tail == g_input.text_buffer_head)
    {
        return false;
    }

    *codepoint = g_input.text_buffer[g_input.text_buffer_tail];
    g_input.text_buffer_tail = (g_input.text_buffer_tail + 1) % TEXT_INPUT_BUFFER_SIZE;
    return true;
}

#endif /* !BAV3D_PLATFORM_WINDOWS */
