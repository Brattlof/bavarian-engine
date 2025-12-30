/**
 * @file codegen.c
 * @brief Lua Bytecode Generation
 *
 * Generates bytecode from the analyzed AST.
 * The bytecode format is designed for efficient interpretation.
 *
 * Register allocation uses a simple stack-based approach.
 * Each expression result goes into the next available register.
 */

#include <bavarian/scripting.h>
#include <bavarian/types.h>
#include <stdlib.h>
#include <string.h>

/* =============================================================================
 * Bytecode Format
 * ============================================================================= */

/**
 * Opcode format: 32-bit instructions
 *
 * Format A:  [op:8][A:8][B:8][C:8]
 * Format AB: [op:8][A:8][Bx:16]
 * Format AX: [op:8][Ax:24]
 * Format AS: [op:8][A:8][sBx:16] (signed)
 */

typedef enum BavOpcode
{
    /* Load/Store */
    OP_LOADNIL,   /* A B     R[A], R[A+1], ..., R[A+B] := nil */
    OP_LOADTRUE,  /* A       R[A] := true */
    OP_LOADFALSE, /* A       R[A] := false */
    OP_LOADK,     /* A Bx    R[A] := K[Bx] */
    OP_MOVE,      /* A B     R[A] := R[B] */

    /* Upvalues */
    OP_GETUPVAL, /* A B     R[A] := UpValue[B] */
    OP_SETUPVAL, /* A B     UpValue[B] := R[A] */

    /* Tables */
    OP_NEWTABLE, /* A B C   R[A] := {} (size hint B=array, C=hash) */
    OP_GETTABLE, /* A B C   R[A] := R[B][RK(C)] */
    OP_SETTABLE, /* A B C   R[A][RK(B)] := RK(C) */
    OP_GETFIELD, /* A B C   R[A] := R[B].K[C] */
    OP_SETFIELD, /* A B C   R[A].K[B] := RK(C) */
    OP_SETLIST,  /* A B C   R[A][(C-1)*50+i] := R[A+i], 1 <= i <= B */

    /* Globals */
    OP_GETGLOBAL, /* A Bx    R[A] := _G[K[Bx]] */
    OP_SETGLOBAL, /* A Bx    _G[K[Bx]] := R[A] */

    /* Arithmetic */
    OP_ADD,  /* A B C   R[A] := RK(B) + RK(C) */
    OP_SUB,  /* A B C   R[A] := RK(B) - RK(C) */
    OP_MUL,  /* A B C   R[A] := RK(B) * RK(C) */
    OP_DIV,  /* A B C   R[A] := RK(B) / RK(C) */
    OP_IDIV, /* A B C   R[A] := RK(B) // RK(C) */
    OP_MOD,  /* A B C   R[A] := RK(B) % RK(C) */
    OP_POW,  /* A B C   R[A] := RK(B) ^ RK(C) */
    OP_UNM,  /* A B     R[A] := -R[B] */

    /* Bitwise */
    OP_BAND, /* A B C   R[A] := RK(B) & RK(C) */
    OP_BOR,  /* A B C   R[A] := RK(B) | RK(C) */
    OP_BXOR, /* A B C   R[A] := RK(B) ~ RK(C) */
    OP_BNOT, /* A B     R[A] := ~R[B] */
    OP_SHL,  /* A B C   R[A] := RK(B) << RK(C) */
    OP_SHR,  /* A B C   R[A] := RK(B) >> RK(C) */

    /* Comparison */
    OP_EQ, /* A B C   if (RK(B) == RK(C)) != A then pc++ */
    OP_LT, /* A B C   if (RK(B) <  RK(C)) != A then pc++ */
    OP_LE, /* A B C   if (RK(B) <= RK(C)) != A then pc++ */

    /* Logic */
    OP_NOT,    /* A B     R[A] := not R[B] */
    OP_LEN,    /* A B     R[A] := #R[B] */
    OP_CONCAT, /* A B C   R[A] := R[B] .. ... .. R[C] */

    /* Jumps */
    OP_JMP,     /* sBx     pc += sBx */
    OP_TEST,    /* A C     if (R[A] <=> C) then pc++ */
    OP_TESTSET, /* A B C   if (R[B] <=> C) then R[A] := R[B] else pc++ */

    /* Calls */
    OP_CALL,     /* A B C   R[A], ..., R[A+C-2] := R[A](R[A+1], ..., R[A+B-1]) */
    OP_TAILCALL, /* A B C   return R[A](R[A+1], ..., R[A+B-1]) */
    OP_RETURN,   /* A B     return R[A], ..., R[A+B-2] */
    OP_SELF,     /* A B C   R[A+1] := R[B]; R[A] := R[B][RK(C)] */

    /* Loops */
    OP_FORPREP,  /* A sBx   R[A] -= R[A+2]; pc += sBx */
    OP_FORLOOP,  /* A sBx   R[A] += R[A+2]; if R[A] <= R[A+1] then { pc += sBx; R[A+3] := R[A] } */
    OP_TFORCALL, /* A C     R[A+3], ..., R[A+2+C] := R[A](R[A+1], R[A+2]) */
    OP_TFORLOOP, /* A sBx   if R[A+1] ~= nil then { R[A] := R[A+1]; pc += sBx } */

    /* Closures */
    OP_CLOSURE, /* A Bx    R[A] := closure(Proto[Bx]) */
    OP_CLOSE,   /* A       close all variables in stack from R[A] to top */

    /* Vararg */
    OP_VARARG, /* A B     R[A], ..., R[A+B-2] := vararg */

    OP_COUNT
} BavOpcode;

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

typedef struct BavAstNode BavAstNode;

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

/* =============================================================================
 * Constant Pool
 * ============================================================================= */

typedef struct Constant
{
    BavValueType type;
    union
    {
        f64 number;
        struct
        {
            const char* data;
            usize length;
        } string;
        b8 boolean;
    };
} Constant;

/* =============================================================================
 * Local Variable Info
 * ============================================================================= */

typedef struct LocalVar
{
    const char* name;
    usize name_len;
    u32 start_pc; /* First instruction where variable is active */
    u32 end_pc;   /* Last instruction where variable is active */
    u8 reg;       /* Register holding this local */
} LocalVar;

/* =============================================================================
 * Upvalue Info
 * ============================================================================= */

typedef struct UpvalueInfo
{
    u8 index;    /* Index in enclosing function's locals or upvalues */
    b8 is_local; /* From enclosing local vs upvalue */
    const char* name;
    usize name_len;
} UpvalueInfo;

/* =============================================================================
 * Jump Patch List
 * ============================================================================= */

typedef struct JumpPatch
{
    u32 pc; /* Instruction to patch */
    struct JumpPatch* next;
} JumpPatch;

/* =============================================================================
 * Function Prototype
 * ============================================================================= */

typedef struct FunctionProto
{
    u32* code;
    u32 code_count;
    u32 code_capacity;

    Constant* constants;
    u32 constant_count;
    u32 constant_capacity;

    LocalVar* locals;
    u32 local_count;
    u32 local_capacity;

    UpvalueInfo* upvalues;
    u32 upvalue_count;
    u32 upvalue_capacity;

    struct FunctionProto** protos; /* Nested function prototypes */
    u32 proto_count;
    u32 proto_capacity;

    u32* lineinfo; /* Line number for each instruction */
    u32 lineinfo_capacity;

    u8 num_params;
    u8 is_vararg;
    u8 max_stack;

    const char* source;
    u32 line_defined;
    u32 last_line_defined;
} FunctionProto;

/* =============================================================================
 * Code Generator State
 * ============================================================================= */

typedef struct CodeGen
{
    FunctionProto* proto; /* Current function being compiled */

    LocalVar* active_locals;
    u32 active_local_count;
    u32 active_local_capacity;

    u8 free_reg;   /* Next free register */
    u8 num_actvar; /* Number of active local variables */

    struct CodeGen* enclosing; /* Enclosing function's codegen */

    /* Jump lists for control flow */
    JumpPatch* break_list;
    u32 loop_start;
    u32 scope_depth;

    /* Error tracking */
    BavCompileError* errors;
    u32 error_count;
    u32 error_capacity;
} CodeGen;

/* =============================================================================
 * Prototypes
 * ============================================================================= */

static void gen_expression(CodeGen* gen, BavAstNode* node, u8 reg);
static void gen_expression_result(CodeGen* gen, BavAstNode* node, u8 target, u32 num_results);
static void gen_statement(CodeGen* gen, BavAstNode* node);
static void gen_block(CodeGen* gen, BavAstNode* node);

/* =============================================================================
 * Utility Functions
 * ============================================================================= */

static void codegen_error(CodeGen* gen, u32 line, const char* message)
{
    if (gen->error_count >= gen->error_capacity)
    {
        u32 new_cap = gen->error_capacity * 2;
        if (new_cap == 0)
            new_cap = 8;
        BavCompileError* new_errors = realloc(gen->errors, new_cap * sizeof(BavCompileError));
        if (!new_errors)
            return;
        gen->errors = new_errors;
        gen->error_capacity = new_cap;
    }

    BavCompileError* err = &gen->errors[gen->error_count++];
    err->message = message;
    err->filename = NULL;
    err->line = line;
    err->column = 0;
    err->length = 0;
}

static FunctionProto* proto_create(void)
{
    FunctionProto* proto = calloc(1, sizeof(FunctionProto));
    if (!proto)
        return NULL;

    proto->code_capacity = 256;
    proto->code = calloc(proto->code_capacity, sizeof(u32));

    proto->constant_capacity = 64;
    proto->constants = calloc(proto->constant_capacity, sizeof(Constant));

    proto->local_capacity = 32;
    proto->locals = calloc(proto->local_capacity, sizeof(LocalVar));

    proto->upvalue_capacity = 16;
    proto->upvalues = calloc(proto->upvalue_capacity, sizeof(UpvalueInfo));

    proto->proto_capacity = 8;
    proto->protos = calloc(proto->proto_capacity, sizeof(FunctionProto*));

    proto->lineinfo_capacity = 256;
    proto->lineinfo = calloc(proto->lineinfo_capacity, sizeof(u32));

    return proto;
}

static void proto_destroy(FunctionProto* proto)
{
    if (!proto)
        return;

    free(proto->code);
    free(proto->constants);
    free(proto->locals);
    free(proto->upvalues);
    free(proto->lineinfo);

    for (u32 i = 0; i < proto->proto_count; i++)
    {
        proto_destroy(proto->protos[i]);
    }
    free(proto->protos);

    free(proto);
}

static u32 emit(CodeGen* gen, u32 instruction, u32 line)
{
    FunctionProto* proto = gen->proto;

    if (proto->code_count >= proto->code_capacity)
    {
        u32 new_cap = proto->code_capacity * 2;
        u32* new_code = realloc(proto->code, new_cap * sizeof(u32));
        if (!new_code)
            return 0;
        proto->code = new_code;
        proto->code_capacity = new_cap;

        u32* new_lines = realloc(proto->lineinfo, new_cap * sizeof(u32));
        if (new_lines)
        {
            proto->lineinfo = new_lines;
            proto->lineinfo_capacity = new_cap;
        }
    }

    u32 pc = proto->code_count++;
    proto->code[pc] = instruction;
    if (pc < proto->lineinfo_capacity)
    {
        proto->lineinfo[pc] = line;
    }

    return pc;
}

static u32 emit_ABC(CodeGen* gen, BavOpcode op, u8 a, u8 b, u8 c, u32 line)
{
    u32 instr = ((u32)op) | ((u32)a << 8) | ((u32)b << 16) | ((u32)c << 24);
    return emit(gen, instr, line);
}

static u32 emit_ABx(CodeGen* gen, BavOpcode op, u8 a, u16 bx, u32 line)
{
    u32 instr = ((u32)op) | ((u32)a << 8) | ((u32)bx << 16);
    return emit(gen, instr, line);
}

static u32 emit_AsBx(CodeGen* gen, BavOpcode op, u8 a, i32 sbx, u32 line)
{
    /* sBx is stored with bias of 0x7FFF */
    u16 encoded = (u16)(sbx + 0x7FFF);
    u32 instr = ((u32)op) | ((u32)a << 8) | ((u32)encoded << 16);
    return emit(gen, instr, line);
}

static void patch_jump(CodeGen* gen, u32 pc, i32 offset)
{
    /* Patch a jump instruction at pc to jump by offset */
    u32 instr = gen->proto->code[pc];
    BavOpcode op = (BavOpcode)(instr & 0xFF);
    u8 a = (instr >> 8) & 0xFF;
    u16 encoded = (u16)(offset + 0x7FFF);
    gen->proto->code[pc] = ((u32)op) | ((u32)a << 8) | ((u32)encoded << 16);
}

static u32 current_pc(CodeGen* gen)
{
    return gen->proto->code_count;
}

static u32 add_constant_number(CodeGen* gen, f64 value)
{
    FunctionProto* proto = gen->proto;

    /* Check for existing constant */
    for (u32 i = 0; i < proto->constant_count; i++)
    {
        if (proto->constants[i].type == BAV_VALUE_NUMBER && proto->constants[i].number == value)
        {
            return i;
        }
    }

    if (proto->constant_count >= proto->constant_capacity)
    {
        u32 new_cap = proto->constant_capacity * 2;
        Constant* new_consts = realloc(proto->constants, new_cap * sizeof(Constant));
        if (!new_consts)
            return 0;
        proto->constants = new_consts;
        proto->constant_capacity = new_cap;
    }

    u32 idx = proto->constant_count++;
    proto->constants[idx].type = BAV_VALUE_NUMBER;
    proto->constants[idx].number = value;
    return idx;
}

static u32 add_constant_string(CodeGen* gen, const char* str, usize len)
{
    FunctionProto* proto = gen->proto;

    /* Check for existing constant */
    for (u32 i = 0; i < proto->constant_count; i++)
    {
        if (proto->constants[i].type == BAV_VALUE_STRING &&
            proto->constants[i].string.length == len &&
            memcmp(proto->constants[i].string.data, str, len) == 0)
        {
            return i;
        }
    }

    if (proto->constant_count >= proto->constant_capacity)
    {
        u32 new_cap = proto->constant_capacity * 2;
        Constant* new_consts = realloc(proto->constants, new_cap * sizeof(Constant));
        if (!new_consts)
            return 0;
        proto->constants = new_consts;
        proto->constant_capacity = new_cap;
    }

    u32 idx = proto->constant_count++;
    proto->constants[idx].type = BAV_VALUE_STRING;
    proto->constants[idx].string.data = str;
    proto->constants[idx].string.length = len;
    return idx;
}

/* RK encoding: if value < 256, it's a register. Otherwise, K[value-256] */
#define RK_IS_K(x) ((x) >= 256)
#define RK_K(x) ((x) - 256)
#define RK_REG(x) (x)
#define MAKE_RK_K(k) ((k) + 256)

static u8 reserve_reg(CodeGen* gen)
{
    u8 reg = gen->free_reg++;
    if (gen->free_reg > gen->proto->max_stack)
    {
        gen->proto->max_stack = gen->free_reg;
    }
    return reg;
}

static void free_reg(CodeGen* gen, u8 reg)
{
    if (reg == gen->free_reg - 1)
    {
        gen->free_reg--;
    }
}

/* Find local variable by name */
static i32 find_local(CodeGen* gen, const char* name, usize len)
{
    for (i32 i = (i32)gen->active_local_count - 1; i >= 0; i--)
    {
        LocalVar* local = &gen->active_locals[i];
        if (local->name_len == len && memcmp(local->name, name, len) == 0)
        {
            return local->reg;
        }
    }
    return -1;
}

/* Find upvalue by name */
static i32 find_upvalue(CodeGen* gen, const char* name, usize len)
{
    FunctionProto* proto = gen->proto;
    for (u32 i = 0; i < proto->upvalue_count; i++)
    {
        if (proto->upvalues[i].name_len == len && memcmp(proto->upvalues[i].name, name, len) == 0)
        {
            return (i32)i;
        }
    }
    return -1;
}

/* Add upvalue */
static u32 add_upvalue(CodeGen* gen, const char* name, usize len, u8 index, b8 is_local)
{
    FunctionProto* proto = gen->proto;

    if (proto->upvalue_count >= proto->upvalue_capacity)
    {
        u32 new_cap = proto->upvalue_capacity * 2;
        UpvalueInfo* new_uvs = realloc(proto->upvalues, new_cap * sizeof(UpvalueInfo));
        if (!new_uvs)
            return 0;
        proto->upvalues = new_uvs;
        proto->upvalue_capacity = new_cap;
    }

    u32 idx = proto->upvalue_count++;
    proto->upvalues[idx].name = name;
    proto->upvalues[idx].name_len = len;
    proto->upvalues[idx].index = index;
    proto->upvalues[idx].is_local = is_local;
    return idx;
}

/* Add local variable */
static void add_local(CodeGen* gen, const char* name, usize len, u8 reg)
{
    if (gen->active_local_count >= gen->active_local_capacity)
    {
        u32 new_cap = gen->active_local_capacity * 2;
        if (new_cap == 0)
            new_cap = 32;
        LocalVar* new_locals = realloc(gen->active_locals, new_cap * sizeof(LocalVar));
        if (!new_locals)
            return;
        gen->active_locals = new_locals;
        gen->active_local_capacity = new_cap;
    }

    LocalVar* local = &gen->active_locals[gen->active_local_count++];
    local->name = name;
    local->name_len = len;
    local->reg = reg;
    local->start_pc = current_pc(gen);
    local->end_pc = 0;
    gen->num_actvar++;
}

/* =============================================================================
 * Expression Code Generation
 * ============================================================================= */

static void gen_expression(CodeGen* gen, BavAstNode* node, u8 reg)
{
    if (!node)
        return;

    switch (node->type)
    {
        case AST_NIL:
            emit_ABC(gen, OP_LOADNIL, reg, 0, 0, node->line);
            break;

        case AST_TRUE:
            emit_ABC(gen, OP_LOADTRUE, reg, 0, 0, node->line);
            break;

        case AST_FALSE:
            emit_ABC(gen, OP_LOADFALSE, reg, 0, 0, node->line);
            break;

        case AST_NUMBER:
        {
            u32 k = add_constant_number(gen, node->number.value);
            emit_ABx(gen, OP_LOADK, reg, (u16)k, node->line);
            break;
        }

        case AST_STRING:
        {
            u32 k = add_constant_string(gen, node->string.data, node->string.length);
            emit_ABx(gen, OP_LOADK, reg, (u16)k, node->line);
            break;
        }

        case AST_VAR:
        {
            i32 local = find_local(gen, node->var.name, node->var.name_len);
            if (local >= 0)
            {
                if ((u8)local != reg)
                {
                    emit_ABC(gen, OP_MOVE, reg, (u8)local, 0, node->line);
                }
            }
            else
            {
                i32 upval = find_upvalue(gen, node->var.name, node->var.name_len);
                if (upval >= 0)
                {
                    emit_ABC(gen, OP_GETUPVAL, reg, (u8)upval, 0, node->line);
                }
                else
                {
                    /* Global */
                    u32 k = add_constant_string(gen, node->var.name, node->var.name_len);
                    emit_ABx(gen, OP_GETGLOBAL, reg, (u16)k, node->line);
                }
            }
            break;
        }

        case AST_BINARY_OP:
        {
            BavTokenType op = node->binary.op;
            u8 b_reg = reserve_reg(gen);
            u8 c_reg = reserve_reg(gen);

            gen_expression(gen, node->binary.left, b_reg);
            gen_expression(gen, node->binary.right, c_reg);

            BavOpcode opcode;
            switch (op)
            {
                case BAV_TOKEN_PLUS:
                    opcode = OP_ADD;
                    break;
                case BAV_TOKEN_MINUS:
                    opcode = OP_SUB;
                    break;
                case BAV_TOKEN_STAR:
                    opcode = OP_MUL;
                    break;
                case BAV_TOKEN_SLASH:
                    opcode = OP_DIV;
                    break;
                case BAV_TOKEN_SLASH_SLASH:
                    opcode = OP_IDIV;
                    break;
                case BAV_TOKEN_PERCENT:
                    opcode = OP_MOD;
                    break;
                case BAV_TOKEN_CARET:
                    opcode = OP_POW;
                    break;
                case BAV_TOKEN_AMPERSAND:
                    opcode = OP_BAND;
                    break;
                case BAV_TOKEN_PIPE:
                    opcode = OP_BOR;
                    break;
                case BAV_TOKEN_TILDE:
                    opcode = OP_BXOR;
                    break;
                case BAV_TOKEN_LT_LT:
                    opcode = OP_SHL;
                    break;
                case BAV_TOKEN_GT_GT:
                    opcode = OP_SHR;
                    break;
                case BAV_TOKEN_DOT_DOT:
                    opcode = OP_CONCAT;
                    break;
                case BAV_TOKEN_EQ:
                    opcode = OP_EQ;
                    break;
                case BAV_TOKEN_NE:
                    opcode = OP_EQ;
                    break;
                case BAV_TOKEN_LT:
                    opcode = OP_LT;
                    break;
                case BAV_TOKEN_LE:
                    opcode = OP_LE;
                    break;
                case BAV_TOKEN_GT:
                    opcode = OP_LT;
                    break; /* Swap args */
                case BAV_TOKEN_GE:
                    opcode = OP_LE;
                    break; /* Swap args */
                default:
                    free_reg(gen, c_reg);
                    free_reg(gen, b_reg);
                    return;
            }

            /* Handle comparison operators specially */
            if (op == BAV_TOKEN_EQ || op == BAV_TOKEN_NE || op == BAV_TOKEN_LT ||
                op == BAV_TOKEN_LE || op == BAV_TOKEN_GT || op == BAV_TOKEN_GE)
            {
                /* Comparisons use TEST pattern */
                u8 invert = (op == BAV_TOKEN_NE) ? 1 : 0;
                u8 left = (op == BAV_TOKEN_GT || op == BAV_TOKEN_GE) ? c_reg : b_reg;
                u8 right = (op == BAV_TOKEN_GT || op == BAV_TOKEN_GE) ? b_reg : c_reg;

                emit_ABC(gen, opcode, invert, left, right, node->line);
                emit_AsBx(gen, OP_JMP, 0, 1, node->line); /* Skip next */
                emit_ABC(gen, OP_LOADFALSE, reg, 0, 0, node->line);
                emit_AsBx(gen, OP_JMP, 0, 1, node->line); /* Skip next */
                emit_ABC(gen, OP_LOADTRUE, reg, 0, 0, node->line);
            }
            else
            {
                emit_ABC(gen, opcode, reg, b_reg, c_reg, node->line);
            }

            free_reg(gen, c_reg);
            free_reg(gen, b_reg);
            break;
        }

        case AST_UNARY_OP:
        {
            BavTokenType op = node->unary.op;
            u8 b_reg = reserve_reg(gen);
            gen_expression(gen, node->unary.operand, b_reg);

            BavOpcode opcode;
            switch (op)
            {
                case BAV_TOKEN_MINUS:
                    opcode = OP_UNM;
                    break;
                case BAV_TOKEN_NOT:
                    opcode = OP_NOT;
                    break;
                case BAV_TOKEN_HASH:
                    opcode = OP_LEN;
                    break;
                case BAV_TOKEN_TILDE:
                    opcode = OP_BNOT;
                    break;
                default:
                    free_reg(gen, b_reg);
                    return;
            }

            emit_ABC(gen, opcode, reg, b_reg, 0, node->line);
            free_reg(gen, b_reg);
            break;
        }

        case AST_INDEX:
        {
            u8 obj_reg = reserve_reg(gen);
            u8 key_reg = reserve_reg(gen);
            gen_expression(gen, node->index.object, obj_reg);
            gen_expression(gen, node->index.key, key_reg);
            emit_ABC(gen, OP_GETTABLE, reg, obj_reg, key_reg, node->line);
            free_reg(gen, key_reg);
            free_reg(gen, obj_reg);
            break;
        }

        case AST_FIELD:
        {
            u8 obj_reg = reserve_reg(gen);
            gen_expression(gen, node->field.object, obj_reg);
            u32 k = add_constant_string(gen, node->field.name, node->field.name_len);
            emit_ABC(gen, OP_GETFIELD, reg, obj_reg, (u8)k, node->line);
            free_reg(gen, obj_reg);
            break;
        }

        case AST_CALL:
        {
            u8 func_reg = reserve_reg(gen);
            gen_expression(gen, node->call.callee, func_reg);

            /* Arguments go in consecutive registers after function */
            for (u32 i = 0; i < node->call.arg_count; i++)
            {
                u8 arg_reg = reserve_reg(gen);
                gen_expression(gen, node->call.args[i], arg_reg);
            }

            /* B = arg_count + 1, C = 2 (one result into reg) */
            emit_ABC(gen, OP_CALL, func_reg, (u8)(node->call.arg_count + 1), 2, node->line);

            /* Move result to target if needed */
            if (func_reg != reg)
            {
                emit_ABC(gen, OP_MOVE, reg, func_reg, 0, node->line);
            }

            /* Free temp registers */
            gen->free_reg = func_reg;
            break;
        }

        case AST_METHOD_CALL:
        {
            u8 self_reg = reserve_reg(gen);
            u8 func_reg = reserve_reg(gen);

            gen_expression(gen, node->method_call.object, self_reg);
            u32 k =
                add_constant_string(gen, node->method_call.method, node->method_call.method_len);

            /* SELF: R[func] := R[self][K[k]]; R[func+1] := R[self] */
            emit_ABC(gen, OP_SELF, func_reg, self_reg, (u8)k, node->line);

            /* Arguments */
            for (u32 i = 0; i < node->method_call.arg_count; i++)
            {
                u8 arg_reg = reserve_reg(gen);
                gen_expression(gen, node->method_call.args[i], arg_reg);
            }

            /* B = arg_count + 2 (include self), C = 2 */
            emit_ABC(gen, OP_CALL, func_reg, (u8)(node->method_call.arg_count + 2), 2, node->line);

            if (func_reg != reg)
            {
                emit_ABC(gen, OP_MOVE, reg, func_reg, 0, node->line);
            }

            gen->free_reg = self_reg;
            break;
        }

        case AST_TABLE:
        {
            /* Count array and hash parts */
            u32 array_count = 0;
            u32 hash_count = 0;
            for (u32 i = 0; i < node->table.field_count; i++)
            {
                BavAstNode* fld = node->table.fields[i];
                if (fld->table_field.key)
                    hash_count++;
                else
                    array_count++;
            }

            emit_ABC(gen, OP_NEWTABLE, reg, (u8)array_count, (u8)hash_count, node->line);

            u32 array_idx = 1;
            for (u32 i = 0; i < node->table.field_count; i++)
            {
                BavAstNode* fld = node->table.fields[i];

                if (fld->table_field.key)
                {
                    /* Hash field */
                    u8 key_reg = reserve_reg(gen);
                    u8 val_reg = reserve_reg(gen);
                    gen_expression(gen, fld->table_field.key, key_reg);
                    gen_expression(gen, fld->table_field.value, val_reg);
                    emit_ABC(gen, OP_SETTABLE, reg, key_reg, val_reg, node->line);
                    free_reg(gen, val_reg);
                    free_reg(gen, key_reg);
                }
                else
                {
                    /* Array field */
                    u8 val_reg = reserve_reg(gen);
                    gen_expression(gen, fld->table_field.value, val_reg);
                    u32 k = add_constant_number(gen, (f64)array_idx++);
                    emit_ABC(gen, OP_SETTABLE, reg, (u8)(k + 256), val_reg, node->line);
                    free_reg(gen, val_reg);
                }
            }
            break;
        }

        case AST_FUNCTION_EXPR:
        {
            /* Compile nested function */
            FunctionProto* func_proto = proto_create();
            if (!func_proto)
                break;

            func_proto->num_params = (u8)node->function.param_count;
            func_proto->is_vararg = node->function.is_vararg;
            func_proto->line_defined = node->line;

            /* Add proto to parent */
            FunctionProto* parent = gen->proto;
            if (parent->proto_count >= parent->proto_capacity)
            {
                u32 new_cap = parent->proto_capacity * 2;
                FunctionProto** new_protos =
                    realloc(parent->protos, new_cap * sizeof(FunctionProto*));
                if (new_protos)
                {
                    parent->protos = new_protos;
                    parent->proto_capacity = new_cap;
                }
            }
            u32 proto_idx = parent->proto_count++;
            parent->protos[proto_idx] = func_proto;

            /* Create child codegen */
            CodeGen child;
            memset(&child, 0, sizeof(child));
            child.proto = func_proto;
            child.enclosing = gen;

            /* Add parameters as locals */
            for (u32 i = 0; i < node->function.param_count; i++)
            {
                u8 param_reg = reserve_reg(&child);
                add_local(&child, node->function.params[i], node->function.param_lens[i],
                          param_reg);
            }

            /* Generate body */
            gen_block(&child, node->function.body);

            /* Ensure return at end */
            emit_ABC(&child, OP_RETURN, 0, 1, 0, node->line);

            func_proto->last_line_defined = node->line;

            free(child.active_locals);

            /* Emit closure instruction */
            emit_ABx(gen, OP_CLOSURE, reg, (u16)proto_idx, node->line);
            break;
        }

        case AST_VARARG:
        {
            emit_ABC(gen, OP_VARARG, reg, 2, 0, node->line); /* 2 = one result */
            break;
        }

        default:
            break;
    }
}

/* =============================================================================
 * Statement Code Generation
 * ============================================================================= */

static void gen_statement(CodeGen* gen, BavAstNode* node)
{
    if (!node)
        return;

    switch (node->type)
    {
        case AST_LOCAL:
        {
            /* Generate values first */
            u32 num_names = node->local.name_count;
            u32 num_values = node->local.value_count;

            u8 base_reg = gen->free_reg;

            for (u32 i = 0; i < num_values && i < num_names; i++)
            {
                u8 reg = reserve_reg(gen);
                gen_expression(gen, node->local.values[i], reg);
            }

            /* Fill remaining with nil */
            if (num_names > num_values)
            {
                u8 first_nil = (u8)(base_reg + num_values);
                emit_ABC(gen, OP_LOADNIL, first_nil, (u8)(num_names - num_values - 1), 0,
                         node->line);
                gen->free_reg = (u8)(base_reg + num_names);
            }

            /* Register locals */
            for (u32 i = 0; i < num_names; i++)
            {
                add_local(gen, node->local.names[i], node->local.name_lens[i], (u8)(base_reg + i));
            }
            break;
        }

        case AST_ASSIGN:
        {
            /* Generate values */
            u32 num_targets = node->assign.target_count;
            u32 num_values = node->assign.value_count;

            u8 base_reg = gen->free_reg;

            for (u32 i = 0; i < num_values; i++)
            {
                u8 reg = reserve_reg(gen);
                gen_expression(gen, node->assign.values[i], reg);
            }

            /* Assign to targets */
            for (u32 i = 0; i < num_targets; i++)
            {
                BavAstNode* target = node->assign.targets[i];
                u8 val_reg = (i < num_values) ? (u8)(base_reg + i) : base_reg;

                if (target->type == AST_VAR)
                {
                    i32 local = find_local(gen, target->var.name, target->var.name_len);
                    if (local >= 0)
                    {
                        emit_ABC(gen, OP_MOVE, (u8)local, val_reg, 0, node->line);
                    }
                    else
                    {
                        i32 upval = find_upvalue(gen, target->var.name, target->var.name_len);
                        if (upval >= 0)
                        {
                            emit_ABC(gen, OP_SETUPVAL, val_reg, (u8)upval, 0, node->line);
                        }
                        else
                        {
                            u32 k =
                                add_constant_string(gen, target->var.name, target->var.name_len);
                            emit_ABx(gen, OP_SETGLOBAL, val_reg, (u16)k, node->line);
                        }
                    }
                }
                else if (target->type == AST_INDEX)
                {
                    u8 obj_reg = reserve_reg(gen);
                    u8 key_reg = reserve_reg(gen);
                    gen_expression(gen, target->index.object, obj_reg);
                    gen_expression(gen, target->index.key, key_reg);
                    emit_ABC(gen, OP_SETTABLE, obj_reg, key_reg, val_reg, node->line);
                    free_reg(gen, key_reg);
                    free_reg(gen, obj_reg);
                }
                else if (target->type == AST_FIELD)
                {
                    u8 obj_reg = reserve_reg(gen);
                    gen_expression(gen, target->field.object, obj_reg);
                    u32 k = add_constant_string(gen, target->field.name, target->field.name_len);
                    emit_ABC(gen, OP_SETFIELD, obj_reg, (u8)k, val_reg, node->line);
                    free_reg(gen, obj_reg);
                }
            }

            gen->free_reg = base_reg;
            break;
        }

        case AST_IF:
        {
            /* Generate condition */
            u8 cond_reg = reserve_reg(gen);
            gen_expression(gen, node->if_stmt.condition, cond_reg);

            /* TEST and JMP to else/end */
            emit_ABC(gen, OP_TEST, cond_reg, 0, 0, node->line);
            u32 then_jump = emit_AsBx(gen, OP_JMP, 0, 0, node->line);

            free_reg(gen, cond_reg);

            /* Then block */
            gen_block(gen, node->if_stmt.then_block);

            /* Jump over else parts */
            u32 end_jump = emit_AsBx(gen, OP_JMP, 0, 0, node->line);

            /* Patch then_jump to here */
            patch_jump(gen, then_jump, (i32)(current_pc(gen) - then_jump - 1));

            /* Elseif blocks */
            u32* elseif_jumps = NULL;
            if (node->if_stmt.elseif_count > 0)
            {
                elseif_jumps = malloc(node->if_stmt.elseif_count * sizeof(u32));
            }

            for (u32 i = 0; i < node->if_stmt.elseif_count; i++)
            {
                cond_reg = reserve_reg(gen);
                gen_expression(gen, node->if_stmt.elseif_conds[i], cond_reg);
                emit_ABC(gen, OP_TEST, cond_reg, 0, 0, node->line);
                u32 elseif_jump = emit_AsBx(gen, OP_JMP, 0, 0, node->line);
                free_reg(gen, cond_reg);

                gen_block(gen, node->if_stmt.elseif_blocks[i]);

                if (elseif_jumps)
                    elseif_jumps[i] = emit_AsBx(gen, OP_JMP, 0, 0, node->line);

                patch_jump(gen, elseif_jump, (i32)(current_pc(gen) - elseif_jump - 1));
            }

            /* Else block */
            if (node->if_stmt.else_block)
            {
                gen_block(gen, node->if_stmt.else_block);
            }

            /* Patch end jumps */
            patch_jump(gen, end_jump, (i32)(current_pc(gen) - end_jump - 1));
            for (u32 i = 0; i < node->if_stmt.elseif_count && elseif_jumps; i++)
            {
                patch_jump(gen, elseif_jumps[i], (i32)(current_pc(gen) - elseif_jumps[i] - 1));
            }
            free(elseif_jumps);
            break;
        }

        case AST_WHILE:
        {
            u32 loop_start_pc = current_pc(gen);
            gen->loop_start = loop_start_pc;

            u8 cond_reg = reserve_reg(gen);
            gen_expression(gen, node->while_loop.condition, cond_reg);
            emit_ABC(gen, OP_TEST, cond_reg, 0, 0, node->line);
            u32 exit_jump = emit_AsBx(gen, OP_JMP, 0, 0, node->line);
            free_reg(gen, cond_reg);

            /* Save break list */
            JumpPatch* old_break = gen->break_list;
            gen->break_list = NULL;

            gen_block(gen, node->while_loop.body);

            /* Jump back to condition */
            emit_AsBx(gen, OP_JMP, 0, (i32)(loop_start_pc - current_pc(gen) - 1), node->line);

            /* Patch exit */
            patch_jump(gen, exit_jump, (i32)(current_pc(gen) - exit_jump - 1));

            /* Patch breaks */
            JumpPatch* brk = gen->break_list;
            while (brk)
            {
                patch_jump(gen, brk->pc, (i32)(current_pc(gen) - brk->pc - 1));
                JumpPatch* next = brk->next;
                free(brk);
                brk = next;
            }
            gen->break_list = old_break;
            break;
        }

        case AST_REPEAT:
        {
            u32 loop_start_pc = current_pc(gen);

            JumpPatch* old_break = gen->break_list;
            gen->break_list = NULL;

            gen_block(gen, node->repeat_loop.body);

            u8 cond_reg = reserve_reg(gen);
            gen_expression(gen, node->repeat_loop.condition, cond_reg);
            emit_ABC(gen, OP_TEST, cond_reg, 0, 1, node->line); /* Test for false */
            emit_AsBx(gen, OP_JMP, 0, (i32)(loop_start_pc - current_pc(gen) - 1), node->line);
            free_reg(gen, cond_reg);

            /* Patch breaks */
            JumpPatch* brk = gen->break_list;
            while (brk)
            {
                patch_jump(gen, brk->pc, (i32)(current_pc(gen) - brk->pc - 1));
                JumpPatch* next = brk->next;
                free(brk);
                brk = next;
            }
            gen->break_list = old_break;
            break;
        }

        case AST_FOR_NUMERIC:
        {
            /* For numeric: for var = start, limit, step do body end
             * Uses 3 internal vars + 1 external var */
            u8 base = gen->free_reg;

            /* Generate start, limit, step */
            u8 init_reg = reserve_reg(gen);
            u8 limit_reg = reserve_reg(gen);
            u8 step_reg = reserve_reg(gen);
            u8 var_reg = reserve_reg(gen);

            gen_expression(gen, node->for_numeric.start, init_reg);
            gen_expression(gen, node->for_numeric.limit, limit_reg);
            if (node->for_numeric.step)
            {
                gen_expression(gen, node->for_numeric.step, step_reg);
            }
            else
            {
                u32 k = add_constant_number(gen, 1.0);
                emit_ABx(gen, OP_LOADK, step_reg, (u16)k, node->line);
            }

            /* FORPREP */
            u32 prep_pc = emit_AsBx(gen, OP_FORPREP, base, 0, node->line);

            /* Register loop variable */
            add_local(gen, node->for_numeric.var_name, node->for_numeric.var_len, var_reg);

            JumpPatch* old_break = gen->break_list;
            gen->break_list = NULL;

            gen_block(gen, node->for_numeric.body);

            /* FORLOOP */
            u32 loop_pc = current_pc(gen);
            emit_AsBx(gen, OP_FORLOOP, base, (i32)(prep_pc + 1 - loop_pc - 1), node->line);

            /* Patch FORPREP */
            patch_jump(gen, prep_pc, (i32)(loop_pc - prep_pc - 1));

            /* Patch breaks */
            JumpPatch* brk = gen->break_list;
            while (brk)
            {
                patch_jump(gen, brk->pc, (i32)(current_pc(gen) - brk->pc - 1));
                JumpPatch* next = brk->next;
                free(brk);
                brk = next;
            }
            gen->break_list = old_break;

            gen->free_reg = base;
            gen->active_local_count--; /* Remove loop var */
            break;
        }

        case AST_FOR_GENERIC:
        {
            /* For generic: for var1, var2, ... in explist do body end */
            u8 base = gen->free_reg;

            /* Generate iterator expressions (expecting 3: iter func, state, initial) */
            for (u32 i = 0; i < node->for_generic.iterator_count && i < 3; i++)
            {
                u8 reg = reserve_reg(gen);
                gen_expression(gen, node->for_generic.iterators[i], reg);
            }

            /* Pad with nils if needed */
            while (gen->free_reg < base + 3)
            {
                u8 reg = reserve_reg(gen);
                emit_ABC(gen, OP_LOADNIL, reg, 0, 0, node->line);
            }

            /* Reserve space for loop vars */
            u8 var_base = gen->free_reg;
            for (u32 i = 0; i < node->for_generic.var_count; i++)
            {
                reserve_reg(gen);
                add_local(gen, node->for_generic.var_names[i], node->for_generic.var_lens[i],
                          (u8)(var_base + i));
            }

            /* Jump to TFORLOOP check */
            u32 prep_jump = emit_AsBx(gen, OP_JMP, 0, 0, node->line);

            u32 loop_start_pc = current_pc(gen);

            JumpPatch* old_break = gen->break_list;
            gen->break_list = NULL;

            gen_block(gen, node->for_generic.body);

            /* Patch prep_jump */
            patch_jump(gen, prep_jump, (i32)(current_pc(gen) - prep_jump - 1));

            /* TFORCALL */
            emit_ABC(gen, OP_TFORCALL, base, 0, (u8)node->for_generic.var_count, node->line);

            /* TFORLOOP */
            emit_AsBx(gen, OP_TFORLOOP, (u8)(base + 2), (i32)(loop_start_pc - current_pc(gen) - 1),
                      node->line);

            /* Patch breaks */
            JumpPatch* brk = gen->break_list;
            while (brk)
            {
                patch_jump(gen, brk->pc, (i32)(current_pc(gen) - brk->pc - 1));
                JumpPatch* next = brk->next;
                free(brk);
                brk = next;
            }
            gen->break_list = old_break;

            /* Remove loop vars */
            gen->active_local_count -= node->for_generic.var_count;
            gen->free_reg = base;
            break;
        }

        case AST_FUNCTION_DEF:
        case AST_LOCAL_FUNCTION:
        {
            /* For local function, register name first */
            u8 func_reg = reserve_reg(gen);
            if (node->type == AST_LOCAL_FUNCTION)
            {
                add_local(gen, node->function.name, node->function.name_len, func_reg);
            }

            /* Generate function as expression */
            BavAstNode func_expr = *node;
            func_expr.type = AST_FUNCTION_EXPR;
            gen_expression(gen, &func_expr, func_reg);

            /* For global function, store to global */
            if (node->type == AST_FUNCTION_DEF)
            {
                u32 k = add_constant_string(gen, node->function.name, node->function.name_len);
                emit_ABx(gen, OP_SETGLOBAL, func_reg, (u16)k, node->line);
                free_reg(gen, func_reg);
            }
            break;
        }

        case AST_RETURN:
        {
            if (node->return_stmt.count == 0)
            {
                emit_ABC(gen, OP_RETURN, 0, 1, 0, node->line);
            }
            else
            {
                u8 base = gen->free_reg;
                for (u32 i = 0; i < node->return_stmt.count; i++)
                {
                    u8 reg = reserve_reg(gen);
                    gen_expression(gen, node->return_stmt.values[i], reg);
                }
                emit_ABC(gen, OP_RETURN, base, (u8)(node->return_stmt.count + 1), 0, node->line);
                gen->free_reg = base;
            }
            break;
        }

        case AST_BREAK:
        {
            /* Add to break list */
            JumpPatch* brk = malloc(sizeof(JumpPatch));
            if (brk)
            {
                brk->pc = emit_AsBx(gen, OP_JMP, 0, 0, node->line);
                brk->next = gen->break_list;
                gen->break_list = brk;
            }
            break;
        }

        case AST_DO:
        {
            gen->scope_depth++;
            u32 old_local_count = gen->active_local_count;

            for (u32 i = 0; i < node->block.count; i++)
            {
                gen_statement(gen, node->block.statements[i]);
            }

            /* Pop locals from this scope */
            gen->active_local_count = old_local_count;
            gen->scope_depth--;
            break;
        }

        case AST_CALL_STMT:
        {
            /* Call as statement - discard results */
            u8 func_reg = reserve_reg(gen);
            gen_expression(gen, node->call.callee, func_reg);

            for (u32 i = 0; i < node->call.arg_count; i++)
            {
                u8 arg_reg = reserve_reg(gen);
                gen_expression(gen, node->call.args[i], arg_reg);
            }

            emit_ABC(gen, OP_CALL, func_reg, (u8)(node->call.arg_count + 1), 1, node->line);
            gen->free_reg = func_reg;
            break;
        }

        default:
            /* Expression as statement */
            {
                u8 reg = reserve_reg(gen);
                gen_expression(gen, node, reg);
                free_reg(gen, reg);
            }
            break;
    }
}

/* =============================================================================
 * Block Code Generation
 * ============================================================================= */

static void gen_block(CodeGen* gen, BavAstNode* node)
{
    if (!node)
        return;

    gen->scope_depth++;
    u32 old_local_count = gen->active_local_count;

    for (u32 i = 0; i < node->block.count; i++)
    {
        gen_statement(gen, node->block.statements[i]);
    }

    /* Pop locals */
    gen->active_local_count = old_local_count;
    gen->scope_depth--;
}

/* =============================================================================
 * Public API
 * ============================================================================= */

/**
 * Generate bytecode for an AST.
 *
 * @param ast         The AST root node
 * @param errors      Output: array of codegen errors
 * @param error_count Output: number of errors
 * @return Function prototype, or NULL on failure
 */
FunctionProto* bav_codegen_lua(BavAstNode* ast, BavCompileError** errors, u32* error_count)
{
    CodeGen gen;
    memset(&gen, 0, sizeof(gen));

    gen.proto = proto_create();
    if (!gen.proto)
    {
        *errors = NULL;
        *error_count = 0;
        return NULL;
    }

    gen.proto->is_vararg = 1; /* Main chunk is vararg */
    gen.proto->source = "<main>";
    gen.proto->line_defined = 1;

    gen.active_local_capacity = 32;
    gen.active_locals = calloc(gen.active_local_capacity, sizeof(LocalVar));

    /* Generate code */
    if (ast->type == AST_CHUNK || ast->type == AST_BLOCK)
    {
        for (u32 i = 0; i < ast->block.count; i++)
        {
            gen_statement(&gen, ast->block.statements[i]);
        }
    }
    else
    {
        gen_statement(&gen, ast);
    }

    /* Ensure return at end */
    emit_ABC(&gen, OP_RETURN, 0, 1, 0, 0);

    gen.proto->last_line_defined = gen.proto->lineinfo[gen.proto->code_count - 1];

    *errors = gen.errors;
    *error_count = gen.error_count;

    free(gen.active_locals);

    return gen.proto;
}

/**
 * Free a function prototype.
 */
void bav_proto_free(FunctionProto* proto)
{
    proto_destroy(proto);
}
