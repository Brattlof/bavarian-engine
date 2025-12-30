/**
 * @file lua_input.c
 * @brief Input bindings for Lua
 *
 * Exposes keyboard, mouse, and gamepad input to scripts. Pretty straightforward
 * stuff - mostly just wrapping the input module functions.
 */

#include <bavarian/scripting.h>
#include <bavarian/types.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations - these would come from input.h in a full build */
/* For now we stub them to get the bindings compiling */

typedef enum Key
{
    KEY_UNKNOWN = 0,
    KEY_SPACE = 32,
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
    KEY_ESCAPE = 256,
    KEY_ENTER,
    KEY_TAB,
    KEY_BACKSPACE,
    KEY_RIGHT = 262,
    KEY_LEFT,
    KEY_DOWN,
    KEY_UP,
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
    KEY_LEFT_SHIFT = 340,
    KEY_LEFT_CONTROL,
    KEY_LEFT_ALT,
    KEY_COUNT
} Key;

typedef enum MouseButton
{
    MOUSE_BUTTON_LEFT = 0,
    MOUSE_BUTTON_RIGHT,
    MOUSE_BUTTON_MIDDLE,
    MOUSE_BUTTON_COUNT
} MouseButton;

/* Stub implementations - these would be replaced by actual input system calls */
static b8 g_keys[KEY_COUNT] = {0};
static b8 g_keys_pressed[KEY_COUNT] = {0};
static b8 g_mouse[MOUSE_BUTTON_COUNT] = {0};
static b8 g_mouse_pressed[MOUSE_BUTTON_COUNT] = {0};
static f32 g_mouse_x = 0, g_mouse_y = 0;
static f32 g_mouse_dx = 0, g_mouse_dy = 0;
static f32 g_scroll_dx = 0, g_scroll_dy = 0;

/* =============================================================================
 * Helper Functions
 * ============================================================================= */

static BavCallResult make_bool(b8 value)
{
    BavCallResult r = {0};
    r.success = true;
    r.value_count = 1;
    r.values = malloc(sizeof(BavValue));
    r.values[0].type = BAV_VALUE_BOOL;
    r.values[0].as_bool = value;
    return r;
}

static BavCallResult make_number(f64 n)
{
    BavCallResult r = {0};
    r.success = true;
    r.value_count = 1;
    r.values = malloc(sizeof(BavValue));
    r.values[0].type = BAV_VALUE_NUMBER;
    r.values[0].as_number = n;
    return r;
}

static BavCallResult make_numbers2(f64 a, f64 b)
{
    BavCallResult r = {0};
    r.success = true;
    r.value_count = 2;
    r.values = malloc(2 * sizeof(BavValue));
    r.values[0].type = BAV_VALUE_NUMBER;
    r.values[0].as_number = a;
    r.values[1].type = BAV_VALUE_NUMBER;
    r.values[1].as_number = b;
    return r;
}

static BavCallResult make_nil(void)
{
    BavCallResult r = {0};
    r.success = true;
    r.value_count = 0;
    return r;
}

static BavCallResult make_error(const char* msg)
{
    BavCallResult r = {0};
    r.success = false;
    r.error = msg;
    return r;
}

/* =============================================================================
 * Key Code Lookup
 * ============================================================================= */

static Key string_to_key(const char* name, usize len)
{
    /* Single character keys */
    if (len == 1)
    {
        char c = name[0];
        if (c >= 'a' && c <= 'z')
            return (Key)(KEY_A + (c - 'a'));
        if (c >= 'A' && c <= 'Z')
            return (Key)(KEY_A + (c - 'A'));
        if (c >= '0' && c <= '9')
            return (Key)(KEY_0 + (c - '0'));
        if (c == ' ')
            return KEY_SPACE;
    }

    /* Named keys */
    if (len == 5 && memcmp(name, "space", 5) == 0)
        return KEY_SPACE;
    if (len == 6 && memcmp(name, "escape", 6) == 0)
        return KEY_ESCAPE;
    if (len == 5 && memcmp(name, "enter", 5) == 0)
        return KEY_ENTER;
    if (len == 3 && memcmp(name, "tab", 3) == 0)
        return KEY_TAB;
    if (len == 9 && memcmp(name, "backspace", 9) == 0)
        return KEY_BACKSPACE;
    if (len == 5 && memcmp(name, "right", 5) == 0)
        return KEY_RIGHT;
    if (len == 4 && memcmp(name, "left", 4) == 0)
        return KEY_LEFT;
    if (len == 4 && memcmp(name, "down", 4) == 0)
        return KEY_DOWN;
    if (len == 2 && memcmp(name, "up", 2) == 0)
        return KEY_UP;
    if (len == 5 && memcmp(name, "shift", 5) == 0)
        return KEY_LEFT_SHIFT;
    if (len == 4 && memcmp(name, "ctrl", 4) == 0)
        return KEY_LEFT_CONTROL;
    if (len == 3 && memcmp(name, "alt", 3) == 0)
        return KEY_LEFT_ALT;

    /* Function keys */
    if (len >= 2 && name[0] == 'f')
    {
        int num = 0;
        for (usize i = 1; i < len; i++)
        {
            if (name[i] >= '0' && name[i] <= '9')
            {
                num = num * 10 + (name[i] - '0');
            }
        }
        if (num >= 1 && num <= 12)
            return (Key)(KEY_F1 + (num - 1));
    }

    return KEY_UNKNOWN;
}

static MouseButton string_to_mouse(const char* name, usize len)
{
    if (len == 4 && memcmp(name, "left", 4) == 0)
        return MOUSE_BUTTON_LEFT;
    if (len == 5 && memcmp(name, "right", 5) == 0)
        return MOUSE_BUTTON_RIGHT;
    if (len == 6 && memcmp(name, "middle", 6) == 0)
        return MOUSE_BUTTON_MIDDLE;
    return MOUSE_BUTTON_LEFT;
}

/* =============================================================================
 * Keyboard Functions
 * ============================================================================= */

/* input.key_down(key_name) -> bool */
static BavCallResult lua_input_key_down(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                        void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    if (arg_count < 1 || args[0].type != BAV_VALUE_STRING)
    {
        return make_error("input.key_down requires a string argument");
    }

    Key key = string_to_key(args[0].as_string.data, args[0].as_string.length);
    if (key == KEY_UNKNOWN)
        return make_bool(false);

    return make_bool(g_keys[key]);
}

/* input.key_pressed(key_name) -> bool */
static BavCallResult lua_input_key_pressed(BavScriptContext* ctx, const BavValue* args,
                                           u32 arg_count, void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    if (arg_count < 1 || args[0].type != BAV_VALUE_STRING)
    {
        return make_error("input.key_pressed requires a string argument");
    }

    Key key = string_to_key(args[0].as_string.data, args[0].as_string.length);
    if (key == KEY_UNKNOWN)
        return make_bool(false);

    return make_bool(g_keys_pressed[key]);
}

/* =============================================================================
 * Mouse Functions
 * ============================================================================= */

/* input.mouse_down(button_name) -> bool */
static BavCallResult lua_input_mouse_down(BavScriptContext* ctx, const BavValue* args,
                                          u32 arg_count, void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    MouseButton button = MOUSE_BUTTON_LEFT;
    if (arg_count >= 1 && args[0].type == BAV_VALUE_STRING)
    {
        button = string_to_mouse(args[0].as_string.data, args[0].as_string.length);
    }

    return make_bool(g_mouse[button]);
}

/* input.mouse_pressed(button_name) -> bool */
static BavCallResult lua_input_mouse_pressed(BavScriptContext* ctx, const BavValue* args,
                                             u32 arg_count, void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    MouseButton button = MOUSE_BUTTON_LEFT;
    if (arg_count >= 1 && args[0].type == BAV_VALUE_STRING)
    {
        button = string_to_mouse(args[0].as_string.data, args[0].as_string.length);
    }

    return make_bool(g_mouse_pressed[button]);
}

/* input.mouse_position() -> x, y */
static BavCallResult lua_input_mouse_position(BavScriptContext* ctx, const BavValue* args,
                                              u32 arg_count, void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(args);
    BAV_UNUSED(arg_count);
    BAV_UNUSED(user_data);

    return make_numbers2(g_mouse_x, g_mouse_y);
}

/* input.mouse_delta() -> dx, dy */
static BavCallResult lua_input_mouse_delta(BavScriptContext* ctx, const BavValue* args,
                                           u32 arg_count, void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(args);
    BAV_UNUSED(arg_count);
    BAV_UNUSED(user_data);

    return make_numbers2(g_mouse_dx, g_mouse_dy);
}

/* input.scroll_delta() -> dx, dy */
static BavCallResult lua_input_scroll_delta(BavScriptContext* ctx, const BavValue* args,
                                            u32 arg_count, void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(args);
    BAV_UNUSED(arg_count);
    BAV_UNUSED(user_data);

    return make_numbers2(g_scroll_dx, g_scroll_dy);
}

/* =============================================================================
 * Utility Functions
 * ============================================================================= */

/* input.get_axis(negative_key, positive_key) -> -1, 0, or 1 */
static BavCallResult lua_input_get_axis(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                        void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    if (arg_count < 2 || args[0].type != BAV_VALUE_STRING || args[1].type != BAV_VALUE_STRING)
    {
        return make_error("input.get_axis requires two string arguments");
    }

    Key neg_key = string_to_key(args[0].as_string.data, args[0].as_string.length);
    Key pos_key = string_to_key(args[1].as_string.data, args[1].as_string.length);

    f64 value = 0.0;
    if (neg_key != KEY_UNKNOWN && g_keys[neg_key])
        value -= 1.0;
    if (pos_key != KEY_UNKNOWN && g_keys[pos_key])
        value += 1.0;

    return make_number(value);
}

/* =============================================================================
 * Internal State Update (called from engine)
 * ============================================================================= */

void bav_input_set_key(int key, b8 down)
{
    if (key >= 0 && key < KEY_COUNT)
    {
        if (down && !g_keys[key])
        {
            g_keys_pressed[key] = true;
        }
        g_keys[key] = down;
    }
}

void bav_input_set_mouse(int button, b8 down)
{
    if (button >= 0 && button < MOUSE_BUTTON_COUNT)
    {
        if (down && !g_mouse[button])
        {
            g_mouse_pressed[button] = true;
        }
        g_mouse[button] = down;
    }
}

void bav_input_set_mouse_position(f32 x, f32 y)
{
    g_mouse_dx = x - g_mouse_x;
    g_mouse_dy = y - g_mouse_y;
    g_mouse_x = x;
    g_mouse_y = y;
}

void bav_input_set_scroll(f32 dx, f32 dy)
{
    g_scroll_dx = dx;
    g_scroll_dy = dy;
}

void bav_input_frame_end(void)
{
    /* Clear per-frame state */
    for (int i = 0; i < KEY_COUNT; i++)
    {
        g_keys_pressed[i] = false;
    }
    for (int i = 0; i < MOUSE_BUTTON_COUNT; i++)
    {
        g_mouse_pressed[i] = false;
    }
    g_mouse_dx = 0;
    g_mouse_dy = 0;
    g_scroll_dx = 0;
    g_scroll_dy = 0;
}

/* =============================================================================
 * Registration
 * ============================================================================= */

void bav_lua_register_input(BavScriptContext* ctx)
{
    static BavNativeFnDef input_funcs[] = {
        {"key_down", lua_input_key_down},
        {"key_pressed", lua_input_key_pressed},
        {"mouse_down", lua_input_mouse_down},
        {"mouse_pressed", lua_input_mouse_pressed},
        {"mouse_position", lua_input_mouse_position},
        {"mouse_delta", lua_input_mouse_delta},
        {"scroll_delta", lua_input_scroll_delta},
        {"get_axis", lua_input_get_axis},
    };
    bav_script_register_module(ctx, "input", input_funcs, BAV_ARRAY_COUNT(input_funcs), NULL);
}
