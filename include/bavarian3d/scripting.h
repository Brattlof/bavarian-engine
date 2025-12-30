/**
 * @file scripting.h
 * @brief Lua scripting interface
 *
 * Purpose:
 *   Provides the Lua integration layer for engine scripting. Scripts can
 *   control scene objects, cameras, materials, and respond to input events.
 *
 * Constraints:
 *   - All engine APIs must be sandboxed (no raw file I/O, system calls, etc.)
 *   - Memory usage must be bounded and configurable
 *   - Script errors must not crash the engine
 *   - Lua state is single-threaded (call only from main thread)
 */

#ifndef BAV3D_SCRIPTING_H
#define BAV3D_SCRIPTING_H

#include <bavarian3d/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* =============================================================================
     * Types
     * ============================================================================= */

    typedef struct ScriptState ScriptState;

    typedef struct ScriptConfig
    {
        usize memory_limit;    /* Max Lua memory in bytes (0 = unlimited) */
        u32 instruction_limit; /* Max instructions per call (0 = unlimited) */
        b8 enable_debug;       /* Enable debug.* library */
    } ScriptConfig;

    /* =============================================================================
     * Lifecycle
     * ============================================================================= */

    /**
     * Create and initialize a new Lua state with engine bindings.
     *
     * @param config Script configuration
     * @return Script state, or NULL on failure
     */
    ScriptState* script_create(const ScriptConfig* config);

    /**
     * Destroy a script state and release resources.
     */
    void script_destroy(ScriptState* state);

    /* =============================================================================
     * Script Execution
     * ============================================================================= */

    /**
     * Load and execute a script file.
     *
     * @param state Script state
     * @param path Path to .lua file
     * @return RESULT_OK on success, error code on failure
     */
    Result script_load_file(ScriptState* state, const char* path);

    /**
     * Load and execute a script from string.
     *
     * @param state Script state
     * @param source Lua source code
     * @param name Name for error messages
     * @return RESULT_OK on success, error code on failure
     */
    Result script_load_string(ScriptState* state, const char* source, const char* name);

    /**
     * Call a global Lua function.
     *
     * @param state Script state
     * @param func_name Name of global function
     * @return RESULT_OK on success, error code on failure
     */
    Result script_call(ScriptState* state, const char* func_name);

    /**
     * Call the frame update function (if defined).
     *
     * @param state Script state
     * @param dt Delta time in seconds
     * @return RESULT_OK on success, error code on failure
     */
    Result script_update(ScriptState* state, f32 dt);

    /* =============================================================================
     * Error Handling
     * ============================================================================= */

    /**
     * Get the last error message.
     * Valid until next script operation.
     */
    const char* script_get_error(const ScriptState* state);

    /**
     * Set error callback for script errors.
     */
    typedef void (*ScriptErrorCallback)(const char* error, void* userdata);
    void script_set_error_callback(ScriptState* state, ScriptErrorCallback cb, void* userdata);

    /* =============================================================================
     * Memory
     * ============================================================================= */

    /**
     * Get current Lua memory usage in bytes.
     */
    usize script_get_memory_usage(const ScriptState* state);

    /**
     * Force garbage collection.
     */
    void script_gc(ScriptState* state);

#ifdef __cplusplus
}
#endif

#endif /* BAV3D_SCRIPTING_H */
