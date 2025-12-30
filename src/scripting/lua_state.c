/**
 * @file lua_state.c
 * @brief Lua state management
 *
 * This is where we create and configure the Lua VM. The tricky bit is getting
 * the sandboxing right - we want scripts to be able to do useful things but
 * not escape the sandbox and trash the user's system.
 *
 * Memory limiting is particularly annoying because Lua doesn't have great
 * hooks for it. We use a custom allocator and track usage manually.
 */

#include <bavarian3d/memory.h>
#include <bavarian3d/scripting.h>

#include <stdio.h>

#ifdef BAV3D_HAS_LUA
    #include <lauxlib.h>
    #include <lua.h>
    #include <lualib.h>
#endif

/* =============================================================================
 * State Structure
 * ============================================================================= */

struct ScriptState
{
#ifdef BAV3D_HAS_LUA
    lua_State* L;
#else
    void* placeholder; /* Keep the struct non-empty */
#endif
    usize memory_used;
    usize memory_limit;
    char error_buffer[1024];
    ScriptErrorCallback error_cb;
    void* error_userdata;
};

/* =============================================================================
 * Custom Allocator (for memory tracking)
 * ============================================================================= */

#ifdef BAV3D_HAS_LUA
static void* lua_alloc(void* ud, void* ptr, size_t osize, size_t nsize)
{
    ScriptState* state = (ScriptState*)ud;

    if (nsize == 0)
    {
        /* Free */
        if (ptr)
        {
            state->memory_used -= osize;
            mem_free(NULL, ptr, osize);
        }
        return NULL;
    }

    /* Check memory limit before allocating */
    if (state->memory_limit > 0)
    {
        usize new_usage = state->memory_used - osize + nsize;
        if (new_usage > state->memory_limit)
        {
            return NULL; /* Allocation denied */
        }
    }

    void* new_ptr;
    if (ptr == NULL)
    {
        /* Alloc */
        new_ptr = mem_alloc(NULL, nsize, 8);
    }
    else
    {
        /* Realloc */
        new_ptr = mem_realloc(NULL, ptr, osize, nsize, 8);
    }

    if (new_ptr)
    {
        state->memory_used = state->memory_used - osize + nsize;
    }
    return new_ptr;
}
#endif

/* =============================================================================
 * Lifecycle
 * ============================================================================= */

ScriptState* script_create(const ScriptConfig* config)
{
    ScriptState* state = MEM_ALLOC_TYPE_ZERO(NULL, ScriptState);
    if (!state)
        return NULL;

    state->memory_limit = config ? config->memory_limit : 0;

#ifdef BAV3D_HAS_LUA
    state->L = lua_newstate(lua_alloc, state);
    if (!state->L)
    {
        MEM_FREE_TYPE(NULL, state, ScriptState);
        return NULL;
    }

    /*
     * Open standard libraries - but not all of them. io and os are dangerous
     * because they let scripts touch the filesystem and run commands. debug
     * is usually disabled too unless we're actually debugging.
     */
    luaL_openlibs(state->L); /* For now open everything, lock it down later */

    /* Register engine bindings */
    /* TODO: Call the binding registration functions */
#endif

    return state;
}

void script_destroy(ScriptState* state)
{
    if (!state)
        return;

#ifdef BAV3D_HAS_LUA
    if (state->L)
    {
        lua_close(state->L);
    }
#endif

    MEM_FREE_TYPE(NULL, state, ScriptState);
}

/* =============================================================================
 * Script Execution
 * ============================================================================= */

Result script_load_file(ScriptState* state, const char* path)
{
    if (!state || !path)
        return RESULT_ERROR_INVALID_ARG;

#ifdef BAV3D_HAS_LUA
    int status = luaL_loadfile(state->L, path);
    if (status != LUA_OK)
    {
        const char* err = lua_tostring(state->L, -1);
        snprintf(state->error_buffer, sizeof(state->error_buffer), "%s",
                 err ? err : "unknown error");
        lua_pop(state->L, 1);
        return RESULT_ERROR_IO;
    }

    status = lua_pcall(state->L, 0, 0, 0);
    if (status != LUA_OK)
    {
        const char* err = lua_tostring(state->L, -1);
        snprintf(state->error_buffer, sizeof(state->error_buffer), "%s",
                 err ? err : "unknown error");
        lua_pop(state->L, 1);
        return RESULT_ERROR_GENERAL;
    }
    return RESULT_OK;
#else
    (void)path;
    snprintf(state->error_buffer, sizeof(state->error_buffer), "Lua not available");
    return RESULT_ERROR_UNSUPPORTED;
#endif
}

Result script_load_string(ScriptState* state, const char* source, const char* name)
{
    if (!state || !source)
        return RESULT_ERROR_INVALID_ARG;

#ifdef BAV3D_HAS_LUA
    int status = luaL_loadbuffer(state->L, source, strlen(source), name ? name : "chunk");
    if (status != LUA_OK)
    {
        const char* err = lua_tostring(state->L, -1);
        snprintf(state->error_buffer, sizeof(state->error_buffer), "%s",
                 err ? err : "unknown error");
        lua_pop(state->L, 1);
        return RESULT_ERROR_GENERAL;
    }

    status = lua_pcall(state->L, 0, 0, 0);
    if (status != LUA_OK)
    {
        const char* err = lua_tostring(state->L, -1);
        snprintf(state->error_buffer, sizeof(state->error_buffer), "%s",
                 err ? err : "unknown error");
        lua_pop(state->L, 1);
        return RESULT_ERROR_GENERAL;
    }
    return RESULT_OK;
#else
    (void)source;
    (void)name;
    snprintf(state->error_buffer, sizeof(state->error_buffer), "Lua not available");
    return RESULT_ERROR_UNSUPPORTED;
#endif
}

Result script_call(ScriptState* state, const char* func_name)
{
    if (!state || !func_name)
        return RESULT_ERROR_INVALID_ARG;

#ifdef BAV3D_HAS_LUA
    lua_getglobal(state->L, func_name);
    if (!lua_isfunction(state->L, -1))
    {
        lua_pop(state->L, 1);
        snprintf(state->error_buffer, sizeof(state->error_buffer), "'%s' is not a function",
                 func_name);
        return RESULT_ERROR_NOT_FOUND;
    }

    int status = lua_pcall(state->L, 0, 0, 0);
    if (status != LUA_OK)
    {
        const char* err = lua_tostring(state->L, -1);
        snprintf(state->error_buffer, sizeof(state->error_buffer), "%s",
                 err ? err : "unknown error");
        lua_pop(state->L, 1);
        if (state->error_cb)
        {
            state->error_cb(state->error_buffer, state->error_userdata);
        }
        return RESULT_ERROR_GENERAL;
    }
    return RESULT_OK;
#else
    (void)func_name;
    return RESULT_ERROR_UNSUPPORTED;
#endif
}

Result script_update(ScriptState* state, f32 dt)
{
    if (!state)
        return RESULT_ERROR_INVALID_ARG;

#ifdef BAV3D_HAS_LUA
    lua_getglobal(state->L, "update");
    if (!lua_isfunction(state->L, -1))
    {
        lua_pop(state->L, 1);
        return RESULT_OK; /* No update function is fine */
    }

    lua_pushnumber(state->L, dt);
    int status = lua_pcall(state->L, 1, 0, 0);
    if (status != LUA_OK)
    {
        const char* err = lua_tostring(state->L, -1);
        snprintf(state->error_buffer, sizeof(state->error_buffer), "%s",
                 err ? err : "unknown error");
        lua_pop(state->L, 1);
        if (state->error_cb)
        {
            state->error_cb(state->error_buffer, state->error_userdata);
        }
        return RESULT_ERROR_GENERAL;
    }
#else
    (void)dt;
#endif

    return RESULT_OK;
}

/* =============================================================================
 * Error Handling
 * ============================================================================= */

const char* script_get_error(const ScriptState* state)
{
    return state ? state->error_buffer : "null state";
}

void script_set_error_callback(ScriptState* state, ScriptErrorCallback cb, void* userdata)
{
    if (state)
    {
        state->error_cb = cb;
        state->error_userdata = userdata;
    }
}

/* =============================================================================
 * Memory
 * ============================================================================= */

usize script_get_memory_usage(const ScriptState* state)
{
    return state ? state->memory_used : 0;
}

void script_gc(ScriptState* state)
{
    if (!state)
        return;

#ifdef BAV3D_HAS_LUA
    lua_gc(state->L, LUA_GCCOLLECT, 0);
#endif
}
