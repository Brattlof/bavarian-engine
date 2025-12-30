/**
 * @file analyzer.c
 * @brief Lua Semantic Analyzer
 *
 * Performs semantic analysis on the AST:
 * - Name resolution and scope management
 * - Upvalue detection for closures
 * - Constant folding (basic)
 * - Loop context tracking (break validation)
 * - Return context tracking
 */

#include <bavarian/scripting.h>
#include <bavarian/types.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* =============================================================================
 * AST Node Types (must match parser.c)
 * ============================================================================= */

typedef enum BavAstNodeType
{
    AST_CHUNK,
    AST_BLOCK,
    AST_ASSIGN,
    AST_LOCAL,
    AST_IF,
    AST_WHILE,
    AST_REPEAT,
    AST_FOR_NUMERIC,
    AST_FOR_GENERIC,
    AST_FUNCTION_DEF,
    AST_LOCAL_FUNCTION,
    AST_RETURN,
    AST_BREAK,
    AST_GOTO,
    AST_LABEL,
    AST_DO,
    AST_CALL_STMT,
    AST_NIL,
    AST_TRUE,
    AST_FALSE,
    AST_NUMBER,
    AST_STRING,
    AST_VARARG,
    AST_FUNCTION_EXPR,
    AST_TABLE,
    AST_TABLE_FIELD,
    AST_BINARY_OP,
    AST_UNARY_OP,
    AST_INDEX,
    AST_FIELD,
    AST_CALL,
    AST_METHOD_CALL,
    AST_VAR,
} BavAstNodeType;

/* Forward declaration for AST node */
typedef struct BavAstNode BavAstNode;

/* =============================================================================
 * Variable Resolution Info
 * ============================================================================= */

typedef enum VarLocation
{
    VAR_GLOBAL,  /* Global variable */
    VAR_LOCAL,   /* Local in current scope */
    VAR_UPVALUE, /* Captured from enclosing scope */
} VarLocation;

typedef struct VarInfo
{
    VarLocation location;
    u32 index; /* Local slot or upvalue index */
} VarInfo;

/* =============================================================================
 * Scope Management
 * ============================================================================= */

typedef struct Local
{
    const char* name;
    usize name_len;
    i32 depth;
    b8 is_captured; /* Used as upvalue by inner function */
    b8 is_const;    /* Constant (from const analysis) */
} Local;

typedef struct Upvalue
{
    u32 index;
    b8 is_local; /* Captured from enclosing local vs upvalue chain */
} Upvalue;

typedef struct Label
{
    const char* name;
    usize name_len;
    u32 line;
    b8 defined;
} Label;

typedef struct Scope
{
    struct Scope* enclosing;

    /* Locals */
    Local* locals;
    u32 local_count;
    u32 local_capacity;

    /* Upvalues */
    Upvalue* upvalues;
    u32 upvalue_count;
    u32 upvalue_capacity;

    /* Labels for goto */
    Label* labels;
    u32 label_count;
    u32 label_capacity;

    i32 depth;
    b8 is_function; /* Function scope (new upvalue context) */
    b8 is_loop;     /* Loop scope (break allowed) */
    b8 is_vararg;   /* Vararg function */
} Scope;

/* =============================================================================
 * Analyzer State
 * ============================================================================= */

typedef struct Analyzer
{
    Scope* current_scope;
    BavCompileError* errors;
    u32 error_count;
    u32 error_capacity;
    b8 in_loop;
    u32 loop_depth;
} Analyzer;

/* =============================================================================
 * Error Reporting
 * ============================================================================= */

static void analyzer_error(Analyzer* analyzer, u32 line, u32 column, const char* message)
{
    if (analyzer->error_count >= analyzer->error_capacity)
    {
        u32 new_cap = analyzer->error_capacity * 2;
        if (new_cap == 0)
            new_cap = 8;
        BavCompileError* new_errors = realloc(analyzer->errors, new_cap * sizeof(BavCompileError));
        if (!new_errors)
            return;
        analyzer->errors = new_errors;
        analyzer->error_capacity = new_cap;
    }

    BavCompileError* err = &analyzer->errors[analyzer->error_count++];
    err->message = message;
    err->filename = NULL;
    err->line = line;
    err->column = column;
    err->length = 0;
}

/* =============================================================================
 * Scope Operations
 * ============================================================================= */

static Scope* scope_create(Scope* enclosing, b8 is_function)
{
    Scope* scope = calloc(1, sizeof(Scope));
    if (!scope)
        return NULL;

    scope->enclosing = enclosing;
    scope->depth = enclosing ? enclosing->depth + 1 : 0;
    scope->is_function = is_function;
    scope->is_loop = 0;
    scope->is_vararg = 0;

    scope->local_capacity = 16;
    scope->locals = calloc(scope->local_capacity, sizeof(Local));

    scope->upvalue_capacity = 16;
    scope->upvalues = calloc(scope->upvalue_capacity, sizeof(Upvalue));

    scope->label_capacity = 8;
    scope->labels = calloc(scope->label_capacity, sizeof(Label));

    return scope;
}

static void scope_destroy(Scope* scope)
{
    if (!scope)
        return;
    free(scope->locals);
    free(scope->upvalues);
    free(scope->labels);
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
    local->is_const = 0;
}

static i32 scope_resolve_local(Scope* scope, const char* name, usize name_len)
{
    if (!scope)
        return -1;

    /* Search backwards to find most recent definition */
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

static i32 scope_add_upvalue(Scope* scope, u32 index, b8 is_local)
{
    /* Check if upvalue already exists */
    for (u32 i = 0; i < scope->upvalue_count; i++)
    {
        Upvalue* uv = &scope->upvalues[i];
        if (uv->index == index && uv->is_local == is_local)
        {
            return (i32)i;
        }
    }

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
    upvalue->index = index;
    upvalue->is_local = is_local;
    return (i32)scope->upvalue_count++;
}

static i32 scope_resolve_upvalue(Scope* scope, const char* name, usize name_len)
{
    if (!scope || !scope->enclosing)
        return -1;

    /* Try to find in enclosing local scope */
    i32 local = scope_resolve_local(scope->enclosing, name, name_len);
    if (local != -1)
    {
        /* Mark as captured */
        scope->enclosing->locals[local].is_captured = 1;
        return scope_add_upvalue(scope, (u32)local, 1);
    }

    /* Try enclosing upvalue chain */
    i32 upvalue = scope_resolve_upvalue(scope->enclosing, name, name_len);
    if (upvalue != -1)
    {
        return scope_add_upvalue(scope, (u32)upvalue, 0);
    }

    return -1;
}

/* Resolve a variable name - returns location and index */
static VarInfo resolve_variable(Analyzer* analyzer, const char* name, usize name_len)
{
    VarInfo info;
    info.location = VAR_GLOBAL;
    info.index = 0;

    Scope* scope = analyzer->current_scope;

    /* Check local first */
    i32 local = scope_resolve_local(scope, name, name_len);
    if (local != -1)
    {
        info.location = VAR_LOCAL;
        info.index = (u32)local;
        return info;
    }

    /* Check upvalue (only in function scopes) */
    if (scope->is_function)
    {
        i32 upvalue = scope_resolve_upvalue(scope, name, name_len);
        if (upvalue != -1)
        {
            info.location = VAR_UPVALUE;
            info.index = (u32)upvalue;
            return info;
        }
    }
    else
    {
        /* Walk up to find enclosing function scope */
        Scope* func_scope = scope;
        while (func_scope && !func_scope->is_function)
        {
            func_scope = func_scope->enclosing;
        }

        if (func_scope)
        {
            /* Check locals in enclosing scopes up to function boundary */
            Scope* s = scope->enclosing;
            while (s && s != func_scope)
            {
                local = scope_resolve_local(s, name, name_len);
                if (local != -1)
                {
                    info.location = VAR_LOCAL;
                    info.index = (u32)local;
                    return info;
                }
                s = s->enclosing;
            }
        }
    }

    /* Default to global */
    return info;
}

/* Add a label to current scope */
static void scope_add_label(Scope* scope, const char* name, usize name_len, u32 line, b8 defined)
{
    if (!scope)
        return;

    /* Check if label already exists */
    for (u32 i = 0; i < scope->label_count; i++)
    {
        Label* lbl = &scope->labels[i];
        if (lbl->name_len == name_len && memcmp(lbl->name, name, name_len) == 0)
        {
            if (defined)
                lbl->defined = 1;
            return;
        }
    }

    if (scope->label_count >= scope->label_capacity)
    {
        u32 new_cap = scope->label_capacity * 2;
        Label* new_labels = realloc(scope->labels, new_cap * sizeof(Label));
        if (!new_labels)
            return;
        scope->labels = new_labels;
        scope->label_capacity = new_cap;
    }

    Label* label = &scope->labels[scope->label_count++];
    label->name = name;
    label->name_len = name_len;
    label->line = line;
    label->defined = defined;
}

/* =============================================================================
 * AST Walking - Forward Declarations
 * ============================================================================= */

static void analyze_node(Analyzer* analyzer, BavAstNode* node);
static void analyze_expression(Analyzer* analyzer, BavAstNode* node);
static void analyze_statement(Analyzer* analyzer, BavAstNode* node);
static void analyze_block(Analyzer* analyzer, BavAstNode* node);

/* =============================================================================
 * Expression Analysis
 * ============================================================================= */

/* External AST node structure access - we need to match parser's layout */
/* This is a workaround since the AST types are defined in parser.c */
/* In a proper implementation, these would be in a shared header */

/* Access node fields via offsets - assumes same struct layout as parser */
#define NODE_TYPE(n) ((n)->type)
#define NODE_LINE(n) (*((u32*)((char*)(n) + sizeof(BavAstNodeType))))
#define NODE_COLUMN(n) (*((u32*)((char*)(n) + sizeof(BavAstNodeType) + sizeof(u32))))

/* For now, we define a minimal compatible structure */
struct BavAstNode
{
    BavAstNodeType type;
    u32 line;
    u32 column;

    union
    {
        struct
        {
            f64 value;
        } number;
        struct
        {
            const char* data;
            usize length;
        } string;
        struct
        {
            const char* name;
            usize name_len;
        } var;
        struct
        {
            BavTokenType op;
            BavAstNode* left;
            BavAstNode* right;
        } binary;
        struct
        {
            BavTokenType op;
            BavAstNode* operand;
        } unary;
        struct
        {
            BavAstNode** statements;
            u32 count;
            u32 capacity;
        } block;
        struct
        {
            BavAstNode* callee;
            BavAstNode** args;
            u32 arg_count;
        } call;
        struct
        {
            BavAstNode* object;
            const char* method;
            usize method_len;
            BavAstNode** args;
            u32 arg_count;
        } method_call;
        struct
        {
            BavAstNode** targets;
            u32 target_count;
            BavAstNode** values;
            u32 value_count;
        } assign;
        struct
        {
            const char** names;
            usize* name_lens;
            u32 name_count;
            BavAstNode** values;
            u32 value_count;
        } local;
        struct
        {
            BavAstNode* condition;
            BavAstNode* then_block;
            BavAstNode** elseif_conds;
            BavAstNode** elseif_blocks;
            u32 elseif_count;
            BavAstNode* else_block;
        } if_stmt;
        struct
        {
            BavAstNode* condition;
            BavAstNode* body;
        } while_loop;
        struct
        {
            BavAstNode* body;
            BavAstNode* condition;
        } repeat_loop;
        struct
        {
            const char* var_name;
            usize var_len;
            BavAstNode* start;
            BavAstNode* limit;
            BavAstNode* step;
            BavAstNode* body;
        } for_numeric;
        struct
        {
            const char** var_names;
            usize* var_lens;
            u32 var_count;
            BavAstNode** iterators;
            u32 iterator_count;
            BavAstNode* body;
        } for_generic;
        struct
        {
            const char* name;
            usize name_len;
            const char** params;
            usize* param_lens;
            u32 param_count;
            b8 is_vararg;
            b8 is_local;
            b8 is_method;
            BavAstNode* body;
        } function;
        struct
        {
            BavAstNode** values;
            u32 count;
        } return_stmt;
        struct
        {
            BavAstNode** fields;
            u32 field_count;
            u32 field_capacity;
        } table;
        struct
        {
            BavAstNode* key;
            BavAstNode* value;
        } table_field;
        struct
        {
            BavAstNode* object;
            BavAstNode* key;
        } index;
        struct
        {
            BavAstNode* object;
            const char* name;
            usize name_len;
        } field;
        struct
        {
            const char* label;
            usize label_len;
        } goto_stmt;
        struct
        {
            const char* name;
            usize name_len;
        } label;
    };
};

static void analyze_expression(Analyzer* analyzer, BavAstNode* node)
{
    if (!node)
        return;

    switch (node->type)
    {
        case AST_NIL:
        case AST_TRUE:
        case AST_FALSE:
        case AST_NUMBER:
        case AST_STRING:
            /* Literals - nothing to analyze */
            break;

        case AST_VARARG:
            /* Check if we're in a vararg function */
            {
                Scope* s = analyzer->current_scope;
                while (s)
                {
                    if (s->is_function)
                    {
                        if (!s->is_vararg)
                        {
                            analyzer_error(analyzer, node->line, node->column,
                                           "Cannot use '...' outside a vararg function");
                        }
                        break;
                    }
                    s = s->enclosing;
                }
            }
            break;

        case AST_VAR:
            /* Resolve variable - this updates scope info for upvalues */
            resolve_variable(analyzer, node->var.name, node->var.name_len);
            break;

        case AST_BINARY_OP:
            analyze_expression(analyzer, node->binary.left);
            analyze_expression(analyzer, node->binary.right);
            break;

        case AST_UNARY_OP:
            analyze_expression(analyzer, node->unary.operand);
            break;

        case AST_INDEX:
            analyze_expression(analyzer, node->index.object);
            analyze_expression(analyzer, node->index.key);
            break;

        case AST_FIELD:
            analyze_expression(analyzer, node->field.object);
            break;

        case AST_CALL:
            analyze_expression(analyzer, node->call.callee);
            for (u32 i = 0; i < node->call.arg_count; i++)
            {
                analyze_expression(analyzer, node->call.args[i]);
            }
            break;

        case AST_METHOD_CALL:
            analyze_expression(analyzer, node->method_call.object);
            for (u32 i = 0; i < node->method_call.arg_count; i++)
            {
                analyze_expression(analyzer, node->method_call.args[i]);
            }
            break;

        case AST_TABLE:
            for (u32 i = 0; i < node->table.field_count; i++)
            {
                analyze_expression(analyzer, node->table.fields[i]);
            }
            break;

        case AST_TABLE_FIELD:
            if (node->table_field.key)
            {
                analyze_expression(analyzer, node->table_field.key);
            }
            analyze_expression(analyzer, node->table_field.value);
            break;

        case AST_FUNCTION_EXPR:
            /* Anonymous function - create new function scope */
            {
                Scope* func_scope = scope_create(analyzer->current_scope, 1);
                if (!func_scope)
                    return;

                func_scope->is_vararg = node->function.is_vararg;

                /* Add parameters as locals */
                for (u32 i = 0; i < node->function.param_count; i++)
                {
                    scope_add_local(func_scope, node->function.params[i],
                                    node->function.param_lens[i]);
                }

                /* Analyze body */
                Scope* old_scope = analyzer->current_scope;
                analyzer->current_scope = func_scope;
                analyze_block(analyzer, node->function.body);
                analyzer->current_scope = old_scope;

                scope_destroy(func_scope);
            }
            break;

        default:
            break;
    }
}

/* =============================================================================
 * Statement Analysis
 * ============================================================================= */

static void analyze_statement(Analyzer* analyzer, BavAstNode* node)
{
    if (!node)
        return;

    switch (node->type)
    {
        case AST_ASSIGN:
            /* Analyze targets (for upvalue tracking) */
            for (u32 i = 0; i < node->assign.target_count; i++)
            {
                analyze_expression(analyzer, node->assign.targets[i]);
            }
            /* Analyze values */
            for (u32 i = 0; i < node->assign.value_count; i++)
            {
                analyze_expression(analyzer, node->assign.values[i]);
            }
            break;

        case AST_LOCAL:
            /* Analyze initializers first (before adding locals) */
            for (u32 i = 0; i < node->local.value_count; i++)
            {
                analyze_expression(analyzer, node->local.values[i]);
            }
            /* Add locals to current scope */
            for (u32 i = 0; i < node->local.name_count; i++)
            {
                scope_add_local(analyzer->current_scope, node->local.names[i],
                                node->local.name_lens[i]);
            }
            break;

        case AST_IF:
            analyze_expression(analyzer, node->if_stmt.condition);
            analyze_block(analyzer, node->if_stmt.then_block);

            for (u32 i = 0; i < node->if_stmt.elseif_count; i++)
            {
                analyze_expression(analyzer, node->if_stmt.elseif_conds[i]);
                analyze_block(analyzer, node->if_stmt.elseif_blocks[i]);
            }

            if (node->if_stmt.else_block)
            {
                analyze_block(analyzer, node->if_stmt.else_block);
            }
            break;

        case AST_WHILE:
        {
            u32 old_loop_depth = analyzer->loop_depth;
            analyzer->loop_depth++;

            analyze_expression(analyzer, node->while_loop.condition);

            /* Create loop scope */
            Scope* loop_scope = scope_create(analyzer->current_scope, 0);
            if (loop_scope)
            {
                loop_scope->is_loop = 1;
                Scope* old_scope = analyzer->current_scope;
                analyzer->current_scope = loop_scope;

                analyze_block(analyzer, node->while_loop.body);

                analyzer->current_scope = old_scope;
                scope_destroy(loop_scope);
            }

            analyzer->loop_depth = old_loop_depth;
        }
        break;

        case AST_REPEAT:
        {
            u32 old_loop_depth = analyzer->loop_depth;
            analyzer->loop_depth++;

            Scope* loop_scope = scope_create(analyzer->current_scope, 0);
            if (loop_scope)
            {
                loop_scope->is_loop = 1;
                Scope* old_scope = analyzer->current_scope;
                analyzer->current_scope = loop_scope;

                analyze_block(analyzer, node->repeat_loop.body);
                /* Condition can reference locals from body */
                analyze_expression(analyzer, node->repeat_loop.condition);

                analyzer->current_scope = old_scope;
                scope_destroy(loop_scope);
            }

            analyzer->loop_depth = old_loop_depth;
        }
        break;

        case AST_FOR_NUMERIC:
        {
            u32 old_loop_depth = analyzer->loop_depth;
            analyzer->loop_depth++;

            /* Analyze bounds (in outer scope) */
            analyze_expression(analyzer, node->for_numeric.start);
            analyze_expression(analyzer, node->for_numeric.limit);
            if (node->for_numeric.step)
            {
                analyze_expression(analyzer, node->for_numeric.step);
            }

            /* Create loop scope with iteration variable */
            Scope* loop_scope = scope_create(analyzer->current_scope, 0);
            if (loop_scope)
            {
                loop_scope->is_loop = 1;
                scope_add_local(loop_scope, node->for_numeric.var_name, node->for_numeric.var_len);

                Scope* old_scope = analyzer->current_scope;
                analyzer->current_scope = loop_scope;

                analyze_block(analyzer, node->for_numeric.body);

                analyzer->current_scope = old_scope;
                scope_destroy(loop_scope);
            }

            analyzer->loop_depth = old_loop_depth;
        }
        break;

        case AST_FOR_GENERIC:
        {
            u32 old_loop_depth = analyzer->loop_depth;
            analyzer->loop_depth++;

            /* Analyze iterators (in outer scope) */
            for (u32 i = 0; i < node->for_generic.iterator_count; i++)
            {
                analyze_expression(analyzer, node->for_generic.iterators[i]);
            }

            /* Create loop scope with iteration variables */
            Scope* loop_scope = scope_create(analyzer->current_scope, 0);
            if (loop_scope)
            {
                loop_scope->is_loop = 1;
                for (u32 i = 0; i < node->for_generic.var_count; i++)
                {
                    scope_add_local(loop_scope, node->for_generic.var_names[i],
                                    node->for_generic.var_lens[i]);
                }

                Scope* old_scope = analyzer->current_scope;
                analyzer->current_scope = loop_scope;

                analyze_block(analyzer, node->for_generic.body);

                analyzer->current_scope = old_scope;
                scope_destroy(loop_scope);
            }

            analyzer->loop_depth = old_loop_depth;
        }
        break;

        case AST_FUNCTION_DEF:
        case AST_LOCAL_FUNCTION:
        {
            /* For local function, add name before analyzing body (allows recursion) */
            if (node->type == AST_LOCAL_FUNCTION)
            {
                scope_add_local(analyzer->current_scope, node->function.name,
                                node->function.name_len);
            }

            /* Create function scope */
            Scope* func_scope = scope_create(analyzer->current_scope, 1);
            if (!func_scope)
                break;

            func_scope->is_vararg = node->function.is_vararg;

            /* Add self for methods */
            if (node->function.is_method)
            {
                scope_add_local(func_scope, "self", 4);
            }

            /* Add parameters as locals */
            for (u32 i = 0; i < node->function.param_count; i++)
            {
                scope_add_local(func_scope, node->function.params[i], node->function.param_lens[i]);
            }

            /* Analyze body */
            Scope* old_scope = analyzer->current_scope;
            analyzer->current_scope = func_scope;
            analyze_block(analyzer, node->function.body);
            analyzer->current_scope = old_scope;

            scope_destroy(func_scope);
        }
        break;

        case AST_RETURN:
            for (u32 i = 0; i < node->return_stmt.count; i++)
            {
                analyze_expression(analyzer, node->return_stmt.values[i]);
            }
            break;

        case AST_BREAK:
            /* Check if we're in a loop */
            if (analyzer->loop_depth == 0)
            {
                analyzer_error(analyzer, node->line, node->column,
                               "Break statement outside of loop");
            }
            break;

        case AST_DO:
        {
            /* Create new scope for do block */
            Scope* do_scope = scope_create(analyzer->current_scope, 0);
            if (do_scope)
            {
                Scope* old_scope = analyzer->current_scope;
                analyzer->current_scope = do_scope;

                /* do block uses the block union member */
                for (u32 i = 0; i < node->block.count; i++)
                {
                    analyze_statement(analyzer, node->block.statements[i]);
                }

                analyzer->current_scope = old_scope;
                scope_destroy(do_scope);
            }
        }
        break;

        case AST_GOTO:
            /* Record goto for later label resolution */
            scope_add_label(analyzer->current_scope, node->goto_stmt.label,
                            node->goto_stmt.label_len, node->line, 0);
            break;

        case AST_LABEL:
            /* Record label definition */
            scope_add_label(analyzer->current_scope, node->label.name, node->label.name_len,
                            node->line, 1);
            break;

        case AST_CALL_STMT:
            /* Function call as statement */
            analyze_expression(analyzer, node->call.callee);
            for (u32 i = 0; i < node->call.arg_count; i++)
            {
                analyze_expression(analyzer, node->call.args[i]);
            }
            break;

        default:
            /* Expression statement - analyze as expression */
            analyze_expression(analyzer, node);
            break;
    }
}

/* =============================================================================
 * Block Analysis
 * ============================================================================= */

static void analyze_block(Analyzer* analyzer, BavAstNode* node)
{
    if (!node)
        return;

    /* Create block scope */
    Scope* block_scope = scope_create(analyzer->current_scope, 0);
    if (!block_scope)
        return;

    Scope* old_scope = analyzer->current_scope;
    analyzer->current_scope = block_scope;

    /* Analyze all statements */
    for (u32 i = 0; i < node->block.count; i++)
    {
        analyze_statement(analyzer, node->block.statements[i]);
    }

    /* Check for undefined labels */
    for (u32 i = 0; i < block_scope->label_count; i++)
    {
        Label* lbl = &block_scope->labels[i];
        if (!lbl->defined)
        {
            analyzer_error(analyzer, lbl->line, 0, "Undefined label in goto statement");
        }
    }

    analyzer->current_scope = old_scope;
    scope_destroy(block_scope);
}

/* =============================================================================
 * Top-Level Analysis
 * ============================================================================= */

static void analyze_node(Analyzer* analyzer, BavAstNode* node)
{
    if (!node)
        return;

    switch (node->type)
    {
        case AST_CHUNK:
        case AST_BLOCK:
            analyze_block(analyzer, node);
            break;

        default:
            if (node->type >= AST_ASSIGN && node->type <= AST_CALL_STMT)
            {
                analyze_statement(analyzer, node);
            }
            else
            {
                analyze_expression(analyzer, node);
            }
            break;
    }
}

/* =============================================================================
 * Public API
 * ============================================================================= */

/**
 * Analyze an AST for semantic correctness.
 *
 * @param ast         The AST root node
 * @param errors      Output: array of semantic errors
 * @param error_count Output: number of errors
 * @return 1 on success (no errors), 0 on failure
 */
b8 bav_analyze_lua(BavAstNode* ast, BavCompileError** errors, u32* error_count)
{
    Analyzer analyzer;
    analyzer.errors = NULL;
    analyzer.error_count = 0;
    analyzer.error_capacity = 0;
    analyzer.loop_depth = 0;

    /* Create global scope */
    analyzer.current_scope = scope_create(NULL, 1);
    if (!analyzer.current_scope)
    {
        *errors = NULL;
        *error_count = 0;
        return 0;
    }

    /* Main chunk is a vararg function */
    analyzer.current_scope->is_vararg = 1;

    /* Analyze the AST */
    analyze_node(&analyzer, ast);

    /* Clean up */
    scope_destroy(analyzer.current_scope);

    *errors = analyzer.errors;
    *error_count = analyzer.error_count;

    return analyzer.error_count == 0;
}

/**
 * Free analysis errors.
 */
void bav_analysis_errors_free(BavCompileError* errors)
{
    free(errors);
}
