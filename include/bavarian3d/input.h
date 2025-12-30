/**
 * @file input.h
 * @brief Input handling interface
 *
 * Purpose:
 *   Platform-agnostic input handling for keyboard, mouse, and gamepad.
 *   Provides both polling and event-based input access.
 *
 * Constraints:
 *   - Input state is per-frame (call input_update each frame)
 *   - Thread-safe for reading, but update must be called from main thread
 */

#ifndef BAV3D_INPUT_H
#define BAV3D_INPUT_H

#include <bavarian3d/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* =============================================================================
     * Key Codes
     * ============================================================================= */

    typedef enum Key
    {
        KEY_UNKNOWN = 0,

        /* Printable keys */
        KEY_SPACE = 32,
        KEY_APOSTROPHE = 39,
        KEY_COMMA = 44,
        KEY_MINUS = 45,
        KEY_PERIOD = 46,
        KEY_SLASH = 47,

        KEY_0 = 48,
        KEY_1,
        KEY_2,
        KEY_3,
        KEY_4,
        KEY_5,
        KEY_6,
        KEY_7,
        KEY_8,
        KEY_9,

        KEY_SEMICOLON = 59,
        KEY_EQUAL = 61,

        KEY_A = 65,
        KEY_B,
        KEY_C,
        KEY_D,
        KEY_E,
        KEY_F,
        KEY_G,
        KEY_H,
        KEY_I,
        KEY_J,
        KEY_K,
        KEY_L,
        KEY_M,
        KEY_N,
        KEY_O,
        KEY_P,
        KEY_Q,
        KEY_R,
        KEY_S,
        KEY_T,
        KEY_U,
        KEY_V,
        KEY_W,
        KEY_X,
        KEY_Y,
        KEY_Z,

        KEY_LEFT_BRACKET = 91,
        KEY_BACKSLASH = 92,
        KEY_RIGHT_BRACKET = 93,
        KEY_GRAVE_ACCENT = 96,

        /* Function keys */
        KEY_ESCAPE = 256,
        KEY_ENTER = 257,
        KEY_TAB = 258,
        KEY_BACKSPACE = 259,
        KEY_INSERT = 260,
        KEY_DELETE = 261,
        KEY_RIGHT = 262,
        KEY_LEFT = 263,
        KEY_DOWN = 264,
        KEY_UP = 265,
        KEY_PAGE_UP = 266,
        KEY_PAGE_DOWN = 267,
        KEY_HOME = 268,
        KEY_END = 269,
        KEY_CAPS_LOCK = 280,
        KEY_SCROLL_LOCK = 281,
        KEY_NUM_LOCK = 282,
        KEY_PRINT_SCREEN = 283,
        KEY_PAUSE = 284,

        KEY_F1 = 290,
        KEY_F2,
        KEY_F3,
        KEY_F4,
        KEY_F5,
        KEY_F6,
        KEY_F7,
        KEY_F8,
        KEY_F9,
        KEY_F10,
        KEY_F11,
        KEY_F12,

        KEY_KP_0 = 320,
        KEY_KP_1,
        KEY_KP_2,
        KEY_KP_3,
        KEY_KP_4,
        KEY_KP_5,
        KEY_KP_6,
        KEY_KP_7,
        KEY_KP_8,
        KEY_KP_9,
        KEY_KP_DECIMAL = 330,
        KEY_KP_DIVIDE = 331,
        KEY_KP_MULTIPLY = 332,
        KEY_KP_SUBTRACT = 333,
        KEY_KP_ADD = 334,
        KEY_KP_ENTER = 335,
        KEY_KP_EQUAL = 336,

        KEY_LEFT_SHIFT = 340,
        KEY_LEFT_CONTROL = 341,
        KEY_LEFT_ALT = 342,
        KEY_LEFT_SUPER = 343,
        KEY_RIGHT_SHIFT = 344,
        KEY_RIGHT_CONTROL = 345,
        KEY_RIGHT_ALT = 346,
        KEY_RIGHT_SUPER = 347,
        KEY_MENU = 348,

        KEY_COUNT
    } Key;

    /* =============================================================================
     * Mouse Buttons
     * ============================================================================= */

    typedef enum MouseButton
    {
        MOUSE_BUTTON_LEFT = 0,
        MOUSE_BUTTON_RIGHT = 1,
        MOUSE_BUTTON_MIDDLE = 2,
        MOUSE_BUTTON_4 = 3,
        MOUSE_BUTTON_5 = 4,
        MOUSE_BUTTON_COUNT
    } MouseButton;

    /* =============================================================================
     * Input State
     * ============================================================================= */

    /**
     * Initialize the input system.
     */
    Result input_init(void);

    /**
     * Shutdown the input system.
     */
    void input_shutdown(void);

    /**
     * Update input state. Call once per frame before polling.
     */
    void input_update(void);

    /* =============================================================================
     * Keyboard
     * ============================================================================= */

    /**
     * Check if key is currently held down.
     */
    b8 input_key_down(Key key);

    /**
     * Check if key was pressed this frame (transition from up to down).
     */
    b8 input_key_pressed(Key key);

    /**
     * Check if key was released this frame (transition from down to up).
     */
    b8 input_key_released(Key key);

    /**
     * Check if any modifier keys are held.
     */
    b8 input_shift_down(void);
    b8 input_ctrl_down(void);
    b8 input_alt_down(void);
    b8 input_super_down(void);

    /* =============================================================================
     * Mouse
     * ============================================================================= */

    /**
     * Check if mouse button is currently held down.
     */
    b8 input_mouse_down(MouseButton button);

    /**
     * Check if mouse button was pressed this frame.
     */
    b8 input_mouse_pressed(MouseButton button);

    /**
     * Check if mouse button was released this frame.
     */
    b8 input_mouse_released(MouseButton button);

    /**
     * Get current mouse position in window coordinates.
     */
    void input_mouse_position(f32* x, f32* y);

    /**
     * Get mouse movement delta since last frame.
     */
    void input_mouse_delta(f32* dx, f32* dy);

    /**
     * Get scroll wheel delta since last frame.
     */
    void input_scroll_delta(f32* dx, f32* dy);

    /**
     * Set mouse cursor visibility.
     */
    void input_set_cursor_visible(b8 visible);

    /**
     * Lock mouse cursor to window center (for FPS-style input).
     */
    void input_set_cursor_locked(b8 locked);

    /* =============================================================================
     * Text Input
     * ============================================================================= */

    /**
     * Enable text input mode.
     * When enabled, character input events are generated.
     */
    void input_text_input_start(void);

    /**
     * Disable text input mode.
     */
    void input_text_input_stop(void);

    /**
     * Get next character from text input buffer.
     *
     * @param codepoint Output UTF-32 codepoint
     * @return true if character was retrieved, false if buffer empty
     */
    b8 input_text_input_get(u32* codepoint);

#ifdef __cplusplus
}
#endif

#endif /* BAV3D_INPUT_H */
