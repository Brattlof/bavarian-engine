/**
 * @file lexer.c
 * @brief Lua Lexer - Tokenization
 *
 * Converts UTF-8 source code into a stream of tokens.
 * This is the first phase of the compilation pipeline.
 */

#include <bavarian/scripting.h>
#include <bavarian/types.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* =============================================================================
 * Lexer State
 * ============================================================================= */

struct BavLexer
{
    const char* source;
    const char* start;
    const char* current;
    usize source_len;
    u32 line;
    u32 column;
    BavToken peeked;
    b8 has_peeked;
};

/* =============================================================================
 * Character Utilities
 * ============================================================================= */

static b8 is_at_end(BavLexer* lexer)
{
    return (usize)(lexer->current - lexer->source) >= lexer->source_len;
}

static char advance(BavLexer* lexer)
{
    if (is_at_end(lexer))
        return '\0';
    lexer->column++;
    return *lexer->current++;
}

static char peek(BavLexer* lexer)
{
    if (is_at_end(lexer))
        return '\0';
    return *lexer->current;
}

static char peek_next(BavLexer* lexer)
{
    if ((usize)(lexer->current - lexer->source + 1) >= lexer->source_len)
        return '\0';
    return lexer->current[1];
}

static b8 match(BavLexer* lexer, char expected)
{
    if (is_at_end(lexer))
        return 0;
    if (*lexer->current != expected)
        return 0;
    lexer->current++;
    lexer->column++;
    return 1;
}

static void skip_whitespace(BavLexer* lexer)
{
    for (;;)
    {
        char c = peek(lexer);
        switch (c)
        {
            case ' ':
            case '\r':
            case '\t':
                advance(lexer);
                break;
            case '\n':
                lexer->line++;
                lexer->column = 0;
                advance(lexer);
                break;
            case '-':
                if (peek_next(lexer) == '-')
                {
                    /* Single-line comment */
                    while (peek(lexer) != '\n' && !is_at_end(lexer))
                    {
                        advance(lexer);
                    }
                }
                else
                {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

/* =============================================================================
 * Token Creation
 * ============================================================================= */

static BavToken make_token(BavLexer* lexer, BavTokenType type)
{
    BavToken token;
    token.type = type;
    token.start = lexer->start;
    token.length = (u32)(lexer->current - lexer->start);
    token.line = lexer->line;
    token.column = lexer->column - token.length;
    return token;
}

static BavToken error_token(BavLexer* lexer, const char* message)
{
    BavToken token;
    token.type = BAV_TOKEN_ERROR;
    token.start = message;
    token.length = (u32)strlen(message);
    token.line = lexer->line;
    token.column = lexer->column;
    return token;
}

/* =============================================================================
 * Keywords
 * ============================================================================= */

typedef struct Keyword
{
    const char* name;
    usize length;
    BavTokenType type;
} Keyword;

static const Keyword keywords[] = {{"and", 3, BAV_TOKEN_AND},
                                   {"break", 5, BAV_TOKEN_BREAK},
                                   {"do", 2, BAV_TOKEN_DO},
                                   {"else", 4, BAV_TOKEN_ELSE},
                                   {"elseif", 6, BAV_TOKEN_ELSEIF},
                                   {"end", 3, BAV_TOKEN_END},
                                   {"false", 5, BAV_TOKEN_FALSE},
                                   {"for", 3, BAV_TOKEN_FOR},
                                   {"function", 8, BAV_TOKEN_FUNCTION},
                                   {"goto", 4, BAV_TOKEN_GOTO},
                                   {"if", 2, BAV_TOKEN_IF},
                                   {"in", 2, BAV_TOKEN_IN},
                                   {"local", 5, BAV_TOKEN_LOCAL},
                                   {"nil", 3, BAV_TOKEN_NIL},
                                   {"not", 3, BAV_TOKEN_NOT},
                                   {"or", 2, BAV_TOKEN_OR},
                                   {"repeat", 6, BAV_TOKEN_REPEAT},
                                   {"return", 6, BAV_TOKEN_RETURN},
                                   {"then", 4, BAV_TOKEN_THEN},
                                   {"true", 4, BAV_TOKEN_TRUE},
                                   {"until", 5, BAV_TOKEN_UNTIL},
                                   {"while", 5, BAV_TOKEN_WHILE},
                                   {NULL, 0, BAV_TOKEN_EOF}};

static BavTokenType check_keyword(const char* start, usize length)
{
    for (const Keyword* kw = keywords; kw->name != NULL; kw++)
    {
        if (kw->length == length && memcmp(start, kw->name, length) == 0)
        {
            return kw->type;
        }
    }
    return BAV_TOKEN_IDENTIFIER;
}

/* =============================================================================
 * Token Scanning
 * ============================================================================= */

static BavToken scan_string(BavLexer* lexer, char quote)
{
    while (peek(lexer) != quote && !is_at_end(lexer))
    {
        if (peek(lexer) == '\n')
        {
            lexer->line++;
            lexer->column = 0;
        }
        if (peek(lexer) == '\\')
        {
            advance(lexer); /* Skip escape char */
            if (!is_at_end(lexer))
                advance(lexer); /* Skip escaped char */
        }
        else
        {
            advance(lexer);
        }
    }

    if (is_at_end(lexer))
    {
        return error_token(lexer, "Unterminated string");
    }

    advance(lexer); /* Closing quote */
    return make_token(lexer, BAV_TOKEN_STRING);
}

static BavToken scan_number(BavLexer* lexer)
{
    while (isdigit(peek(lexer)))
        advance(lexer);

    /* Look for decimal part */
    if (peek(lexer) == '.' && isdigit(peek_next(lexer)))
    {
        advance(lexer); /* Consume '.' */
        while (isdigit(peek(lexer)))
            advance(lexer);
    }

    /* Look for exponent */
    if (peek(lexer) == 'e' || peek(lexer) == 'E')
    {
        advance(lexer);
        if (peek(lexer) == '+' || peek(lexer) == '-')
            advance(lexer);
        while (isdigit(peek(lexer)))
            advance(lexer);
    }

    return make_token(lexer, BAV_TOKEN_NUMBER);
}

static BavToken scan_identifier(BavLexer* lexer)
{
    while (isalnum(peek(lexer)) || peek(lexer) == '_')
    {
        advance(lexer);
    }

    BavTokenType type = check_keyword(lexer->start, lexer->current - lexer->start);
    return make_token(lexer, type);
}

static BavToken scan_token(BavLexer* lexer)
{
    skip_whitespace(lexer);
    lexer->start = lexer->current;

    if (is_at_end(lexer))
        return make_token(lexer, BAV_TOKEN_EOF);

    char c = advance(lexer);

    if (isalpha(c) || c == '_')
        return scan_identifier(lexer);
    if (isdigit(c))
        return scan_number(lexer);

    switch (c)
    {
        case '(':
            return make_token(lexer, BAV_TOKEN_LPAREN);
        case ')':
            return make_token(lexer, BAV_TOKEN_RPAREN);
        case '{':
            return make_token(lexer, BAV_TOKEN_LBRACE);
        case '}':
            return make_token(lexer, BAV_TOKEN_RBRACE);
        case '[':
            return make_token(lexer, BAV_TOKEN_LBRACKET);
        case ']':
            return make_token(lexer, BAV_TOKEN_RBRACKET);
        case ';':
            return make_token(lexer, BAV_TOKEN_SEMICOLON);
        case ',':
            return make_token(lexer, BAV_TOKEN_COMMA);
        case '+':
            return make_token(lexer, BAV_TOKEN_PLUS);
        case '-':
            return make_token(lexer, BAV_TOKEN_MINUS);
        case '*':
            return make_token(lexer, BAV_TOKEN_STAR);
        case '%':
            return make_token(lexer, BAV_TOKEN_PERCENT);
        case '^':
            return make_token(lexer, BAV_TOKEN_CARET);
        case '#':
            return make_token(lexer, BAV_TOKEN_HASH);
        case '&':
            return make_token(lexer, BAV_TOKEN_AMPERSAND);
        case '|':
            return make_token(lexer, BAV_TOKEN_PIPE);

        case '/':
            if (match(lexer, '/'))
                return make_token(lexer, BAV_TOKEN_SLASH_SLASH);
            return make_token(lexer, BAV_TOKEN_SLASH);

        case ':':
            if (match(lexer, ':'))
                return make_token(lexer, BAV_TOKEN_DOUBLE_COLON);
            return make_token(lexer, BAV_TOKEN_COLON);

        case '.':
            if (match(lexer, '.'))
            {
                if (match(lexer, '.'))
                    return make_token(lexer, BAV_TOKEN_DOT_DOT_DOT);
                return make_token(lexer, BAV_TOKEN_DOT_DOT);
            }
            if (isdigit(peek(lexer)))
                return scan_number(lexer);
            return make_token(lexer, BAV_TOKEN_DOT);

        case '=':
            if (match(lexer, '='))
                return make_token(lexer, BAV_TOKEN_EQ);
            return make_token(lexer, BAV_TOKEN_ASSIGN);

        case '~':
            if (match(lexer, '='))
                return make_token(lexer, BAV_TOKEN_NE);
            return make_token(lexer, BAV_TOKEN_TILDE);

        case '<':
            if (match(lexer, '='))
                return make_token(lexer, BAV_TOKEN_LE);
            if (match(lexer, '<'))
                return make_token(lexer, BAV_TOKEN_LT_LT);
            return make_token(lexer, BAV_TOKEN_LT);

        case '>':
            if (match(lexer, '='))
                return make_token(lexer, BAV_TOKEN_GE);
            if (match(lexer, '>'))
                return make_token(lexer, BAV_TOKEN_GT_GT);
            return make_token(lexer, BAV_TOKEN_GT);

        case '"':
        case '\'':
            return scan_string(lexer, c);
    }

    return error_token(lexer, "Unexpected character");
}

/* =============================================================================
 * Public API
 * ============================================================================= */

BavLexer* bav_lexer_create(const char* source, usize source_len)
{
    BavLexer* lexer = calloc(1, sizeof(BavLexer));
    if (!lexer)
        return NULL;

    lexer->source = source;
    lexer->start = source;
    lexer->current = source;
    lexer->source_len = source_len;
    lexer->line = 1;
    lexer->column = 1;
    lexer->has_peeked = 0;

    return lexer;
}

void bav_lexer_destroy(BavLexer* lexer)
{
    free(lexer);
}

BavToken bav_lexer_next_token(BavLexer* lexer)
{
    if (!lexer)
    {
        BavToken eof = {BAV_TOKEN_EOF, "", 0, 0, 0};
        return eof;
    }

    if (lexer->has_peeked)
    {
        lexer->has_peeked = 0;
        return lexer->peeked;
    }

    return scan_token(lexer);
}

BavToken bav_lexer_peek_token(BavLexer* lexer)
{
    if (!lexer)
    {
        BavToken eof = {BAV_TOKEN_EOF, "", 0, 0, 0};
        return eof;
    }

    if (!lexer->has_peeked)
    {
        lexer->peeked = scan_token(lexer);
        lexer->has_peeked = 1;
    }

    return lexer->peeked;
}
