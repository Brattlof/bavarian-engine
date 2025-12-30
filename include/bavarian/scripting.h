/**
 * @file scripting.h
 * @brief Scripting system for Bavarian Engine
 *
 * Purpose:
 *   Provides Lua scripting integration with a custom compiler infrastructure
 *   designed for future multi-language support.
 *
 * Architecture:
 *   - Language-agnostic plugin interface
 *   - Lua as primary scripting language
 *   - Custom compiler pipeline: Lexer -> Parser -> Analyzer -> Codegen
 *   - Sandboxed execution with resource limits
 *
 * Constraints:
 *   - No direct exposure of engine internals to scripts
 *   - Handle-based references for all engine objects
 *   - Graceful error handling (no engine crashes from script errors)
 *   - Deterministic compilation (same input = same output)
 */

#ifndef BAV_SCRIPTING_H
#define BAV_SCRIPTING_H

#include <bavarian/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* =============================================================================
     * Forward Declarations
     * ============================================================================= */

    typedef struct BavScriptContext BavScriptContext;
    typedef struct BavScriptVM BavScriptVM;
    typedef struct BavCompiledScript BavCompiledScript;

    /* =============================================================================
     * Script Value Types
     * ============================================================================= */

    typedef enum BavValueType
    {
        BAV_VALUE_NIL = 0,
        BAV_VALUE_BOOL,
        BAV_VALUE_NUMBER,
        BAV_VALUE_STRING,
        BAV_VALUE_TABLE,
        BAV_VALUE_FUNCTION,
        BAV_VALUE_USERDATA,
        BAV_VALUE_HANDLE, /* Engine object handle */
    } BavValueType;

    /**
     * Tagged union for script values.
     */
    typedef struct BavValue
    {
        BavValueType type;
        union
        {
            b8 as_bool;
            f64 as_number;
            struct
            {
                const char* data;
                usize length;
            } as_string;
            void* as_userdata;
            BavHandle as_handle;
        };
    } BavValue;

#define BAV_VALUE_NIL_INIT ((BavValue){.type = BAV_VALUE_NIL})

    /* =============================================================================
     * Compilation
     * ============================================================================= */

    /**
     * Compilation options.
     */
    typedef struct BavCompileOptions
    {
        const char* filename; /* Source filename (for error messages) */
        b8 debug_info;        /* Include debug info for stack traces */
        b8 optimize;          /* Enable optimizations */
        u32 max_errors;       /* Stop after this many errors (0 = unlimited) */
    } BavCompileOptions;

    /**
     * Compilation error information.
     */
    typedef struct BavCompileError
    {
        const char* message;
        const char* filename;
        u32 line;
        u32 column;
        u32 length; /* Length of error span */
    } BavCompileError;

    /**
     * Compilation result.
     */
    typedef struct BavCompileResult
    {
        BavCompiledScript* script; /* Compiled script, or NULL on failure */
        BavCompileError* errors;   /* Array of errors */
        u32 error_count;
        b8 success;
    } BavCompileResult;

    /**
     * Compile Lua source code to bytecode.
     *
     * @param source     Source code (UTF-8)
     * @param source_len Length of source in bytes
     * @param options    Compilation options (NULL for defaults)
     * @return Compilation result
     */
    BavCompileResult bav_compile_lua(const char* source, usize source_len,
                                     const BavCompileOptions* options);

    /**
     * Free compilation result (including script and errors).
     */
    void bav_compile_result_free(BavCompileResult* result);

    /**
     * Free compiled script.
     */
    void bav_compiled_script_free(BavCompiledScript* script);

    /* =============================================================================
     * Script Context
     * ============================================================================= */

    /**
     * Script context configuration.
     */
    typedef struct BavScriptContextConfig
    {
        usize max_memory;     /* Memory limit in bytes (0 = unlimited) */
        u64 max_instructions; /* Instruction limit per call (0 = unlimited) */
        b8 enable_debug;      /* Enable debug hooks */
    } BavScriptContextConfig;

    /**
     * Create a new script execution context.
     *
     * @param config Configuration (NULL for defaults)
     * @return Script context, or NULL on failure
     */
    BavScriptContext* bav_script_context_create(const BavScriptContextConfig* config);

    /**
     * Destroy script context and release resources.
     */
    void bav_script_context_destroy(BavScriptContext* ctx);

    /**
     * Load a compiled script into the context.
     *
     * @param ctx    Script context
     * @param script Compiled script
     * @return BAV_OK on success
     */
    BavResult bav_script_context_load(BavScriptContext* ctx, BavCompiledScript* script);

    /**
     * Get current memory usage of context.
     */
    usize bav_script_context_memory_used(BavScriptContext* ctx);

    /* =============================================================================
     * Script Execution
     * ============================================================================= */

    /**
     * Call result.
     */
    typedef struct BavCallResult
    {
        BavValue* values; /* Return values */
        u32 value_count;
        b8 success;
        const char* error; /* Error message if !success */
    } BavCallResult;

    /**
     * Call a script function.
     *
     * @param ctx       Script context
     * @param name      Function name (NULL to call main chunk)
     * @param args      Arguments (NULL if none)
     * @param arg_count Number of arguments
     * @return Call result
     */
    BavCallResult bav_script_call(BavScriptContext* ctx, const char* name, const BavValue* args,
                                  u32 arg_count);

    /**
     * Free call result.
     */
    void bav_call_result_free(BavCallResult* result);

    /**
     * Set a global variable in the context.
     *
     * @param ctx   Script context
     * @param name  Variable name
     * @param value Value to set
     */
    void bav_script_set_global(BavScriptContext* ctx, const char* name, BavValue value);

    /**
     * Get a global variable from the context.
     *
     * @param ctx  Script context
     * @param name Variable name
     * @return Value (BAV_VALUE_NIL if not found)
     */
    BavValue bav_script_get_global(BavScriptContext* ctx, const char* name);

    /* =============================================================================
     * Native Function Binding
     * ============================================================================= */

    /**
     * Native function signature.
     *
     * @param ctx       Script context
     * @param args      Arguments passed from script
     * @param arg_count Number of arguments
     * @param user_data User-provided context
     * @return Result to return to script
     */
    typedef BavCallResult (*BavNativeFn)(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                         void* user_data);

    /**
     * Register a native function.
     *
     * @param ctx       Script context
     * @param name      Function name in script
     * @param fn        Native function pointer
     * @param user_data Context passed to function
     */
    void bav_script_register_function(BavScriptContext* ctx, const char* name, BavNativeFn fn,
                                      void* user_data);

    /**
     * Register a module of functions.
     *
     * @param ctx         Script context
     * @param module_name Module name (e.g., "math", "ecs")
     * @param functions   Array of function name/pointer pairs
     * @param count       Number of functions
     * @param user_data   Context passed to all functions
     */
    typedef struct BavNativeFnDef
    {
        const char* name;
        BavNativeFn fn;
    } BavNativeFnDef;

    void bav_script_register_module(BavScriptContext* ctx, const char* module_name,
                                    const BavNativeFnDef* functions, u32 count, void* user_data);

    /* =============================================================================
     * Language Plugin System (for future language support)
     * ============================================================================= */

    /**
     * Language plugin interface.
     * Implement this to add support for additional scripting languages.
     */
    typedef struct BavLanguagePlugin
    {
        const char* name;      /* Language name (e.g., "lua", "wren") */
        const char* extension; /* File extension (e.g., ".lua", ".wren") */

        /* Compilation */
        BavCompileResult (*compile)(const char* source, usize source_len,
                                    const BavCompileOptions* options);

        /* Context lifecycle */
        BavScriptContext* (*create_context)(const BavScriptContextConfig* config);
        void (*destroy_context)(BavScriptContext* ctx);

        /* Execution */
        BavResult (*load_script)(BavScriptContext* ctx, BavCompiledScript* script);
        BavCallResult (*call_function)(BavScriptContext* ctx, const char* name,
                                       const BavValue* args, u32 arg_count);
    } BavLanguagePlugin;

    /**
     * Register a language plugin.
     *
     * @param plugin Language plugin definition
     * @return BAV_OK on success
     */
    BavResult bav_scripting_register_language(const BavLanguagePlugin* plugin);

    /**
     * Get the default language plugin (Lua).
     */
    const BavLanguagePlugin* bav_scripting_get_default_language(void);

    /* =============================================================================
     * Hot Reload Support
     * ============================================================================= */

    /**
     * Hot reload a script file.
     * Attempts to preserve state where possible.
     *
     * @param ctx      Script context
     * @param filename File to reload
     * @return BAV_OK on success
     */
    BavResult bav_script_hot_reload(BavScriptContext* ctx, const char* filename);

    /**
     * Check if a script file needs reloading.
     *
     * @param ctx      Script context
     * @param filename File to check
     * @return true if file has changed since last load
     */
    b8 bav_script_needs_reload(BavScriptContext* ctx, const char* filename);

    /* =============================================================================
     * Compiler Infrastructure (Advanced)
     * ============================================================================= */

    /* These are exposed for custom language implementations */

    /**
     * Token types for the lexer.
     */
    typedef enum BavTokenType
    {
        BAV_TOKEN_EOF = 0,
        BAV_TOKEN_ERROR,

        /* Literals */
        BAV_TOKEN_NUMBER,
        BAV_TOKEN_STRING,
        BAV_TOKEN_IDENTIFIER,

        /* Keywords */
        BAV_TOKEN_AND,
        BAV_TOKEN_BREAK,
        BAV_TOKEN_DO,
        BAV_TOKEN_ELSE,
        BAV_TOKEN_ELSEIF,
        BAV_TOKEN_END,
        BAV_TOKEN_FALSE,
        BAV_TOKEN_FOR,
        BAV_TOKEN_FUNCTION,
        BAV_TOKEN_GOTO,
        BAV_TOKEN_IF,
        BAV_TOKEN_IN,
        BAV_TOKEN_LOCAL,
        BAV_TOKEN_NIL,
        BAV_TOKEN_NOT,
        BAV_TOKEN_OR,
        BAV_TOKEN_REPEAT,
        BAV_TOKEN_RETURN,
        BAV_TOKEN_THEN,
        BAV_TOKEN_TRUE,
        BAV_TOKEN_UNTIL,
        BAV_TOKEN_WHILE,

        /* Operators and punctuation */
        BAV_TOKEN_PLUS,
        BAV_TOKEN_MINUS,
        BAV_TOKEN_STAR,
        BAV_TOKEN_SLASH,
        BAV_TOKEN_PERCENT,
        BAV_TOKEN_CARET,
        BAV_TOKEN_HASH,
        BAV_TOKEN_AMPERSAND,
        BAV_TOKEN_TILDE,
        BAV_TOKEN_PIPE,
        BAV_TOKEN_LT_LT,
        BAV_TOKEN_GT_GT,
        BAV_TOKEN_SLASH_SLASH,
        BAV_TOKEN_EQ,
        BAV_TOKEN_NE,
        BAV_TOKEN_LE,
        BAV_TOKEN_GE,
        BAV_TOKEN_LT,
        BAV_TOKEN_GT,
        BAV_TOKEN_ASSIGN,
        BAV_TOKEN_LPAREN,
        BAV_TOKEN_RPAREN,
        BAV_TOKEN_LBRACE,
        BAV_TOKEN_RBRACE,
        BAV_TOKEN_LBRACKET,
        BAV_TOKEN_RBRACKET,
        BAV_TOKEN_DOUBLE_COLON,
        BAV_TOKEN_SEMICOLON,
        BAV_TOKEN_COLON,
        BAV_TOKEN_COMMA,
        BAV_TOKEN_DOT,
        BAV_TOKEN_DOT_DOT,
        BAV_TOKEN_DOT_DOT_DOT,
    } BavTokenType;

    /**
     * Token structure.
     */
    typedef struct BavToken
    {
        BavTokenType type;
        const char* start;
        u32 length;
        u32 line;
        u32 column;
    } BavToken;

    /**
     * Lexer state.
     */
    typedef struct BavLexer BavLexer;

    BavLexer* bav_lexer_create(const char* source, usize source_len);
    void bav_lexer_destroy(BavLexer* lexer);
    BavToken bav_lexer_next_token(BavLexer* lexer);
    BavToken bav_lexer_peek_token(BavLexer* lexer);

#ifdef __cplusplus
}
#endif

#endif /* BAV_SCRIPTING_H */
