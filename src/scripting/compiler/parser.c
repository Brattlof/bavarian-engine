/**
 * @file parser.c
 * @brief Lua Parser - AST Construction
 *
 * Converts token stream into an Abstract Syntax Tree.
 * This is the second phase of the compilation pipeline.
 */

#include <bavarian/scripting.h>
#include <bavarian/types.h>
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
        } block;

        /* Function call */
        struct
        {
            BavAstNode* callee;
            BavAstNode** args;
            u32 arg_count;
        } call;

        /* Assignment */
        struct
        {
            BavAstNode** targets;
            u32 target_count;
            BavAstNode** values;
            u32 value_count;
        } assign;

        /* If statement */
        struct
        {
            BavAstNode* condition;
            BavAstNode* then_block;
            BavAstNode** elseifs; /* Pairs of condition/block */
            u32 elseif_count;
            BavAstNode* else_block;
        } if_stmt;

        /* While loop */
        struct
        {
            BavAstNode* condition;
            BavAstNode* body;
        } while_loop;

        /* For numeric loop */
        struct
        {
            const char* var_name;
            usize var_len;
            BavAstNode* start;
            BavAstNode* limit;
            BavAstNode* step;
            BavAstNode* body;
        } for_numeric;

        /* Function definition */
        struct
        {
            const char** params;
            u32 param_count;
            b8 is_vararg;
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
        } table;

        /* Index expression (a[b]) */
        struct
        {
            BavAstNode* object;
            BavAstNode* index;
        } index;

        /* Field access (a.b) */
        struct
        {
            BavAstNode* object;
            const char* field;
            usize field_len;
        } field;
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
    err->filename = NULL; /* Set by caller */
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

/* =============================================================================
 * Expression Parsing (Pratt Parser)
 * ============================================================================= */

/* Forward declarations */
static BavAstNode* parse_expression(BavParser* parser);
static BavAstNode* parse_statement(BavParser* parser);
static BavAstNode* parse_block(BavParser* parser);

typedef enum Precedence
{
    PREC_NONE,
    PREC_OR,      /* or */
    PREC_AND,     /* and */
    PREC_COMPARE, /* < > <= >= ~= == */
    PREC_BOR,     /* | */
    PREC_BXOR,    /* ~ */
    PREC_BAND,    /* & */
    PREC_SHIFT,   /* << >> */
    PREC_CONCAT,  /* .. */
    PREC_TERM,    /* + - */
    PREC_FACTOR,  /* * / // % */
    PREC_UNARY,   /* not # - ~ */
    PREC_POWER,   /* ^ */
    PREC_CALL,    /* . [] () : */
} Precedence;

/* Placeholder for expression parsing - full implementation would be much longer */
static BavAstNode* parse_expression(BavParser* parser)
{
    /* Simplified: just handle basic cases */
    parser_advance(parser);

    switch (parser->previous.type)
    {
        case BAV_TOKEN_NIL:
            return make_node(AST_NIL, parser->previous.line, parser->previous.column);

        case BAV_TOKEN_TRUE:
            return make_node(AST_TRUE, parser->previous.line, parser->previous.column);

        case BAV_TOKEN_FALSE:
            return make_node(AST_FALSE, parser->previous.line, parser->previous.column);

        case BAV_TOKEN_NUMBER:
        {
            BavAstNode* node =
                make_node(AST_NUMBER, parser->previous.line, parser->previous.column);
            if (node)
            {
                /* Parse number value - simplified */
                node->number.value = strtod(parser->previous.start, NULL);
            }
            return node;
        }

        case BAV_TOKEN_STRING:
        {
            BavAstNode* node =
                make_node(AST_STRING, parser->previous.line, parser->previous.column);
            if (node)
            {
                node->string.data = parser->previous.start + 1;    /* Skip quote */
                node->string.length = parser->previous.length - 2; /* Exclude quotes */
            }
            return node;
        }

        case BAV_TOKEN_IDENTIFIER:
        {
            BavAstNode* node = make_node(AST_VAR, parser->previous.line, parser->previous.column);
            if (node)
            {
                node->var.name = parser->previous.start;
                node->var.name_len = parser->previous.length;
            }
            return node;
        }

        default:
            error(parser, "Expected expression");
            return NULL;
    }
}

/* =============================================================================
 * Statement Parsing
 * ============================================================================= */

static BavAstNode* parse_statement(BavParser* parser)
{
    /* Simplified statement parsing */
    if (match(parser, BAV_TOKEN_LOCAL))
    {
        /* Local declaration */
        BavAstNode* node = make_node(AST_LOCAL, parser->previous.line, parser->previous.column);
        /* TODO: Parse local declaration */
        return node;
    }

    if (match(parser, BAV_TOKEN_RETURN))
    {
        BavAstNode* node = make_node(AST_RETURN, parser->previous.line, parser->previous.column);
        /* TODO: Parse return values */
        return node;
    }

    if (match(parser, BAV_TOKEN_IF))
    {
        BavAstNode* node = make_node(AST_IF, parser->previous.line, parser->previous.column);
        /* TODO: Parse if statement */
        return node;
    }

    if (match(parser, BAV_TOKEN_WHILE))
    {
        BavAstNode* node = make_node(AST_WHILE, parser->previous.line, parser->previous.column);
        /* TODO: Parse while loop */
        return node;
    }

    if (match(parser, BAV_TOKEN_FOR))
    {
        /* TODO: Parse for loop */
        return NULL;
    }

    if (match(parser, BAV_TOKEN_FUNCTION))
    {
        /* TODO: Parse function definition */
        return NULL;
    }

    /* Expression statement */
    return parse_expression(parser);
}

static BavAstNode* parse_block(BavParser* parser)
{
    BavAstNode* block = make_node(AST_BLOCK, parser->current.line, parser->current.column);
    if (!block)
        return NULL;

    /* TODO: Parse statements until block-ending keyword */

    return block;
}

/* =============================================================================
 * Public API (Stub)
 * ============================================================================= */

/* Parser is internal - exposed through compile API in scripting.c */
