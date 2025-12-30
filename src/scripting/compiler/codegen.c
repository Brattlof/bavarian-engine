/**
 * @file codegen.c
 * @brief Lua Bytecode Generation
 *
 * Generates bytecode from the analyzed AST.
 * The bytecode format is designed for efficient interpretation.
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
    OP_NEWTABLE, /* A B C   R[A] := {} (size hint B, C) */
    OP_GETTABLE, /* A B C   R[A] := R[B][RK(C)] */
    OP_SETTABLE, /* A B C   R[A][RK(B)] := RK(C) */
    OP_GETFIELD, /* A B C   R[A] := R[B].K[C] */
    OP_SETFIELD, /* A B C   R[A].K[B] := RK(C) */

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

    /* Loops */
    OP_FORPREP,  /* A sBx   R[A] -= R[A+2]; pc += sBx */
    OP_FORLOOP,  /* A sBx   R[A] += R[A+2]; if R[A] <= R[A+1] then pc += sBx; R[A+3] := R[A] */
    OP_TFORCALL, /* A C     R[A+3], ..., R[A+2+C] := R[A](R[A+1], R[A+2]) */
    OP_TFORLOOP, /* A sBx   if R[A+1] ~= nil then R[A] := R[A+1]; pc += sBx */

    /* Closures */
    OP_CLOSURE, /* A Bx    R[A] := closure(Proto[Bx]) */
    OP_CLOSE,   /* A       close all variables in stack from R[A] to top */

    /* Vararg */
    OP_VARARG, /* A B     R[A], ..., R[A+B-2] := vararg */

    OP_COUNT
} BavOpcode;

/* =============================================================================
 * Code Generator State
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

typedef struct CodeGen
{
    u32* code;
    u32 code_count;
    u32 code_capacity;

    Constant* constants;
    u32 constant_count;
    u32 constant_capacity;

    u32 max_stack;
    u32 current_stack;
} CodeGen;

/* =============================================================================
 * Code Generation Utilities
 * ============================================================================= */

static void emit_byte(CodeGen* gen, u32 instruction)
{
    if (!gen)
        return;

    if (gen->code_count >= gen->code_capacity)
    {
        u32 new_cap = gen->code_capacity * 2;
        if (new_cap == 0)
            new_cap = 256;
        u32* new_code = realloc(gen->code, new_cap * sizeof(u32));
        if (!new_code)
            return;
        gen->code = new_code;
        gen->code_capacity = new_cap;
    }

    gen->code[gen->code_count++] = instruction;
}

static u32 make_instruction_ABC(BavOpcode op, u8 a, u8 b, u8 c)
{
    return ((u32)op) | ((u32)a << 8) | ((u32)b << 16) | ((u32)c << 24);
}

static u32 make_instruction_ABx(BavOpcode op, u8 a, u16 bx)
{
    return ((u32)op) | ((u32)a << 8) | ((u32)bx << 16);
}

static u32 make_instruction_AsBx(BavOpcode op, u8 a, i16 sbx)
{
    /* sBx is stored with bias to make it unsigned in the encoding */
    u16 encoded = (u16)(sbx + 0x7FFF);
    return ((u32)op) | ((u32)a << 8) | ((u32)encoded << 16);
}

static u32 add_constant(CodeGen* gen, Constant* constant)
{
    if (!gen || !constant)
        return 0;

    if (gen->constant_count >= gen->constant_capacity)
    {
        u32 new_cap = gen->constant_capacity * 2;
        if (new_cap == 0)
            new_cap = 64;
        Constant* new_constants = realloc(gen->constants, new_cap * sizeof(Constant));
        if (!new_constants)
            return 0;
        gen->constants = new_constants;
        gen->constant_capacity = new_cap;
    }

    u32 index = gen->constant_count++;
    gen->constants[index] = *constant;
    return index;
}

/* =============================================================================
 * Code Generation Entry Point
 * ============================================================================= */

static CodeGen* codegen_create(void)
{
    CodeGen* gen = calloc(1, sizeof(CodeGen));
    if (!gen)
        return NULL;

    gen->code_capacity = 256;
    gen->code = calloc(gen->code_capacity, sizeof(u32));

    gen->constant_capacity = 64;
    gen->constants = calloc(gen->constant_capacity, sizeof(Constant));

    if (!gen->code || !gen->constants)
    {
        free(gen->code);
        free(gen->constants);
        free(gen);
        return NULL;
    }

    return gen;
}

static void codegen_destroy(CodeGen* gen)
{
    if (!gen)
        return;
    free(gen->code);
    free(gen->constants);
    free(gen);
}

/* Code generator is internal - used by compiler pipeline */
