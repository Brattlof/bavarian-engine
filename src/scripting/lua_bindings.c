/**
 * @file lua_bindings.c
 * @brief Engine API bindings for Lua
 *
 * This is where we expose engine functionality to Lua scripts. The goal is
 * to make the API feel natural to Lua programmers while being safe and
 * performant.
 *
 * Every function here that talks to the engine needs to handle errors
 * gracefully and never crash, no matter what garbage the script throws at us.
 */

#include <bavarian/scripting.h>
#include <bavarian/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* External registration functions from other binding files */
extern void bav_lua_register_math(BavScriptContext* ctx);
extern void bav_lua_register_input(BavScriptContext* ctx);
extern void bav_lua_register_renderer(BavScriptContext* ctx);
extern void bav_lua_register_ecs(BavScriptContext* ctx);

/* =============================================================================
 * Core Functions
 * ============================================================================= */

/* print(...) - Debug output */
static BavCallResult lua_print(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                               void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    for (u32 i = 0; i < arg_count; i++)
    {
        if (i > 0)
            printf("\t");

        switch (args[i].type)
        {
            case BAV_VALUE_NIL:
                printf("nil");
                break;
            case BAV_VALUE_BOOL:
                printf("%s", args[i].as_bool ? "true" : "false");
                break;
            case BAV_VALUE_NUMBER:
                printf("%g", args[i].as_number);
                break;
            case BAV_VALUE_STRING:
                printf("%.*s", (int)args[i].as_string.length, args[i].as_string.data);
                break;
            default:
                printf("<value>");
                break;
        }
    }
    printf("\n");

    BavCallResult r = {0};
    r.success = true;
    return r;
}

/* type(value) -> string */
static BavCallResult lua_type(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                              void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    BavCallResult r = {0};
    r.success = true;
    r.value_count = 1;
    r.values = malloc(sizeof(BavValue));
    r.values[0].type = BAV_VALUE_STRING;

    const char* type_name = "nil";
    if (arg_count >= 1)
    {
        switch (args[0].type)
        {
            case BAV_VALUE_NIL:
                type_name = "nil";
                break;
            case BAV_VALUE_BOOL:
                type_name = "boolean";
                break;
            case BAV_VALUE_NUMBER:
                type_name = "number";
                break;
            case BAV_VALUE_STRING:
                type_name = "string";
                break;
            case BAV_VALUE_TABLE:
                type_name = "table";
                break;
            default:
                type_name = "userdata";
                break;
        }
    }

    r.values[0].as_string.data = type_name;
    r.values[0].as_string.length = strlen(type_name);
    return r;
}

/* tostring(value) -> string */
static BavCallResult lua_tostring(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                  void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    BavCallResult r = {0};
    r.success = true;
    r.value_count = 1;
    r.values = malloc(sizeof(BavValue));
    r.values[0].type = BAV_VALUE_STRING;

    static char buffer[64];

    if (arg_count < 1)
    {
        r.values[0].as_string.data = "nil";
        r.values[0].as_string.length = 3;
        return r;
    }

    switch (args[0].type)
    {
        case BAV_VALUE_NIL:
            r.values[0].as_string.data = "nil";
            r.values[0].as_string.length = 3;
            break;
        case BAV_VALUE_BOOL:
            if (args[0].as_bool)
            {
                r.values[0].as_string.data = "true";
                r.values[0].as_string.length = 4;
            }
            else
            {
                r.values[0].as_string.data = "false";
                r.values[0].as_string.length = 5;
            }
            break;
        case BAV_VALUE_NUMBER:
            snprintf(buffer, sizeof(buffer), "%g", args[0].as_number);
            r.values[0].as_string.data = buffer;
            r.values[0].as_string.length = strlen(buffer);
            break;
        case BAV_VALUE_STRING:
            r.values[0].as_string = args[0].as_string;
            break;
        default:
            r.values[0].as_string.data = "<value>";
            r.values[0].as_string.length = 7;
            break;
    }

    return r;
}

/* tonumber(value) -> number or nil */
static BavCallResult lua_tonumber(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                  void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    BavCallResult r = {0};
    r.success = true;
    r.value_count = 1;
    r.values = malloc(sizeof(BavValue));

    if (arg_count < 1)
    {
        r.values[0].type = BAV_VALUE_NIL;
        return r;
    }

    if (args[0].type == BAV_VALUE_NUMBER)
    {
        r.values[0] = args[0];
        return r;
    }

    if (args[0].type == BAV_VALUE_STRING)
    {
        char* end;
        double num = strtod(args[0].as_string.data, &end);
        if (end != args[0].as_string.data)
        {
            r.values[0].type = BAV_VALUE_NUMBER;
            r.values[0].as_number = num;
            return r;
        }
    }

    r.values[0].type = BAV_VALUE_NIL;
    return r;
}

/* error(message) - Raise an error */
static BavCallResult lua_error(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                               void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    BavCallResult r = {0};
    r.success = false;

    if (arg_count >= 1 && args[0].type == BAV_VALUE_STRING)
    {
        r.error = args[0].as_string.data;
    }
    else
    {
        r.error = "error";
    }

    return r;
}

/* assert(condition, message) */
static BavCallResult lua_assert(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    BavCallResult r = {0};

    b8 condition = false;
    if (arg_count >= 1)
    {
        if (args[0].type != BAV_VALUE_NIL && !(args[0].type == BAV_VALUE_BOOL && !args[0].as_bool))
        {
            condition = true;
        }
    }

    if (!condition)
    {
        r.success = false;
        if (arg_count >= 2 && args[1].type == BAV_VALUE_STRING)
        {
            r.error = args[1].as_string.data;
        }
        else
        {
            r.error = "assertion failed!";
        }
        return r;
    }

    /* Return all arguments on success */
    r.success = true;
    r.value_count = arg_count;
    if (arg_count > 0)
    {
        r.values = malloc(arg_count * sizeof(BavValue));
        memcpy(r.values, args, arg_count * sizeof(BavValue));
    }
    return r;
}

/* =============================================================================
 * Time Functions
 * ============================================================================= */

static f64 g_time = 0.0;
static f64 g_delta_time = 0.016;

/* time.now() -> seconds since start */
static BavCallResult lua_time_now(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                  void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(args);
    BAV_UNUSED(arg_count);
    BAV_UNUSED(user_data);

    BavCallResult r = {0};
    r.success = true;
    r.value_count = 1;
    r.values = malloc(sizeof(BavValue));
    r.values[0].type = BAV_VALUE_NUMBER;
    r.values[0].as_number = g_time;
    return r;
}

/* time.delta() -> seconds since last frame */
static BavCallResult lua_time_delta(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                    void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(args);
    BAV_UNUSED(arg_count);
    BAV_UNUSED(user_data);

    BavCallResult r = {0};
    r.success = true;
    r.value_count = 1;
    r.values = malloc(sizeof(BavValue));
    r.values[0].type = BAV_VALUE_NUMBER;
    r.values[0].as_number = g_delta_time;
    return r;
}

/* Called by engine to update time values */
void bav_lua_set_time(f64 time, f64 delta)
{
    g_time = time;
    g_delta_time = delta;
}

/* =============================================================================
 * Registration
 * ============================================================================= */

void bav_lua_register_all(BavScriptContext* ctx)
{
    /* Core functions */
    bav_script_register_function(ctx, "print", lua_print, NULL);
    bav_script_register_function(ctx, "type", lua_type, NULL);
    bav_script_register_function(ctx, "tostring", lua_tostring, NULL);
    bav_script_register_function(ctx, "tonumber", lua_tonumber, NULL);
    bav_script_register_function(ctx, "error", lua_error, NULL);
    bav_script_register_function(ctx, "assert", lua_assert, NULL);

    /* Time module */
    static BavNativeFnDef time_funcs[] = {
        {"now", lua_time_now},
        {"delta", lua_time_delta},
    };
    bav_script_register_module(ctx, "time", time_funcs, BAV_ARRAY_COUNT(time_funcs), NULL);

    /* Register sub-modules */
    bav_lua_register_math(ctx);
    bav_lua_register_input(ctx);
    bav_lua_register_renderer(ctx);
    bav_lua_register_ecs(ctx);
}
