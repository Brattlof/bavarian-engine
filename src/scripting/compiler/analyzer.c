/**
 * @file analyzer.c
 * @brief Lua Semantic Analyzer
 *
 * Performs semantic analysis on the AST:
 * - Name resolution and scope management
 * - Type inference (optional for Lua)
 * - Constant folding
 * - Upvalue detection
 */

#include <bavarian/scripting.h>
#include <bavarian/types.h>
#include <stdlib.h>
#include <string.h>

/* =============================================================================
 * Scope Management
 * ============================================================================= */

typedef struct Local
{
    const char* name;
    usize name_len;
    i32 depth;
    b8 is_captured; /* Used as upvalue */
} Local;

typedef struct Upvalue
{
    u32 index;
    b8 is_local; /* Captured from enclosing local vs upvalue */
} Upvalue;

typedef struct Scope
{
    struct Scope* enclosing;
    Local* locals;
    u32 local_count;
    u32 local_capacity;
    Upvalue* upvalues;
    u32 upvalue_count;
    u32 upvalue_capacity;
    i32 depth;
} Scope;

typedef struct Analyzer
{
    Scope* current_scope;
} Analyzer;

/* =============================================================================
 * Scope Operations
 * ============================================================================= */

static Scope* scope_create(Scope* enclosing)
{
    Scope* scope = calloc(1, sizeof(Scope));
    if (!scope)
        return NULL;

    scope->enclosing = enclosing;
    scope->depth = enclosing ? enclosing->depth + 1 : 0;
    scope->local_capacity = 16;
    scope->locals = calloc(scope->local_capacity, sizeof(Local));
    scope->upvalue_capacity = 16;
    scope->upvalues = calloc(scope->upvalue_capacity, sizeof(Upvalue));

    return scope;
}

static void scope_destroy(Scope* scope)
{
    if (!scope)
        return;
    free(scope->locals);
    free(scope->upvalues);
    free(scope);
}

static void scope_add_local(Scope* scope, const char* name, usize name_len)
{
    if (!scope)
        return;

    if (scope->local_count >= scope->local_capacity)
    {
        u32 new_cap = scope->local_capacity * 2;
        Local* new_locals = realloc(scope->locals, new_cap * sizeof(Local));
        if (!new_locals)
            return;
        scope->locals = new_locals;
        scope->local_capacity = new_cap;
    }

    Local* local = &scope->locals[scope->local_count++];
    local->name = name;
    local->name_len = name_len;
    local->depth = scope->depth;
    local->is_captured = 0;
}

static i32 scope_resolve_local(Scope* scope, const char* name, usize name_len)
{
    if (!scope)
        return -1;

    for (i32 i = (i32)scope->local_count - 1; i >= 0; i--)
    {
        Local* local = &scope->locals[i];
        if (local->name_len == name_len && memcmp(local->name, name, name_len) == 0)
        {
            return i;
        }
    }

    return -1;
}

static i32 scope_resolve_upvalue(Scope* scope, const char* name, usize name_len)
{
    if (!scope || !scope->enclosing)
        return -1;

    /* Try to find in enclosing local */
    i32 local = scope_resolve_local(scope->enclosing, name, name_len);
    if (local != -1)
    {
        scope->enclosing->locals[local].is_captured = 1;

        /* Add upvalue */
        if (scope->upvalue_count >= scope->upvalue_capacity)
        {
            u32 new_cap = scope->upvalue_capacity * 2;
            Upvalue* new_upvalues = realloc(scope->upvalues, new_cap * sizeof(Upvalue));
            if (!new_upvalues)
                return -1;
            scope->upvalues = new_upvalues;
            scope->upvalue_capacity = new_cap;
        }

        Upvalue* upvalue = &scope->upvalues[scope->upvalue_count];
        upvalue->index = (u32)local;
        upvalue->is_local = 1;
        return (i32)scope->upvalue_count++;
    }

    /* Try enclosing upvalue */
    i32 upvalue = scope_resolve_upvalue(scope->enclosing, name, name_len);
    if (upvalue != -1)
    {
        if (scope->upvalue_count >= scope->upvalue_capacity)
        {
            u32 new_cap = scope->upvalue_capacity * 2;
            Upvalue* new_upvalues = realloc(scope->upvalues, new_cap * sizeof(Upvalue));
            if (!new_upvalues)
                return -1;
            scope->upvalues = new_upvalues;
            scope->upvalue_capacity = new_cap;
        }

        Upvalue* uv = &scope->upvalues[scope->upvalue_count];
        uv->index = (u32)upvalue;
        uv->is_local = 0;
        return (i32)scope->upvalue_count++;
    }

    return -1;
}

/* =============================================================================
 * Constant Folding
 * ============================================================================= */

/* Placeholder - would implement constant evaluation for compile-time optimization */

/* =============================================================================
 * Analysis Entry Point
 * ============================================================================= */

/* Analyzer is internal - used by compiler pipeline */
