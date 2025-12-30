/**
 * @file parser.c
 * @brief Lua Parser - AST Construction
 *
 * Converts token stream into an Abstract Syntax Tree.
 * This is the second phase of the compilation pipeline.
 *
 * Uses Pratt parsing for expressions with proper precedence handling.
 * Lua's operator precedence (lowest to highest):
 *   or
 *   and
 *   < > <= >= ~= ==
 *   |
 *   ~
 *   &
 *   << >>
 *   ..
 *   + -
 *   * / // %
 *   unary: not # - ~
 *   ^
 *   function calls, indexing
 */

#include <bavarian/scripting.h>
#include <bavarian/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =============================================================================
 * AST Node Types
 * ============================================================================= */

typedef enum BavAstNodeType
{
    AST_CHUNK,
    AST_BLOCK,

    /* Statements */
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

    /* Expressions */
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

typedef struct BavAstNode BavAstNode;

struct BavAstNode
{
    BavAstNodeType type;
    u32 line;
    u32 column;

    union
    {
        /* Number literal */
        struct
        {
            f64 value;
        } number;

        /* String literal */
        struct
        {
            const char* data;
            usize length;
        } string;

        /* Variable reference */
        struct
        {
            const char* name;
            usize name_len;
        } var;

        /* Binary operation */
        struct
        {
            BavTokenType op;
            BavAstNode* left;
            BavAstNode* right;
        } binary;

        /* Unary operation */
        struct
        {
            BavTokenType op;
            BavAstNode* operand;
        } unary;

        /* Block of statements */
        struct
        {
            BavAstNode** statements;
            u32 count;
            u32 capacity;
        } block;

        /* Function call */
        struct
        {
            BavAstNode* callee;
            BavAstNode** args;
            u32 arg_count;
        } call;

        /* Method call (a:b(args)) */
        struct
        {
            BavAstNode* object;
            const char* method;
            usize method_len;
            BavAstNode** args;
            u32 arg_count;
        } method_call;

        /* Assignment */
        struct
        {
            BavAstNode** targets;
            u32 target_count;
            BavAstNode** values;
            u32 value_count;
        } assign;

        /* Local declaration */
        struct
        {
            const char** names;
            usize* name_lens;
            u32 name_count;
            BavAstNode** values;
            u32 value_count;
        } local;

        /* If statement */
        struct
        {
            BavAstNode* condition;
            BavAstNode* then_block;
            BavAstNode** elseif_conds;
            BavAstNode** elseif_blocks;
            u32 elseif_count;
            BavAstNode* else_block;
        } if_stmt;

        /* While loop */
        struct
        {
            BavAstNode* condition;
            BavAstNode* body;
        } while_loop;

        /* Repeat-until loop */
        struct
        {
            BavAstNode* body;
            BavAstNode* condition;
        } repeat_loop;

        /* For numeric loop */
        struct
        {
            const char* var_name;
            usize var_len;
            BavAstNode* start;
            BavAstNode* limit;
            BavAstNode* step; /* NULL if not specified */
            BavAstNode* body;
        } for_numeric;

        /* For generic loop */
        struct
        {
            const char** var_names;
            usize* var_lens;
            u32 var_count;
            BavAstNode** iterators;
            u32 iterator_count;
            BavAstNode* body;
        } for_generic;

        /* Function definition */
        struct
        {
            const char* name; /* NULL for anonymous */
            usize name_len;
            const char** params;
            usize* param_lens;
            u32 param_count;
            b8 is_vararg;
            b8 is_local;
            b8 is_method; /* Has implicit self */
            BavAstNode* body;
        } function;

        /* Return statement */
        struct
        {
            BavAstNode** values;
            u32 count;
        } return_stmt;

        /* Table constructor */
        struct
        {
            BavAstNode** fields;
            u32 field_count;
            u32 field_capacity;
        } table;

        /* Table field [key] = value or name = value or just value */
        struct
        {
            BavAstNode* key; /* NULL for array-style */
            BavAstNode* value;
        } table_field;

        /* Index expression (a[b]) */
        struct
        {
            BavAstNode* object;
            BavAstNode* key;
        } index;

        /* Field access (a.b) */
        struct
        {
            BavAstNode* object;
            const char* name;
            usize name_len;
        } field;

        /* Goto */
        struct
        {
            const char* label;
            usize label_len;
        } goto_stmt;

        /* Label */
        struct
        {
            const char* name;
            usize name_len;
        } label;
    };
};

/* =============================================================================
 * Parser State
 * ============================================================================= */

typedef struct BavParser
{
    BavLexer* lexer;
    BavToken current;
    BavToken previous;
    BavCompileError* errors;
    u32 error_count;
    u32 error_capacity;
    b8 had_error;
    b8 panic_mode;
} BavParser;

/* =============================================================================
 * Error Handling
 * ============================================================================= */

static void error_at(BavParser* parser, BavToken* token, const char* message)
{
    if (parser->panic_mode)
        return;
    parser->panic_mode = 1;
    parser->had_error = 1;

    if (parser->error_count >= parser->error_capacity)
    {
        u32 new_cap = parser->error_capacity * 2;
        if (new_cap == 0)
            new_cap = 8;
        BavCompileError* new_errors = realloc(parser->errors, new_cap * sizeof(BavCompileError));
        if (!new_errors)
            return;
        parser->errors = new_errors;
        parser->error_capacity = new_cap;
    }

    BavCompileError* err = &parser->errors[parser->error_count++];
    err->message = message;
    err->filename = NULL;
    err->line = token->line;
    err->column = token->column;
    err->length = token->length;
}

static void error(BavParser* parser, const char* message)
{
    error_at(parser, &parser->previous, message);
}

static void error_at_current(BavParser* parser, const char* message)
{
    error_at(parser, &parser->current, message);
}

/* =============================================================================
 * Parser Utilities
 * ============================================================================= */

static void parser_advance(BavParser* parser)
{
    parser->previous = parser->current;

    for (;;)
    {
        parser->current = bav_lexer_next_token(parser->lexer);
        if (parser->current.type != BAV_TOKEN_ERROR)
            break;
        error_at_current(parser, parser->current.start);
    }
}

static void consume(BavParser* parser, BavTokenType type, const char* message)
{
    if (parser->current.type == type)
    {
        parser_advance(parser);
        return;
    }
    error_at_current(parser, message);
}

static b8 check(BavParser* parser, BavTokenType type)
{
    return parser->current.type == type;
}

static b8 match(BavParser* parser, BavTokenType type)
{
    if (!check(parser, type))
        return 0;
    parser_advance(parser);
    return 1;
}

/* Synchronize after an error - skip tokens until we reach a statement boundary */
static void synchronize(BavParser* parser)
{
    parser->panic_mode = 0;

    while (parser->current.type != BAV_TOKEN_EOF)
    {
        /* Stop at statement-ending semicolons */
        if (parser->previous.type == BAV_TOKEN_SEMICOLON)
            return;

        /* Stop at statement-starting keywords */
        switch (parser->current.type)
        {
            case BAV_TOKEN_FUNCTION:
            case BAV_TOKEN_LOCAL:
            case BAV_TOKEN_FOR:
            case BAV_TOKEN_IF:
            case BAV_TOKEN_WHILE:
            case BAV_TOKEN_REPEAT:
            case BAV_TOKEN_RETURN:
            case BAV_TOKEN_DO:
            case BAV_TOKEN_GOTO:
                return;
            default:
                break;
        }

        parser_advance(parser);
    }
}

/* Check if current token can start an expression */
static b8 is_expression_start(BavTokenType type)
{
    switch (type)
    {
        case BAV_TOKEN_NIL:
        case BAV_TOKEN_TRUE:
        case BAV_TOKEN_FALSE:
        case BAV_TOKEN_NUMBER:
        case BAV_TOKEN_STRING:
        case BAV_TOKEN_IDENTIFIER:
        case BAV_TOKEN_LPAREN:
        case BAV_TOKEN_LBRACE:
        case BAV_TOKEN_FUNCTION:
        case BAV_TOKEN_MINUS:
        case BAV_TOKEN_NOT:
        case BAV_TOKEN_HASH:
        case BAV_TOKEN_TILDE:
        case BAV_TOKEN_DOT_DOT_DOT:
            return 1;
        default:
            return 0;
    }
}

/* Check if current token ends a block */
static b8 is_block_end(BavTokenType type)
{
    switch (type)
    {
        case BAV_TOKEN_END:
        case BAV_TOKEN_ELSE:
        case BAV_TOKEN_ELSEIF:
        case BAV_TOKEN_UNTIL:
        case BAV_TOKEN_EOF:
            return 1;
        default:
            return 0;
    }
}

/* =============================================================================
 * AST Node Creation
 * ============================================================================= */

static BavAstNode* make_node(BavAstNodeType type, u32 line, u32 column)
{
    BavAstNode* node = calloc(1, sizeof(BavAstNode));
    if (!node)
        return NULL;
    node->type = type;
    node->line = line;
    node->column = column;
    return node;
}

static void free_node(BavAstNode* node)
{
    if (!node)
        return;

    switch (node->type)
    {
        case AST_BLOCK:
            for (u32 i = 0; i < node->block.count; i++)
                free_node(node->block.statements[i]);
            free(node->block.statements);
            break;

        case AST_BINARY_OP:
            free_node(node->binary.left);
            free_node(node->binary.right);
            break;

        case AST_UNARY_OP:
            free_node(node->unary.operand);
            break;

        case AST_CALL:
            free_node(node->call.callee);
            for (u32 i = 0; i < node->call.arg_count; i++)
                free_node(node->call.args[i]);
            free(node->call.args);
            break;

        case AST_METHOD_CALL:
            free_node(node->method_call.object);
            for (u32 i = 0; i < node->method_call.arg_count; i++)
                free_node(node->method_call.args[i]);
            free(node->method_call.args);
            break;

        case AST_INDEX:
            free_node(node->index.object);
            free_node(node->index.key);
            break;

        case AST_FIELD:
            free_node(node->field.object);
            break;

        case AST_TABLE:
            for (u32 i = 0; i < node->table.field_count; i++)
                free_node(node->table.fields[i]);
            free(node->table.fields);
            break;

        case AST_TABLE_FIELD:
            free_node(node->table_field.key);
            free_node(node->table_field.value);
            break;

        case AST_ASSIGN:
            for (u32 i = 0; i < node->assign.target_count; i++)
                free_node(node->assign.targets[i]);
            free(node->assign.targets);
            for (u32 i = 0; i < node->assign.value_count; i++)
                free_node(node->assign.values[i]);
            free(node->assign.values);
            break;

        case AST_LOCAL:
            free(node->local.names);
            free(node->local.name_lens);
            for (u32 i = 0; i < node->local.value_count; i++)
                free_node(node->local.values[i]);
            free(node->local.values);
            break;

        case AST_IF:
            free_node(node->if_stmt.condition);
            free_node(node->if_stmt.then_block);
            for (u32 i = 0; i < node->if_stmt.elseif_count; i++)
            {
                free_node(node->if_stmt.elseif_conds[i]);
                free_node(node->if_stmt.elseif_blocks[i]);
            }
            free(node->if_stmt.elseif_conds);
            free(node->if_stmt.elseif_blocks);
            free_node(node->if_stmt.else_block);
            break;

        case AST_WHILE:
            free_node(node->while_loop.condition);
            free_node(node->while_loop.body);
            break;

        case AST_REPEAT:
            free_node(node->repeat_loop.body);
            free_node(node->repeat_loop.condition);
            break;

        case AST_FOR_NUMERIC:
            free_node(node->for_numeric.start);
            free_node(node->for_numeric.limit);
            free_node(node->for_numeric.step);
            free_node(node->for_numeric.body);
            break;

        case AST_FOR_GENERIC:
            free(node->for_generic.var_names);
            free(node->for_generic.var_lens);
            for (u32 i = 0; i < node->for_generic.iterator_count; i++)
                free_node(node->for_generic.iterators[i]);
            free(node->for_generic.iterators);
            free_node(node->for_generic.body);
            break;

        case AST_FUNCTION_DEF:
        case AST_LOCAL_FUNCTION:
        case AST_FUNCTION_EXPR:
            free(node->function.params);
            free(node->function.param_lens);
            free_node(node->function.body);
            break;

        case AST_RETURN:
            for (u32 i = 0; i < node->return_stmt.count; i++)
                free_node(node->return_stmt.values[i]);
            free(node->return_stmt.values);
            break;

        default:
            break;
    }

    free(node);
}

/* Add a statement to a block node */
static void block_add_statement(BavAstNode* block, BavAstNode* stmt)
{
    if (!block || !stmt)
        return;

    if (block->block.count >= block->block.capacity)
    {
        u32 new_cap = block->block.capacity * 2;
        if (new_cap == 0)
            new_cap = 8;
        BavAstNode** new_stmts = realloc(block->block.statements, new_cap * sizeof(BavAstNode*));
        if (!new_stmts)
            return;
        block->block.statements = new_stmts;
        block->block.capacity = new_cap;
    }

    block->block.statements[block->block.count++] = stmt;
}

/* =============================================================================
 * Forward Declarations
 * ============================================================================= */

static BavAstNode* parse_expression(BavParser* parser);
static BavAstNode* parse_expression_precedence(BavParser* parser, int min_prec);
static BavAstNode* parse_prefix_expression(BavParser* parser);
static BavAstNode* parse_statement(BavParser* parser);
static BavAstNode* parse_block(BavParser* parser);
static BavAstNode* parse_expression_list(BavParser* parser, u32* count);

/* =============================================================================
 * Precedence Table
 * ============================================================================= */

typedef enum Precedence
{
    PREC_NONE = 0,
    PREC_OR,      /* or */
    PREC_AND,     /* and */
    PREC_COMPARE, /* < > <= >= ~= == */
    PREC_BOR,     /* | */
    PREC_BXOR,    /* ~ (binary) */
    PREC_BAND,    /* & */
    PREC_SHIFT,   /* << >> */
    PREC_CONCAT,  /* .. (right associative) */
    PREC_TERM,    /* + - */
    PREC_FACTOR,  /* * / // % */
    PREC_UNARY,   /* not # - ~ (unary) */
    PREC_POWER,   /* ^ (right associative) */
    PREC_POSTFIX, /* . [] () : */
} Precedence;

/* Get precedence for binary operators */
static int get_binary_precedence(BavTokenType type)
{
    switch (type)
    {
        case BAV_TOKEN_OR:
            return PREC_OR;
        case BAV_TOKEN_AND:
            return PREC_AND;
        case BAV_TOKEN_LT:
        case BAV_TOKEN_GT:
        case BAV_TOKEN_LE:
        case BAV_TOKEN_GE:
        case BAV_TOKEN_NE:
        case BAV_TOKEN_EQ:
            return PREC_COMPARE;
        case BAV_TOKEN_PIPE:
            return PREC_BOR;
        case BAV_TOKEN_TILDE:
            return PREC_BXOR;
        case BAV_TOKEN_AMPERSAND:
            return PREC_BAND;
        case BAV_TOKEN_LT_LT:
        case BAV_TOKEN_GT_GT:
            return PREC_SHIFT;
        case BAV_TOKEN_DOT_DOT:
            return PREC_CONCAT;
        case BAV_TOKEN_PLUS:
        case BAV_TOKEN_MINUS:
            return PREC_TERM;
        case BAV_TOKEN_STAR:
        case BAV_TOKEN_SLASH:
        case BAV_TOKEN_SLASH_SLASH:
        case BAV_TOKEN_PERCENT:
            return PREC_FACTOR;
        case BAV_TOKEN_CARET:
            return PREC_POWER;
        default:
            return PREC_NONE;
    }
}

/* Check if operator is right associative */
static b8 is_right_associative(BavTokenType type)
{
    return type == BAV_TOKEN_CARET || type == BAV_TOKEN_DOT_DOT;
}

/* =============================================================================
 * Expression Parsing
 * ============================================================================= */

/* Parse a primary expression (literals, identifiers, parenthesized, etc.) */
static BavAstNode* parse_primary(BavParser* parser)
{
    switch (parser->current.type)
    {
        case BAV_TOKEN_NIL:
            parser_advance(parser);
            return make_node(AST_NIL, parser->previous.line, parser->previous.column);

        case BAV_TOKEN_TRUE:
            parser_advance(parser);
            return make_node(AST_TRUE, parser->previous.line, parser->previous.column);

        case BAV_TOKEN_FALSE:
            parser_advance(parser);
            return make_node(AST_FALSE, parser->previous.line, parser->previous.column);

        case BAV_TOKEN_NUMBER:
        {
            parser_advance(parser);
            BavAstNode* node =
                make_node(AST_NUMBER, parser->previous.line, parser->previous.column);
            if (node)
            {
                node->number.value = strtod(parser->previous.start, NULL);
            }
            return node;
        }

        case BAV_TOKEN_STRING:
        {
            parser_advance(parser);
            BavAstNode* node =
                make_node(AST_STRING, parser->previous.line, parser->previous.column);
            if (node)
            {
                /* Skip opening quote, exclude closing quote */
                node->string.data = parser->previous.start + 1;
                node->string.length = parser->previous.length - 2;
            }
            return node;
        }

        case BAV_TOKEN_IDENTIFIER:
        {
            parser_advance(parser);
            BavAstNode* node = make_node(AST_VAR, parser->previous.line, parser->previous.column);
            if (node)
            {
                node->var.name = parser->previous.start;
                node->var.name_len = parser->previous.length;
            }
            return node;
        }

        case BAV_TOKEN_DOT_DOT_DOT:
        {
            parser_advance(parser);
            return make_node(AST_VARARG, parser->previous.line, parser->previous.column);
        }

        case BAV_TOKEN_LPAREN:
        {
            parser_advance(parser);
            BavAstNode* expr = parse_expression(parser);
            consume(parser, BAV_TOKEN_RPAREN, "Expected ')' after expression");
            return expr;
        }

        case BAV_TOKEN_LBRACE:
            return parse_prefix_expression(parser); /* Table constructor */

        case BAV_TOKEN_FUNCTION:
            return parse_prefix_expression(parser); /* Anonymous function */

        default:
            error_at_current(parser, "Expected expression");
            return NULL;
    }
}

/* Parse function arguments - returns arg array, sets count */
static BavAstNode** parse_args(BavParser* parser, u32* arg_count)
{
    BavAstNode** args = NULL;
    *arg_count = 0;

    /* String or table literal as single argument */
    if (parser->current.type == BAV_TOKEN_STRING)
    {
        args = malloc(sizeof(BavAstNode*));
        if (args)
        {
            args[0] = parse_primary(parser);
            *arg_count = 1;
        }
        return args;
    }

    if (parser->current.type == BAV_TOKEN_LBRACE)
    {
        args = malloc(sizeof(BavAstNode*));
        if (args)
        {
            args[0] = parse_prefix_expression(parser);
            *arg_count = 1;
        }
        return args;
    }

    /* Regular parenthesized arguments */
    consume(parser, BAV_TOKEN_LPAREN, "Expected '(' for function call");

    if (!check(parser, BAV_TOKEN_RPAREN))
    {
        u32 capacity = 4;
        args = malloc(capacity * sizeof(BavAstNode*));
        if (!args)
            return NULL;

        do
        {
            if (*arg_count >= capacity)
            {
                capacity *= 2;
                BavAstNode** new_args = realloc(args, capacity * sizeof(BavAstNode*));
                if (!new_args)
                {
                    free(args);
                    return NULL;
                }
                args = new_args;
            }
            args[(*arg_count)++] = parse_expression(parser);
        } while (match(parser, BAV_TOKEN_COMMA));
    }

    consume(parser, BAV_TOKEN_RPAREN, "Expected ')' after arguments");
    return args;
}

/* Parse table constructor: { field, field, ... } */
static BavAstNode* parse_table_constructor(BavParser* parser)
{
    u32 line = parser->current.line;
    u32 col = parser->current.column;

    consume(parser, BAV_TOKEN_LBRACE, "Expected '{'");

    BavAstNode* table = make_node(AST_TABLE, line, col);
    if (!table)
        return NULL;

    while (!check(parser, BAV_TOKEN_RBRACE) && !check(parser, BAV_TOKEN_EOF))
    {
        BavAstNode* field =
            make_node(AST_TABLE_FIELD, parser->current.line, parser->current.column);
        if (!field)
            break;

        if (match(parser, BAV_TOKEN_LBRACKET))
        {
            /* [expr] = value */
            field->table_field.key = parse_expression(parser);
            consume(parser, BAV_TOKEN_RBRACKET, "Expected ']'");
            consume(parser, BAV_TOKEN_ASSIGN, "Expected '='");
            field->table_field.value = parse_expression(parser);
        }
        else if (parser->current.type == BAV_TOKEN_IDENTIFIER)
        {
            /* Check if it's name = value or just a value */
            BavToken id = parser->current;
            parser_advance(parser);

            if (match(parser, BAV_TOKEN_ASSIGN))
            {
                /* name = value */
                BavAstNode* key = make_node(AST_STRING, id.line, id.column);
                if (key)
                {
                    key->string.data = id.start;
                    key->string.length = id.length;
                }
                field->table_field.key = key;
                field->table_field.value = parse_expression(parser);
            }
            else
            {
                /* Just a value - need to parse rest of expression starting from identifier */
                BavAstNode* var = make_node(AST_VAR, id.line, id.column);
                if (var)
                {
                    var->var.name = id.start;
                    var->var.name_len = id.length;
                }
                /* Continue parsing any postfix operations */
                field->table_field.key = NULL; /* Array-style */
                /* We need to parse the rest of the expression if there are postfix ops */
                /* For simplicity, just use the var as the value */
                field->table_field.value = var;

                /* Handle postfix operations on the variable */
                while (check(parser, BAV_TOKEN_DOT) || check(parser, BAV_TOKEN_LBRACKET) ||
                       check(parser, BAV_TOKEN_LPAREN) || check(parser, BAV_TOKEN_COLON) ||
                       check(parser, BAV_TOKEN_STRING) || check(parser, BAV_TOKEN_LBRACE))
                {
                    if (match(parser, BAV_TOKEN_DOT))
                    {
                        consume(parser, BAV_TOKEN_IDENTIFIER, "Expected field name after '.'");
                        BavAstNode* access =
                            make_node(AST_FIELD, parser->previous.line, parser->previous.column);
                        if (access)
                        {
                            access->field.object = field->table_field.value;
                            access->field.name = parser->previous.start;
                            access->field.name_len = parser->previous.length;
                            field->table_field.value = access;
                        }
                    }
                    else if (match(parser, BAV_TOKEN_LBRACKET))
                    {
                        BavAstNode* idx =
                            make_node(AST_INDEX, parser->previous.line, parser->previous.column);
                        if (idx)
                        {
                            idx->index.object = field->table_field.value;
                            idx->index.key = parse_expression(parser);
                            field->table_field.value = idx;
                        }
                        consume(parser, BAV_TOKEN_RBRACKET, "Expected ']'");
                    }
                    else if (check(parser, BAV_TOKEN_LPAREN) || check(parser, BAV_TOKEN_STRING) ||
                             check(parser, BAV_TOKEN_LBRACE))
                    {
                        u32 arg_count;
                        BavAstNode** args = parse_args(parser, &arg_count);
                        BavAstNode* call =
                            make_node(AST_CALL, parser->previous.line, parser->previous.column);
                        if (call)
                        {
                            call->call.callee = field->table_field.value;
                            call->call.args = args;
                            call->call.arg_count = arg_count;
                            field->table_field.value = call;
                        }
                    }
                    else if (match(parser, BAV_TOKEN_COLON))
                    {
                        consume(parser, BAV_TOKEN_IDENTIFIER, "Expected method name");
                        const char* method = parser->previous.start;
                        usize method_len = parser->previous.length;
                        u32 arg_count;
                        BavAstNode** args = parse_args(parser, &arg_count);
                        BavAstNode* mcall = make_node(AST_METHOD_CALL, parser->previous.line,
                                                      parser->previous.column);
                        if (mcall)
                        {
                            mcall->method_call.object = field->table_field.value;
                            mcall->method_call.method = method;
                            mcall->method_call.method_len = method_len;
                            mcall->method_call.args = args;
                            mcall->method_call.arg_count = arg_count;
                            field->table_field.value = mcall;
                        }
                    }
                    else
                    {
                        break;
                    }
                }
            }
        }
        else
        {
            /* Just a value (array-style) */
            field->table_field.key = NULL;
            field->table_field.value = parse_expression(parser);
        }

        /* Add field to table */
        if (table->table.field_count >= table->table.field_capacity)
        {
            u32 new_cap = table->table.field_capacity * 2;
            if (new_cap == 0)
                new_cap = 4;
            BavAstNode** new_fields = realloc(table->table.fields, new_cap * sizeof(BavAstNode*));
            if (new_fields)
            {
                table->table.fields = new_fields;
                table->table.field_capacity = new_cap;
            }
        }
        if (table->table.field_count < table->table.field_capacity)
        {
            table->table.fields[table->table.field_count++] = field;
        }

        /* Optional separator */
        if (!match(parser, BAV_TOKEN_COMMA) && !match(parser, BAV_TOKEN_SEMICOLON))
        {
            break;
        }
    }

    consume(parser, BAV_TOKEN_RBRACE, "Expected '}'");
    return table;
}

/* Parse anonymous function: function(params) body end */
static BavAstNode* parse_function_expr(BavParser* parser)
{
    u32 line = parser->current.line;
    u32 col = parser->current.column;

    consume(parser, BAV_TOKEN_FUNCTION, "Expected 'function'");

    BavAstNode* func = make_node(AST_FUNCTION_EXPR, line, col);
    if (!func)
        return NULL;

    /* Parameter list */
    consume(parser, BAV_TOKEN_LPAREN, "Expected '(' after 'function'");

    u32 param_capacity = 4;
    func->function.params = malloc(param_capacity * sizeof(const char*));
    func->function.param_lens = malloc(param_capacity * sizeof(usize));
    func->function.param_count = 0;
    func->function.is_vararg = 0;

    if (!check(parser, BAV_TOKEN_RPAREN))
    {
        do
        {
            if (match(parser, BAV_TOKEN_DOT_DOT_DOT))
            {
                func->function.is_vararg = 1;
                break;
            }

            consume(parser, BAV_TOKEN_IDENTIFIER, "Expected parameter name");

            if (func->function.param_count >= param_capacity)
            {
                param_capacity *= 2;
                func->function.params =
                    realloc(func->function.params, param_capacity * sizeof(const char*));
                func->function.param_lens =
                    realloc(func->function.param_lens, param_capacity * sizeof(usize));
            }

            func->function.params[func->function.param_count] = parser->previous.start;
            func->function.param_lens[func->function.param_count] = parser->previous.length;
            func->function.param_count++;

        } while (match(parser, BAV_TOKEN_COMMA));
    }

    consume(parser, BAV_TOKEN_RPAREN, "Expected ')' after parameters");

    /* Body */
    func->function.body = parse_block(parser);

    consume(parser, BAV_TOKEN_END, "Expected 'end' after function body");

    return func;
}

/* Parse prefix expression - handles unary operators, tables, functions */
static BavAstNode* parse_prefix_expression(BavParser* parser)
{
    /* Unary operators */
    if (match(parser, BAV_TOKEN_MINUS) || match(parser, BAV_TOKEN_NOT) ||
        match(parser, BAV_TOKEN_HASH) || match(parser, BAV_TOKEN_TILDE))
    {
        BavTokenType op = parser->previous.type;
        u32 line = parser->previous.line;
        u32 col = parser->previous.column;

        /* Right associative, same precedence */
        BavAstNode* operand = parse_expression_precedence(parser, PREC_UNARY);

        BavAstNode* unary = make_node(AST_UNARY_OP, line, col);
        if (unary)
        {
            unary->unary.op = op;
            unary->unary.operand = operand;
        }
        return unary;
    }

    /* Table constructor */
    if (check(parser, BAV_TOKEN_LBRACE))
    {
        return parse_table_constructor(parser);
    }

    /* Function expression */
    if (check(parser, BAV_TOKEN_FUNCTION))
    {
        return parse_function_expr(parser);
    }

    /* Primary expression */
    return parse_primary(parser);
}

/* Parse postfix operations (calls, indexing, field access, method calls) */
static BavAstNode* parse_postfix(BavParser* parser, BavAstNode* left)
{
    for (;;)
    {
        if (match(parser, BAV_TOKEN_DOT))
        {
            /* Field access: a.b */
            consume(parser, BAV_TOKEN_IDENTIFIER, "Expected field name after '.'");
            BavAstNode* field =
                make_node(AST_FIELD, parser->previous.line, parser->previous.column);
            if (field)
            {
                field->field.object = left;
                field->field.name = parser->previous.start;
                field->field.name_len = parser->previous.length;
            }
            left = field;
        }
        else if (match(parser, BAV_TOKEN_LBRACKET))
        {
            /* Index: a[b] */
            BavAstNode* idx = make_node(AST_INDEX, parser->previous.line, parser->previous.column);
            if (idx)
            {
                idx->index.object = left;
                idx->index.key = parse_expression(parser);
            }
            consume(parser, BAV_TOKEN_RBRACKET, "Expected ']' after index");
            left = idx;
        }
        else if (check(parser, BAV_TOKEN_LPAREN) || check(parser, BAV_TOKEN_STRING) ||
                 check(parser, BAV_TOKEN_LBRACE))
        {
            /* Function call: a(args) or a"string" or a{table} */
            u32 arg_count;
            BavAstNode** args = parse_args(parser, &arg_count);
            BavAstNode* call = make_node(AST_CALL, parser->previous.line, parser->previous.column);
            if (call)
            {
                call->call.callee = left;
                call->call.args = args;
                call->call.arg_count = arg_count;
            }
            left = call;
        }
        else if (match(parser, BAV_TOKEN_COLON))
        {
            /* Method call: a:b(args) */
            consume(parser, BAV_TOKEN_IDENTIFIER, "Expected method name after ':'");
            const char* method = parser->previous.start;
            usize method_len = parser->previous.length;

            u32 arg_count;
            BavAstNode** args = parse_args(parser, &arg_count);

            BavAstNode* mcall =
                make_node(AST_METHOD_CALL, parser->previous.line, parser->previous.column);
            if (mcall)
            {
                mcall->method_call.object = left;
                mcall->method_call.method = method;
                mcall->method_call.method_len = method_len;
                mcall->method_call.args = args;
                mcall->method_call.arg_count = arg_count;
            }
            left = mcall;
        }
        else
        {
            break;
        }
    }

    return left;
}

/* Parse expression with precedence climbing */
static BavAstNode* parse_expression_precedence(BavParser* parser, int min_prec)
{
    BavAstNode* left = parse_prefix_expression(parser);
    if (!left)
        return NULL;

    /* Handle postfix operations first */
    left = parse_postfix(parser, left);

    /* Binary operators with precedence climbing */
    for (;;)
    {
        int prec = get_binary_precedence(parser->current.type);
        if (prec < min_prec)
            break;

        BavTokenType op = parser->current.type;
        parser_advance(parser);

        /* Adjust for right associativity */
        int next_prec = is_right_associative(op) ? prec : prec + 1;

        BavAstNode* right = parse_expression_precedence(parser, next_prec);

        BavAstNode* binary =
            make_node(AST_BINARY_OP, parser->previous.line, parser->previous.column);
        if (binary)
        {
            binary->binary.op = op;
            binary->binary.left = left;
            binary->binary.right = right;
        }
        left = binary;
    }

    return left;
}

/* Top-level expression parsing entry point */
static BavAstNode* parse_expression(BavParser* parser)
{
    return parse_expression_precedence(parser, PREC_OR);
}

/* Parse comma-separated expression list, return array and count */
static BavAstNode** parse_expression_list_to_array(BavParser* parser, u32* count)
{
    u32 capacity = 4;
    BavAstNode** exprs = malloc(capacity * sizeof(BavAstNode*));
    *count = 0;

    if (!exprs)
        return NULL;

    do
    {
        if (*count >= capacity)
        {
            capacity *= 2;
            BavAstNode** new_exprs = realloc(exprs, capacity * sizeof(BavAstNode*));
            if (!new_exprs)
            {
                free(exprs);
                return NULL;
            }
            exprs = new_exprs;
        }
        exprs[(*count)++] = parse_expression(parser);
    } while (match(parser, BAV_TOKEN_COMMA));

    return exprs;
}

/* =============================================================================
 * Statement Parsing
 * ============================================================================= */

/* Parse local variable declaration or local function */
static BavAstNode* parse_local(BavParser* parser)
{
    u32 line = parser->previous.line;
    u32 col = parser->previous.column;

    /* Local function */
    if (match(parser, BAV_TOKEN_FUNCTION))
    {
        consume(parser, BAV_TOKEN_IDENTIFIER, "Expected function name");

        BavAstNode* func = make_node(AST_LOCAL_FUNCTION, line, col);
        if (!func)
            return NULL;

        func->function.name = parser->previous.start;
        func->function.name_len = parser->previous.length;
        func->function.is_local = 1;

        /* Parameters */
        consume(parser, BAV_TOKEN_LPAREN, "Expected '('");

        u32 param_capacity = 4;
        func->function.params = malloc(param_capacity * sizeof(const char*));
        func->function.param_lens = malloc(param_capacity * sizeof(usize));
        func->function.param_count = 0;
        func->function.is_vararg = 0;

        if (!check(parser, BAV_TOKEN_RPAREN))
        {
            do
            {
                if (match(parser, BAV_TOKEN_DOT_DOT_DOT))
                {
                    func->function.is_vararg = 1;
                    break;
                }

                consume(parser, BAV_TOKEN_IDENTIFIER, "Expected parameter name");

                if (func->function.param_count >= param_capacity)
                {
                    param_capacity *= 2;
                    func->function.params =
                        realloc(func->function.params, param_capacity * sizeof(const char*));
                    func->function.param_lens =
                        realloc(func->function.param_lens, param_capacity * sizeof(usize));
                }

                func->function.params[func->function.param_count] = parser->previous.start;
                func->function.param_lens[func->function.param_count] = parser->previous.length;
                func->function.param_count++;

            } while (match(parser, BAV_TOKEN_COMMA));
        }

        consume(parser, BAV_TOKEN_RPAREN, "Expected ')'");

        func->function.body = parse_block(parser);
        consume(parser, BAV_TOKEN_END, "Expected 'end'");

        return func;
    }

    /* Local variable declaration */
    BavAstNode* local = make_node(AST_LOCAL, line, col);
    if (!local)
        return NULL;

    u32 name_capacity = 4;
    local->local.names = malloc(name_capacity * sizeof(const char*));
    local->local.name_lens = malloc(name_capacity * sizeof(usize));
    local->local.name_count = 0;
    local->local.values = NULL;
    local->local.value_count = 0;

    /* Parse name list */
    do
    {
        consume(parser, BAV_TOKEN_IDENTIFIER, "Expected variable name");

        if (local->local.name_count >= name_capacity)
        {
            name_capacity *= 2;
            local->local.names = realloc(local->local.names, name_capacity * sizeof(const char*));
            local->local.name_lens = realloc(local->local.name_lens, name_capacity * sizeof(usize));
        }

        local->local.names[local->local.name_count] = parser->previous.start;
        local->local.name_lens[local->local.name_count] = parser->previous.length;
        local->local.name_count++;

    } while (match(parser, BAV_TOKEN_COMMA));

    /* Optional initialization */
    if (match(parser, BAV_TOKEN_ASSIGN))
    {
        local->local.values = parse_expression_list_to_array(parser, &local->local.value_count);
    }

    return local;
}

/* Parse if statement */
static BavAstNode* parse_if(BavParser* parser)
{
    u32 line = parser->previous.line;
    u32 col = parser->previous.column;

    BavAstNode* if_stmt = make_node(AST_IF, line, col);
    if (!if_stmt)
        return NULL;

    /* Condition */
    if_stmt->if_stmt.condition = parse_expression(parser);
    consume(parser, BAV_TOKEN_THEN, "Expected 'then' after condition");

    /* Then block */
    if_stmt->if_stmt.then_block = parse_block(parser);

    /* Elseif clauses */
    u32 elseif_capacity = 0;
    if_stmt->if_stmt.elseif_conds = NULL;
    if_stmt->if_stmt.elseif_blocks = NULL;
    if_stmt->if_stmt.elseif_count = 0;
    if_stmt->if_stmt.else_block = NULL;

    while (match(parser, BAV_TOKEN_ELSEIF))
    {
        if (if_stmt->if_stmt.elseif_count >= elseif_capacity)
        {
            elseif_capacity = elseif_capacity * 2 + 2;
            if_stmt->if_stmt.elseif_conds =
                realloc(if_stmt->if_stmt.elseif_conds, elseif_capacity * sizeof(BavAstNode*));
            if_stmt->if_stmt.elseif_blocks =
                realloc(if_stmt->if_stmt.elseif_blocks, elseif_capacity * sizeof(BavAstNode*));
        }

        if_stmt->if_stmt.elseif_conds[if_stmt->if_stmt.elseif_count] = parse_expression(parser);
        consume(parser, BAV_TOKEN_THEN, "Expected 'then' after elseif condition");
        if_stmt->if_stmt.elseif_blocks[if_stmt->if_stmt.elseif_count] = parse_block(parser);
        if_stmt->if_stmt.elseif_count++;
    }

    /* Else clause */
    if (match(parser, BAV_TOKEN_ELSE))
    {
        if_stmt->if_stmt.else_block = parse_block(parser);
    }

    consume(parser, BAV_TOKEN_END, "Expected 'end' after if statement");

    return if_stmt;
}

/* Parse while loop */
static BavAstNode* parse_while(BavParser* parser)
{
    u32 line = parser->previous.line;
    u32 col = parser->previous.column;

    BavAstNode* while_stmt = make_node(AST_WHILE, line, col);
    if (!while_stmt)
        return NULL;

    while_stmt->while_loop.condition = parse_expression(parser);
    consume(parser, BAV_TOKEN_DO, "Expected 'do' after while condition");
    while_stmt->while_loop.body = parse_block(parser);
    consume(parser, BAV_TOKEN_END, "Expected 'end' after while body");

    return while_stmt;
}

/* Parse repeat-until loop */
static BavAstNode* parse_repeat(BavParser* parser)
{
    u32 line = parser->previous.line;
    u32 col = parser->previous.column;

    BavAstNode* repeat = make_node(AST_REPEAT, line, col);
    if (!repeat)
        return NULL;

    repeat->repeat_loop.body = parse_block(parser);
    consume(parser, BAV_TOKEN_UNTIL, "Expected 'until'");
    repeat->repeat_loop.condition = parse_expression(parser);

    return repeat;
}

/* Parse for loop (numeric or generic) */
static BavAstNode* parse_for(BavParser* parser)
{
    u32 line = parser->previous.line;
    u32 col = parser->previous.column;

    consume(parser, BAV_TOKEN_IDENTIFIER, "Expected variable name");
    const char* first_var = parser->previous.start;
    usize first_len = parser->previous.length;

    if (match(parser, BAV_TOKEN_ASSIGN))
    {
        /* Numeric for: for i = start, limit [, step] do body end */
        BavAstNode* for_num = make_node(AST_FOR_NUMERIC, line, col);
        if (!for_num)
            return NULL;

        for_num->for_numeric.var_name = first_var;
        for_num->for_numeric.var_len = first_len;
        for_num->for_numeric.start = parse_expression(parser);

        consume(parser, BAV_TOKEN_COMMA, "Expected ',' after start value");
        for_num->for_numeric.limit = parse_expression(parser);

        if (match(parser, BAV_TOKEN_COMMA))
        {
            for_num->for_numeric.step = parse_expression(parser);
        }
        else
        {
            for_num->for_numeric.step = NULL;
        }

        consume(parser, BAV_TOKEN_DO, "Expected 'do'");
        for_num->for_numeric.body = parse_block(parser);
        consume(parser, BAV_TOKEN_END, "Expected 'end'");

        return for_num;
    }
    else
    {
        /* Generic for: for var1, var2, ... in explist do body end */
        BavAstNode* for_gen = make_node(AST_FOR_GENERIC, line, col);
        if (!for_gen)
            return NULL;

        u32 var_capacity = 4;
        for_gen->for_generic.var_names = malloc(var_capacity * sizeof(const char*));
        for_gen->for_generic.var_lens = malloc(var_capacity * sizeof(usize));
        for_gen->for_generic.var_names[0] = first_var;
        for_gen->for_generic.var_lens[0] = first_len;
        for_gen->for_generic.var_count = 1;

        while (match(parser, BAV_TOKEN_COMMA))
        {
            consume(parser, BAV_TOKEN_IDENTIFIER, "Expected variable name");

            if (for_gen->for_generic.var_count >= var_capacity)
            {
                var_capacity *= 2;
                for_gen->for_generic.var_names =
                    realloc(for_gen->for_generic.var_names, var_capacity * sizeof(const char*));
                for_gen->for_generic.var_lens =
                    realloc(for_gen->for_generic.var_lens, var_capacity * sizeof(usize));
            }

            for_gen->for_generic.var_names[for_gen->for_generic.var_count] = parser->previous.start;
            for_gen->for_generic.var_lens[for_gen->for_generic.var_count] = parser->previous.length;
            for_gen->for_generic.var_count++;
        }

        consume(parser, BAV_TOKEN_IN, "Expected 'in'");
        for_gen->for_generic.iterators =
            parse_expression_list_to_array(parser, &for_gen->for_generic.iterator_count);

        consume(parser, BAV_TOKEN_DO, "Expected 'do'");
        for_gen->for_generic.body = parse_block(parser);
        consume(parser, BAV_TOKEN_END, "Expected 'end'");

        return for_gen;
    }
}

/* Parse function definition (global or with dot/colon path) */
static BavAstNode* parse_function_def(BavParser* parser)
{
    u32 line = parser->previous.line;
    u32 col = parser->previous.column;

    BavAstNode* func = make_node(AST_FUNCTION_DEF, line, col);
    if (!func)
        return NULL;

    /* Function name - can be a.b.c or a.b:c */
    consume(parser, BAV_TOKEN_IDENTIFIER, "Expected function name");
    func->function.name = parser->previous.start;
    func->function.name_len = parser->previous.length;
    func->function.is_method = 0;

    /* Handle dotted names and method syntax - for now just store the base name */
    /* Full implementation would build up the path */
    while (match(parser, BAV_TOKEN_DOT))
    {
        consume(parser, BAV_TOKEN_IDENTIFIER, "Expected name after '.'");
        /* Extend name... simplified: just update to final segment */
        func->function.name = parser->previous.start;
        func->function.name_len = parser->previous.length;
    }

    if (match(parser, BAV_TOKEN_COLON))
    {
        consume(parser, BAV_TOKEN_IDENTIFIER, "Expected method name after ':'");
        func->function.name = parser->previous.start;
        func->function.name_len = parser->previous.length;
        func->function.is_method = 1;
    }

    /* Parameters */
    consume(parser, BAV_TOKEN_LPAREN, "Expected '('");

    u32 param_capacity = 4;
    func->function.params = malloc(param_capacity * sizeof(const char*));
    func->function.param_lens = malloc(param_capacity * sizeof(usize));
    func->function.param_count = 0;
    func->function.is_vararg = 0;

    if (!check(parser, BAV_TOKEN_RPAREN))
    {
        do
        {
            if (match(parser, BAV_TOKEN_DOT_DOT_DOT))
            {
                func->function.is_vararg = 1;
                break;
            }

            consume(parser, BAV_TOKEN_IDENTIFIER, "Expected parameter name");

            if (func->function.param_count >= param_capacity)
            {
                param_capacity *= 2;
                func->function.params =
                    realloc(func->function.params, param_capacity * sizeof(const char*));
                func->function.param_lens =
                    realloc(func->function.param_lens, param_capacity * sizeof(usize));
            }

            func->function.params[func->function.param_count] = parser->previous.start;
            func->function.param_lens[func->function.param_count] = parser->previous.length;
            func->function.param_count++;

        } while (match(parser, BAV_TOKEN_COMMA));
    }

    consume(parser, BAV_TOKEN_RPAREN, "Expected ')'");

    func->function.body = parse_block(parser);
    consume(parser, BAV_TOKEN_END, "Expected 'end'");

    return func;
}

/* Parse return statement */
static BavAstNode* parse_return(BavParser* parser)
{
    u32 line = parser->previous.line;
    u32 col = parser->previous.column;

    BavAstNode* ret = make_node(AST_RETURN, line, col);
    if (!ret)
        return NULL;

    ret->return_stmt.values = NULL;
    ret->return_stmt.count = 0;

    /* Check if there are return values */
    if (!is_block_end(parser->current.type) && parser->current.type != BAV_TOKEN_SEMICOLON)
    {
        if (is_expression_start(parser->current.type))
        {
            ret->return_stmt.values =
                parse_expression_list_to_array(parser, &ret->return_stmt.count);
        }
    }

    /* Optional semicolon after return */
    match(parser, BAV_TOKEN_SEMICOLON);

    return ret;
}

/* Parse do block */
static BavAstNode* parse_do(BavParser* parser)
{
    u32 line = parser->previous.line;
    u32 col = parser->previous.column;

    BavAstNode* do_block = make_node(AST_DO, line, col);
    if (!do_block)
        return NULL;

    do_block->block.statements = NULL;
    do_block->block.count = 0;
    do_block->block.capacity = 0;

    BavAstNode* body = parse_block(parser);
    consume(parser, BAV_TOKEN_END, "Expected 'end'");

    /* Copy block contents into do_block */
    if (body)
    {
        do_block->block.statements = body->block.statements;
        do_block->block.count = body->block.count;
        do_block->block.capacity = body->block.capacity;
        body->block.statements = NULL; /* Prevent double-free */
        free(body);
    }

    return do_block;
}

/* Parse goto statement */
static BavAstNode* parse_goto(BavParser* parser)
{
    u32 line = parser->previous.line;
    u32 col = parser->previous.column;

    consume(parser, BAV_TOKEN_IDENTIFIER, "Expected label name");

    BavAstNode* goto_stmt = make_node(AST_GOTO, line, col);
    if (goto_stmt)
    {
        goto_stmt->goto_stmt.label = parser->previous.start;
        goto_stmt->goto_stmt.label_len = parser->previous.length;
    }

    return goto_stmt;
}

/* Parse label ::name:: */
static BavAstNode* parse_label(BavParser* parser)
{
    u32 line = parser->previous.line;
    u32 col = parser->previous.column;

    consume(parser, BAV_TOKEN_IDENTIFIER, "Expected label name");
    const char* name = parser->previous.start;
    usize name_len = parser->previous.length;

    consume(parser, BAV_TOKEN_DOUBLE_COLON, "Expected '::' after label name");

    BavAstNode* label = make_node(AST_LABEL, line, col);
    if (label)
    {
        label->label.name = name;
        label->label.name_len = name_len;
    }

    return label;
}

/* Parse expression statement (assignment or call) */
static BavAstNode* parse_expression_statement(BavParser* parser)
{
    BavAstNode* expr = parse_expression(parser);
    if (!expr)
        return NULL;

    /* Check for assignment */
    if (check(parser, BAV_TOKEN_ASSIGN) || check(parser, BAV_TOKEN_COMMA))
    {
        /* Multi-assignment: target, target, ... = value, value, ... */
        u32 target_capacity = 4;
        u32 target_count = 1;
        BavAstNode** targets = malloc(target_capacity * sizeof(BavAstNode*));
        if (!targets)
        {
            free_node(expr);
            return NULL;
        }
        targets[0] = expr;

        while (match(parser, BAV_TOKEN_COMMA))
        {
            if (target_count >= target_capacity)
            {
                target_capacity *= 2;
                targets = realloc(targets, target_capacity * sizeof(BavAstNode*));
            }
            targets[target_count++] = parse_expression(parser);
        }

        consume(parser, BAV_TOKEN_ASSIGN, "Expected '='");

        u32 value_count;
        BavAstNode** values = parse_expression_list_to_array(parser, &value_count);

        BavAstNode* assign = make_node(AST_ASSIGN, expr->line, expr->column);
        if (assign)
        {
            assign->assign.targets = targets;
            assign->assign.target_count = target_count;
            assign->assign.values = values;
            assign->assign.value_count = value_count;
        }
        return assign;
    }

    /* Must be a function call - wrap it as a statement */
    if (expr->type == AST_CALL || expr->type == AST_METHOD_CALL)
    {
        BavAstNode* call_stmt = make_node(AST_CALL_STMT, expr->line, expr->column);
        if (call_stmt)
        {
            call_stmt->call = expr->call;
            /* Transfer ownership - don't free expr's call data */
            expr->type = AST_NIL; /* Prevent double-free */
        }
        free(expr);
        return call_stmt;
    }

    /* If not an assignment and not a call, it's an error in Lua */
    error(parser, "Expected assignment or function call");
    free_node(expr);
    return NULL;
}

/* Parse a single statement */
static BavAstNode* parse_statement(BavParser* parser)
{
    if (match(parser, BAV_TOKEN_LOCAL))
    {
        return parse_local(parser);
    }

    if (match(parser, BAV_TOKEN_IF))
    {
        return parse_if(parser);
    }

    if (match(parser, BAV_TOKEN_WHILE))
    {
        return parse_while(parser);
    }

    if (match(parser, BAV_TOKEN_REPEAT))
    {
        return parse_repeat(parser);
    }

    if (match(parser, BAV_TOKEN_FOR))
    {
        return parse_for(parser);
    }

    if (match(parser, BAV_TOKEN_FUNCTION))
    {
        return parse_function_def(parser);
    }

    if (match(parser, BAV_TOKEN_RETURN))
    {
        return parse_return(parser);
    }

    if (match(parser, BAV_TOKEN_BREAK))
    {
        return make_node(AST_BREAK, parser->previous.line, parser->previous.column);
    }

    if (match(parser, BAV_TOKEN_DO))
    {
        return parse_do(parser);
    }

    if (match(parser, BAV_TOKEN_GOTO))
    {
        return parse_goto(parser);
    }

    if (match(parser, BAV_TOKEN_DOUBLE_COLON))
    {
        return parse_label(parser);
    }

    /* Skip empty statements */
    if (match(parser, BAV_TOKEN_SEMICOLON))
    {
        return NULL;
    }

    /* Must be expression statement (assignment or call) */
    return parse_expression_statement(parser);
}

/* Parse a block of statements */
static BavAstNode* parse_block(BavParser* parser)
{
    BavAstNode* block = make_node(AST_BLOCK, parser->current.line, parser->current.column);
    if (!block)
        return NULL;

    while (!is_block_end(parser->current.type))
    {
        BavAstNode* stmt = parse_statement(parser);
        if (stmt)
        {
            block_add_statement(block, stmt);
        }

        if (parser->panic_mode)
        {
            synchronize(parser);
        }

        /* Return is the last statement in a block */
        if (stmt && stmt->type == AST_RETURN)
        {
            break;
        }
    }

    return block;
}

/* Parse entire chunk (file/string) */
static BavAstNode* parse_chunk(BavParser* parser)
{
    BavAstNode* chunk = make_node(AST_CHUNK, 1, 1);
    if (!chunk)
        return NULL;

    /* A chunk is just a block */
    BavAstNode* block = parse_block(parser);
    if (block)
    {
        chunk->block = block->block;
        block->block.statements = NULL; /* Prevent double-free */
        free(block);
    }

    consume(parser, BAV_TOKEN_EOF, "Expected end of file");

    return chunk;
}

/* =============================================================================
 * Public API
 * ============================================================================= */

/**
 * Parse Lua source code into an AST.
 *
 * @param source     Source code (UTF-8)
 * @param source_len Length of source
 * @param errors     Output: array of errors (caller frees)
 * @param error_count Output: number of errors
 * @return AST root node, or NULL on failure
 */
BavAstNode* bav_parse_lua(const char* source, usize source_len, BavCompileError** errors,
                          u32* error_count)
{
    BavParser parser;
    parser.lexer = bav_lexer_create(source, source_len);
    parser.errors = NULL;
    parser.error_count = 0;
    parser.error_capacity = 0;
    parser.had_error = 0;
    parser.panic_mode = 0;

    if (!parser.lexer)
    {
        *errors = NULL;
        *error_count = 0;
        return NULL;
    }

    /* Prime the parser */
    parser_advance(&parser);

    /* Parse the chunk */
    BavAstNode* ast = parse_chunk(&parser);

    /* Clean up */
    bav_lexer_destroy(parser.lexer);

    *errors = parser.errors;
    *error_count = parser.error_count;

    if (parser.had_error)
    {
        free_node(ast);
        return NULL;
    }

    return ast;
}

/**
 * Free an AST.
 */
void bav_ast_free(BavAstNode* ast)
{
    free_node(ast);
}
