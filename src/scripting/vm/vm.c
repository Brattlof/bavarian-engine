/**
 * @file vm.c
 * @brief Bavarian Lua Virtual Machine
 *
 * The heart of the scripting system - a register-based bytecode interpreter.
 * Designed for reasonable performance while keeping the code maintainable.
 *
 * Register allocation is done by the compiler. The VM just executes instructions
 * and manages the call stack. Tables and closures are heap-allocated with
 * reference counting for deterministic cleanup (proper GC can come later).
 *
 * The dispatch loop uses a switch-case which the compiler should optimize
 * into a jump table. Could move to computed gotos later if it matters.
 */

#include <bavarian/scripting.h>
#include <bavarian/types.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =============================================================================
 * Opcodes (must match codegen.c exactly)
 * ============================================================================= */

typedef enum BavOpcode
{
    OP_LOADNIL = 0,
    OP_LOADTRUE,
    OP_LOADFALSE,
    OP_LOADK,
    OP_MOVE,

    OP_GETUPVAL,
    OP_SETUPVAL,

    OP_NEWTABLE,
    OP_GETTABLE,
    OP_SETTABLE,
    OP_GETFIELD,
    OP_SETFIELD,
    OP_SETLIST,

    OP_GETGLOBAL,
    OP_SETGLOBAL,

    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_IDIV,
    OP_MOD,
    OP_POW,
    OP_UNM,

    OP_BAND,
    OP_BOR,
    OP_BXOR,
    OP_BNOT,
    OP_SHL,
    OP_SHR,

    OP_EQ,
    OP_LT,
    OP_LE,

    OP_NOT,
    OP_LEN,
    OP_CONCAT,

    OP_JMP,
    OP_TEST,
    OP_TESTSET,

    OP_CALL,
    OP_TAILCALL,
    OP_RETURN,
    OP_SELF,

    OP_FORPREP,
    OP_FORLOOP,
    OP_TFORCALL,
    OP_TFORLOOP,

    OP_CLOSURE,
    OP_CLOSE,

    OP_VARARG,

    OP_COUNT
} BavOpcode;

/* =============================================================================
 * Instruction Decoding
 * ============================================================================= */

#define GET_OPCODE(i) ((BavOpcode)((i) & 0xFF))
#define GET_A(i) (((i) >> 8) & 0xFF)
#define GET_B(i) (((i) >> 16) & 0xFF)
#define GET_C(i) (((i) >> 24) & 0xFF)
#define GET_Bx(i) (((i) >> 16) & 0xFFFF)
#define GET_sBx(i) ((i32)(((i) >> 16) & 0xFFFF) - 0x7FFF)

/* RK encoding: if bit 8 set, it's a constant index */
#define RK_IS_K(x) ((x) & 0x100)
#define RK_INDEX(x) ((x) & 0xFF)

/* =============================================================================
 * Value Types
 * ============================================================================= */

/* Forward declarations */
typedef struct LuaTable LuaTable;
typedef struct LuaClosure LuaClosure;
typedef struct LuaString LuaString;
typedef struct LuaUpvalue LuaUpvalue;

typedef enum ValueTag
{
    TAG_NIL = 0,
    TAG_BOOL,
    TAG_NUMBER,
    TAG_STRING,
    TAG_TABLE,
    TAG_CLOSURE,
    TAG_NATIVE_FN,
    TAG_USERDATA,
} ValueTag;

typedef struct Value
{
    ValueTag tag;
    union
    {
        b8 boolean;
        f64 number;
        LuaString* string;
        LuaTable* table;
        LuaClosure* closure;
        BavNativeFn native_fn;
        void* userdata;
    };
} Value;

#define VALUE_NIL() ((Value){.tag = TAG_NIL})
#define VALUE_BOOL(v) ((Value){.tag = TAG_BOOL, .boolean = (v)})
#define VALUE_NUMBER(v) ((Value){.tag = TAG_NUMBER, .number = (v)})
#define VALUE_STRING(s) ((Value){.tag = TAG_STRING, .string = (s)})
#define VALUE_TABLE(t) ((Value){.tag = TAG_TABLE, .table = (t)})
#define VALUE_CLOSURE(c) ((Value){.tag = TAG_CLOSURE, .closure = (c)})
#define VALUE_NATIVE_FN(f) ((Value){.tag = TAG_NATIVE_FN, .native_fn = (f)})

#define IS_NIL(v) ((v).tag == TAG_NIL)
#define IS_BOOL(v) ((v).tag == TAG_BOOL)
#define IS_NUMBER(v) ((v).tag == TAG_NUMBER)
#define IS_STRING(v) ((v).tag == TAG_STRING)
#define IS_TABLE(v) ((v).tag == TAG_TABLE)
#define IS_CLOSURE(v) ((v).tag == TAG_CLOSURE)
#define IS_NATIVE_FN(v) ((v).tag == TAG_NATIVE_FN)

/* Truthiness: false and nil are falsy, everything else is truthy */
static inline b8 is_truthy(Value v)
{
    if (v.tag == TAG_NIL)
        return false;
    if (v.tag == TAG_BOOL)
        return v.boolean;
    return true;
}

/* =============================================================================
 * Strings (interned, immutable)
 * ============================================================================= */

struct LuaString
{
    u32 hash;
    u32 length;
    u32 refcount;
    char data[1]; /* Variable length - we allocate extra space */
};

static u32 hash_string(const char* str, usize len)
{
    /* FNV-1a hash */
    u32 hash = 2166136261u;
    for (usize i = 0; i < len; i++)
    {
        hash ^= (u8)str[i];
        hash *= 16777619u;
    }
    return hash;
}

static LuaString* string_create(const char* data, usize len)
{
    LuaString* str = malloc(sizeof(LuaString) + len + 1);
    if (!str)
        return NULL;

    str->hash = hash_string(data, len);
    str->length = (u32)len;
    str->refcount = 1;
    memcpy(str->data, data, len);
    str->data[len] = '\0';
    return str;
}

static void string_release(LuaString* str)
{
    if (str && --str->refcount == 0)
    {
        free(str);
    }
}

static LuaString* string_retain(LuaString* str)
{
    if (str)
        str->refcount++;
    return str;
}

static b8 strings_equal(LuaString* a, LuaString* b)
{
    if (a == b)
        return true;
    if (!a || !b)
        return false;
    if (a->hash != b->hash)
        return false;
    if (a->length != b->length)
        return false;
    return memcmp(a->data, b->data, a->length) == 0;
}

/* =============================================================================
 * Tables (hash tables with array optimization)
 * ============================================================================= */

typedef struct TableEntry
{
    Value key;
    Value value;
    struct TableEntry* next;
} TableEntry;

struct LuaTable
{
    Value* array; /* Array part for integer keys 1..n */
    u32 array_size;
    u32 array_capacity;

    TableEntry** buckets; /* Hash part */
    u32 bucket_count;
    u32 hash_size;

    u32 refcount;
};

static LuaTable* table_create(u32 array_hint, u32 hash_hint)
{
    LuaTable* t = calloc(1, sizeof(LuaTable));
    if (!t)
        return NULL;

    t->refcount = 1;

    if (array_hint > 0)
    {
        t->array_capacity = array_hint;
        t->array = calloc(array_hint, sizeof(Value));
    }

    if (hash_hint > 0)
    {
        /* Round up to power of 2 */
        u32 size = 1;
        while (size < hash_hint)
            size *= 2;
        t->bucket_count = size;
        t->buckets = calloc(size, sizeof(TableEntry*));
    }

    return t;
}

static void table_destroy(LuaTable* t);

static void table_release(LuaTable* t)
{
    if (t && --t->refcount == 0)
    {
        table_destroy(t);
    }
}

static LuaTable* table_retain(LuaTable* t)
{
    if (t)
        t->refcount++;
    return t;
}

static void table_destroy(LuaTable* t)
{
    if (!t)
        return;

    free(t->array);

    for (u32 i = 0; i < t->bucket_count; i++)
    {
        TableEntry* entry = t->buckets[i];
        while (entry)
        {
            TableEntry* next = entry->next;
            /* Release string keys if any */
            if (IS_STRING(entry->key))
            {
                string_release(entry->key.string);
            }
            if (IS_STRING(entry->value))
            {
                string_release(entry->value.string);
            }
            free(entry);
            entry = next;
        }
    }
    free(t->buckets);
    free(t);
}

static u32 value_hash(Value v)
{
    switch (v.tag)
    {
        case TAG_NIL:
            return 0;
        case TAG_BOOL:
            return v.boolean ? 1 : 0;
        case TAG_NUMBER:
        {
            /* Hash the bits of the double */
            union
            {
                f64 d;
                u64 u;
            } conv;
            conv.d = v.number;
            return (u32)(conv.u ^ (conv.u >> 32));
        }
        case TAG_STRING:
            return v.string->hash;
        case TAG_TABLE:
            return (u32)(uintptr_t)v.table;
        default:
            return (u32)(uintptr_t)v.userdata;
    }
}

static b8 values_equal(Value a, Value b)
{
    if (a.tag != b.tag)
        return false;
    switch (a.tag)
    {
        case TAG_NIL:
            return true;
        case TAG_BOOL:
            return a.boolean == b.boolean;
        case TAG_NUMBER:
            return a.number == b.number;
        case TAG_STRING:
            return strings_equal(a.string, b.string);
        case TAG_TABLE:
            return a.table == b.table;
        case TAG_CLOSURE:
            return a.closure == b.closure;
        default:
            return a.userdata == b.userdata;
    }
}

static Value table_get(LuaTable* t, Value key)
{
    if (!t)
        return VALUE_NIL();

    /* Check array part for integer keys */
    if (IS_NUMBER(key))
    {
        f64 n = key.number;
        if (n == (f64)(i32)n && n >= 1 && n <= t->array_size)
        {
            return t->array[(i32)n - 1];
        }
    }

    /* Check hash part */
    if (t->bucket_count == 0)
        return VALUE_NIL();

    u32 hash = value_hash(key);
    u32 bucket = hash & (t->bucket_count - 1);

    TableEntry* entry = t->buckets[bucket];
    while (entry)
    {
        if (values_equal(entry->key, key))
        {
            return entry->value;
        }
        entry = entry->next;
    }

    return VALUE_NIL();
}

static void table_set(LuaTable* t, Value key, Value value)
{
    if (!t || IS_NIL(key))
        return;

    /* Check array part for integer keys */
    if (IS_NUMBER(key))
    {
        f64 n = key.number;
        i32 idx = (i32)n;
        if (n == (f64)idx && idx >= 1)
        {
            /* Expand array if needed */
            if ((u32)idx > t->array_capacity)
            {
                u32 new_cap = t->array_capacity == 0 ? 8 : t->array_capacity;
                while (new_cap < (u32)idx)
                    new_cap *= 2;

                Value* new_arr = realloc(t->array, new_cap * sizeof(Value));
                if (!new_arr)
                    return;

                /* Initialize new slots to nil */
                for (u32 i = t->array_capacity; i < new_cap; i++)
                {
                    new_arr[i] = VALUE_NIL();
                }
                t->array = new_arr;
                t->array_capacity = new_cap;
            }

            t->array[idx - 1] = value;
            if ((u32)idx > t->array_size)
                t->array_size = idx;
            return;
        }
    }

    /* Hash part */
    if (t->bucket_count == 0)
    {
        t->bucket_count = 8;
        t->buckets = calloc(8, sizeof(TableEntry*));
        if (!t->buckets)
            return;
    }

    u32 hash = value_hash(key);
    u32 bucket = hash & (t->bucket_count - 1);

    /* Look for existing entry */
    TableEntry* entry = t->buckets[bucket];
    while (entry)
    {
        if (values_equal(entry->key, key))
        {
            entry->value = value;
            return;
        }
        entry = entry->next;
    }

    /* Create new entry */
    entry = malloc(sizeof(TableEntry));
    if (!entry)
        return;

    entry->key = key;
    if (IS_STRING(key))
        string_retain(key.string);
    entry->value = value;
    entry->next = t->buckets[bucket];
    t->buckets[bucket] = entry;
    t->hash_size++;
}

static u32 table_len(LuaTable* t)
{
    /* Return array size (standard Lua behavior for #) */
    return t ? t->array_size : 0;
}

/* =============================================================================
 * Function Prototypes (from codegen)
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

typedef struct UpvalueInfo
{
    u8 index;
    b8 is_local;
    const char* name;
    usize name_len;
} UpvalueInfo;

typedef struct FunctionProto
{
    u32* code;
    u32 code_count;
    u32 code_capacity;

    Constant* constants;
    u32 constant_count;
    u32 constant_capacity;

    void* locals;
    u32 local_count;
    u32 local_capacity;

    UpvalueInfo* upvalues;
    u32 upvalue_count;
    u32 upvalue_capacity;

    struct FunctionProto** protos;
    u32 proto_count;
    u32 proto_capacity;

    u32* lineinfo;
    u32 lineinfo_capacity;

    u8 num_params;
    u8 is_vararg;
    u8 max_stack;

    const char* source;
    u32 line_defined;
    u32 last_line_defined;
} FunctionProto;

/* =============================================================================
 * Closures and Upvalues
 * ============================================================================= */

struct LuaUpvalue
{
    Value* location;         /* Points to stack slot or closed value */
    Value closed;            /* Storage when closed */
    struct LuaUpvalue* next; /* For open upvalue list */
    u32 refcount;
};

struct LuaClosure
{
    FunctionProto* proto;
    LuaUpvalue** upvalues;
    u32 upvalue_count;
    u32 refcount;
};

static LuaClosure* closure_create(FunctionProto* proto, u32 upvalue_count)
{
    LuaClosure* c = malloc(sizeof(LuaClosure));
    if (!c)
        return NULL;

    c->proto = proto;
    c->upvalue_count = upvalue_count;
    c->refcount = 1;

    if (upvalue_count > 0)
    {
        c->upvalues = calloc(upvalue_count, sizeof(LuaUpvalue*));
        if (!c->upvalues)
        {
            free(c);
            return NULL;
        }
    }
    else
    {
        c->upvalues = NULL;
    }

    return c;
}

static void closure_release(LuaClosure* c)
{
    if (c && --c->refcount == 0)
    {
        free(c->upvalues);
        free(c);
    }
}

static LuaUpvalue* upvalue_create(Value* location)
{
    LuaUpvalue* uv = malloc(sizeof(LuaUpvalue));
    if (!uv)
        return NULL;

    uv->location = location;
    uv->closed = VALUE_NIL();
    uv->next = NULL;
    uv->refcount = 1;
    return uv;
}

/* =============================================================================
 * Call Frame
 * ============================================================================= */

typedef struct CallFrame
{
    LuaClosure* closure;
    u32* ip;         /* Instruction pointer */
    Value* base;     /* Base of this frame's registers */
    u32 num_results; /* Expected return count */
} CallFrame;

#define MAX_FRAMES 256
#define STACK_SIZE 16384

/* =============================================================================
 * VM State
 * ============================================================================= */

typedef struct VM
{
    Value stack[STACK_SIZE];
    Value* stack_top;

    CallFrame frames[MAX_FRAMES];
    u32 frame_count;

    LuaTable* globals;         /* Global variables */
    LuaUpvalue* open_upvalues; /* Linked list of open upvalues */

    char error_msg[1024];
    b8 has_error;

    /* Resource limits */
    u64 instruction_count;
    u64 max_instructions;
} VM;

static VM* vm_create(void)
{
    VM* vm = calloc(1, sizeof(VM));
    if (!vm)
        return NULL;

    vm->stack_top = vm->stack;
    vm->globals = table_create(0, 64);

    return vm;
}

static void vm_destroy(VM* vm)
{
    if (!vm)
        return;
    table_release(vm->globals);
    free(vm);
}

static void vm_error(VM* vm, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(vm->error_msg, sizeof(vm->error_msg), fmt, args);
    va_end(args);
    vm->has_error = true;
}

/* =============================================================================
 * Constant Loading
 * ============================================================================= */

static Value load_constant(FunctionProto* proto, u32 idx)
{
    if (idx >= proto->constant_count)
        return VALUE_NIL();

    Constant* k = &proto->constants[idx];
    switch (k->type)
    {
        case BAV_VALUE_NIL:
            return VALUE_NIL();
        case BAV_VALUE_BOOL:
            return VALUE_BOOL(k->boolean);
        case BAV_VALUE_NUMBER:
            return VALUE_NUMBER(k->number);
        case BAV_VALUE_STRING:
        {
            LuaString* s = string_create(k->string.data, k->string.length);
            return VALUE_STRING(s);
        }
        default:
            return VALUE_NIL();
    }
}

/* Get RK value - either register or constant */
static Value get_rk(VM* vm, CallFrame* frame, u32 rk)
{
    BAV_UNUSED(vm);
    if (rk >= 256)
    {
        return load_constant(frame->closure->proto, rk - 256);
    }
    return frame->base[rk];
}

/* =============================================================================
 * VM Dispatch Loop
 * ============================================================================= */

static b8 vm_execute(VM* vm)
{
    CallFrame* frame = &vm->frames[vm->frame_count - 1];
    FunctionProto* proto = frame->closure->proto;
    Value* base = frame->base;

    for (;;)
    {
        /* Check instruction limit */
        if (vm->max_instructions > 0 && ++vm->instruction_count > vm->max_instructions)
        {
            vm_error(vm, "instruction limit exceeded");
            return false;
        }

        u32 instr = *frame->ip++;
        BavOpcode op = GET_OPCODE(instr);
        u8 A = GET_A(instr);
        u8 B = GET_B(instr);
        u8 C = GET_C(instr);

        switch (op)
        {
            case OP_LOADNIL:
            {
                /* R[A], R[A+1], ..., R[A+B] := nil */
                for (u32 i = A; i <= (u32)A + (u32)B; i++)
                {
                    base[i] = VALUE_NIL();
                }
                break;
            }

            case OP_LOADTRUE:
            {
                base[A] = VALUE_BOOL(true);
                break;
            }

            case OP_LOADFALSE:
            {
                base[A] = VALUE_BOOL(false);
                break;
            }

            case OP_LOADK:
            {
                u16 Bx = GET_Bx(instr);
                base[A] = load_constant(proto, Bx);
                break;
            }

            case OP_MOVE:
            {
                base[A] = base[B];
                break;
            }

            case OP_GETUPVAL:
            {
                LuaUpvalue* uv = frame->closure->upvalues[B];
                base[A] = *uv->location;
                break;
            }

            case OP_SETUPVAL:
            {
                LuaUpvalue* uv = frame->closure->upvalues[B];
                *uv->location = base[A];
                break;
            }

            case OP_NEWTABLE:
            {
                LuaTable* t = table_create(B, C);
                base[A] = VALUE_TABLE(t);
                break;
            }

            case OP_GETTABLE:
            {
                Value obj = base[B];
                Value key = get_rk(vm, frame, C);
                if (!IS_TABLE(obj))
                {
                    vm_error(vm, "attempt to index a %s value",
                             obj.tag == TAG_NIL ? "nil" : "non-table");
                    return false;
                }
                base[A] = table_get(obj.table, key);
                break;
            }

            case OP_SETTABLE:
            {
                Value obj = base[A];
                Value key = get_rk(vm, frame, B);
                Value val = get_rk(vm, frame, C);
                if (!IS_TABLE(obj))
                {
                    vm_error(vm, "attempt to index a %s value",
                             obj.tag == TAG_NIL ? "nil" : "non-table");
                    return false;
                }
                table_set(obj.table, key, val);
                break;
            }

            case OP_GETFIELD:
            {
                Value obj = base[B];
                if (!IS_TABLE(obj))
                {
                    vm_error(vm, "attempt to index a non-table value");
                    return false;
                }
                Value key = load_constant(proto, C);
                base[A] = table_get(obj.table, key);
                break;
            }

            case OP_SETFIELD:
            {
                Value obj = base[A];
                if (!IS_TABLE(obj))
                {
                    vm_error(vm, "attempt to index a non-table value");
                    return false;
                }
                Value key = load_constant(proto, B);
                Value val = get_rk(vm, frame, C);
                table_set(obj.table, key, val);
                break;
            }

            case OP_GETGLOBAL:
            {
                u16 Bx = GET_Bx(instr);
                Value key = load_constant(proto, Bx);
                base[A] = table_get(vm->globals, key);
                break;
            }

            case OP_SETGLOBAL:
            {
                u16 Bx = GET_Bx(instr);
                Value key = load_constant(proto, Bx);
                table_set(vm->globals, key, base[A]);
                break;
            }

            case OP_ADD:
            {
                Value b = get_rk(vm, frame, B);
                Value c = get_rk(vm, frame, C);
                if (!IS_NUMBER(b) || !IS_NUMBER(c))
                {
                    vm_error(vm, "attempt to perform arithmetic on non-number");
                    return false;
                }
                base[A] = VALUE_NUMBER(b.number + c.number);
                break;
            }

            case OP_SUB:
            {
                Value b = get_rk(vm, frame, B);
                Value c = get_rk(vm, frame, C);
                if (!IS_NUMBER(b) || !IS_NUMBER(c))
                {
                    vm_error(vm, "attempt to perform arithmetic on non-number");
                    return false;
                }
                base[A] = VALUE_NUMBER(b.number - c.number);
                break;
            }

            case OP_MUL:
            {
                Value b = get_rk(vm, frame, B);
                Value c = get_rk(vm, frame, C);
                if (!IS_NUMBER(b) || !IS_NUMBER(c))
                {
                    vm_error(vm, "attempt to perform arithmetic on non-number");
                    return false;
                }
                base[A] = VALUE_NUMBER(b.number * c.number);
                break;
            }

            case OP_DIV:
            {
                Value b = get_rk(vm, frame, B);
                Value c = get_rk(vm, frame, C);
                if (!IS_NUMBER(b) || !IS_NUMBER(c))
                {
                    vm_error(vm, "attempt to perform arithmetic on non-number");
                    return false;
                }
                base[A] = VALUE_NUMBER(b.number / c.number);
                break;
            }

            case OP_IDIV:
            {
                Value b = get_rk(vm, frame, B);
                Value c = get_rk(vm, frame, C);
                if (!IS_NUMBER(b) || !IS_NUMBER(c))
                {
                    vm_error(vm, "attempt to perform arithmetic on non-number");
                    return false;
                }
                base[A] = VALUE_NUMBER(floor(b.number / c.number));
                break;
            }

            case OP_MOD:
            {
                Value b = get_rk(vm, frame, B);
                Value c = get_rk(vm, frame, C);
                if (!IS_NUMBER(b) || !IS_NUMBER(c))
                {
                    vm_error(vm, "attempt to perform arithmetic on non-number");
                    return false;
                }
                base[A] = VALUE_NUMBER(fmod(b.number, c.number));
                break;
            }

            case OP_POW:
            {
                Value b = get_rk(vm, frame, B);
                Value c = get_rk(vm, frame, C);
                if (!IS_NUMBER(b) || !IS_NUMBER(c))
                {
                    vm_error(vm, "attempt to perform arithmetic on non-number");
                    return false;
                }
                base[A] = VALUE_NUMBER(pow(b.number, c.number));
                break;
            }

            case OP_UNM:
            {
                Value b = base[B];
                if (!IS_NUMBER(b))
                {
                    vm_error(vm, "attempt to perform arithmetic on non-number");
                    return false;
                }
                base[A] = VALUE_NUMBER(-b.number);
                break;
            }

            case OP_BAND:
            {
                Value b = get_rk(vm, frame, B);
                Value c = get_rk(vm, frame, C);
                if (!IS_NUMBER(b) || !IS_NUMBER(c))
                {
                    vm_error(vm, "attempt to perform bitwise on non-number");
                    return false;
                }
                base[A] = VALUE_NUMBER((f64)((i64)b.number & (i64)c.number));
                break;
            }

            case OP_BOR:
            {
                Value b = get_rk(vm, frame, B);
                Value c = get_rk(vm, frame, C);
                if (!IS_NUMBER(b) || !IS_NUMBER(c))
                {
                    vm_error(vm, "attempt to perform bitwise on non-number");
                    return false;
                }
                base[A] = VALUE_NUMBER((f64)((i64)b.number | (i64)c.number));
                break;
            }

            case OP_BXOR:
            {
                Value b = get_rk(vm, frame, B);
                Value c = get_rk(vm, frame, C);
                if (!IS_NUMBER(b) || !IS_NUMBER(c))
                {
                    vm_error(vm, "attempt to perform bitwise on non-number");
                    return false;
                }
                base[A] = VALUE_NUMBER((f64)((i64)b.number ^ (i64)c.number));
                break;
            }

            case OP_BNOT:
            {
                Value b = base[B];
                if (!IS_NUMBER(b))
                {
                    vm_error(vm, "attempt to perform bitwise on non-number");
                    return false;
                }
                base[A] = VALUE_NUMBER((f64)(~(i64)b.number));
                break;
            }

            case OP_SHL:
            {
                Value b = get_rk(vm, frame, B);
                Value c = get_rk(vm, frame, C);
                if (!IS_NUMBER(b) || !IS_NUMBER(c))
                {
                    vm_error(vm, "attempt to perform bitwise on non-number");
                    return false;
                }
                base[A] = VALUE_NUMBER((f64)((i64)b.number << (i64)c.number));
                break;
            }

            case OP_SHR:
            {
                Value b = get_rk(vm, frame, B);
                Value c = get_rk(vm, frame, C);
                if (!IS_NUMBER(b) || !IS_NUMBER(c))
                {
                    vm_error(vm, "attempt to perform bitwise on non-number");
                    return false;
                }
                base[A] = VALUE_NUMBER((f64)((u64)b.number >> (i64)c.number));
                break;
            }

            case OP_EQ:
            {
                Value b = get_rk(vm, frame, B);
                Value c = get_rk(vm, frame, C);
                b8 result = values_equal(b, c);
                if (result != (A != 0))
                {
                    frame->ip++; /* Skip next instruction */
                }
                break;
            }

            case OP_LT:
            {
                Value b = get_rk(vm, frame, B);
                Value c = get_rk(vm, frame, C);
                b8 result = false;
                if (IS_NUMBER(b) && IS_NUMBER(c))
                {
                    result = b.number < c.number;
                }
                else if (IS_STRING(b) && IS_STRING(c))
                {
                    result = strcmp(b.string->data, c.string->data) < 0;
                }
                else
                {
                    vm_error(vm, "attempt to compare incompatible types");
                    return false;
                }
                if (result != (A != 0))
                {
                    frame->ip++;
                }
                break;
            }

            case OP_LE:
            {
                Value b = get_rk(vm, frame, B);
                Value c = get_rk(vm, frame, C);
                b8 result = false;
                if (IS_NUMBER(b) && IS_NUMBER(c))
                {
                    result = b.number <= c.number;
                }
                else if (IS_STRING(b) && IS_STRING(c))
                {
                    result = strcmp(b.string->data, c.string->data) <= 0;
                }
                else
                {
                    vm_error(vm, "attempt to compare incompatible types");
                    return false;
                }
                if (result != (A != 0))
                {
                    frame->ip++;
                }
                break;
            }

            case OP_NOT:
            {
                base[A] = VALUE_BOOL(!is_truthy(base[B]));
                break;
            }

            case OP_LEN:
            {
                Value b = base[B];
                if (IS_STRING(b))
                {
                    base[A] = VALUE_NUMBER((f64)b.string->length);
                }
                else if (IS_TABLE(b))
                {
                    base[A] = VALUE_NUMBER((f64)table_len(b.table));
                }
                else
                {
                    vm_error(vm, "attempt to get length of non-string/table");
                    return false;
                }
                break;
            }

            case OP_CONCAT:
            {
                /* Concatenate strings from R[B] to R[C] into R[A] */
                usize total_len = 0;
                for (u32 i = B; i <= C; i++)
                {
                    if (!IS_STRING(base[i]))
                    {
                        vm_error(vm, "attempt to concatenate non-string value");
                        return false;
                    }
                    total_len += base[i].string->length;
                }

                char* buf = malloc(total_len + 1);
                if (!buf)
                {
                    vm_error(vm, "out of memory");
                    return false;
                }

                char* p = buf;
                for (u32 i = B; i <= C; i++)
                {
                    LuaString* s = base[i].string;
                    memcpy(p, s->data, s->length);
                    p += s->length;
                }
                *p = '\0';

                LuaString* result = string_create(buf, total_len);
                free(buf);
                base[A] = VALUE_STRING(result);
                break;
            }

            case OP_JMP:
            {
                i32 sBx = GET_sBx(instr);
                frame->ip += sBx;
                break;
            }

            case OP_TEST:
            {
                /* if (R[A] <=> C) then pc++ */
                b8 truthy = is_truthy(base[A]);
                if (truthy != (C != 0))
                {
                    frame->ip++; /* Skip next jump */
                }
                break;
            }

            case OP_TESTSET:
            {
                /* if (R[B] <=> C) then R[A] := R[B] else pc++ */
                b8 truthy = is_truthy(base[B]);
                if (truthy == (C != 0))
                {
                    base[A] = base[B];
                }
                else
                {
                    frame->ip++;
                }
                break;
            }

            case OP_CALL:
            {
                /* R[A], ..., R[A+C-2] := R[A](R[A+1], ..., R[A+B-1]) */
                Value func = base[A];
                u32 arg_count = (B == 0) ? (u32)(vm->stack_top - &base[A] - 1) : B - 1;
                u32 want_results = (C == 0) ? 0xFFFF : C - 1;

                if (IS_CLOSURE(func))
                {
                    LuaClosure* closure = func.closure;
                    FunctionProto* new_proto = closure->proto;

                    if (vm->frame_count >= MAX_FRAMES)
                    {
                        vm_error(vm, "call stack overflow");
                        return false;
                    }

                    /* Set up new frame */
                    CallFrame* new_frame = &vm->frames[vm->frame_count++];
                    new_frame->closure = closure;
                    new_frame->ip = new_proto->code;
                    new_frame->base = &base[A + 1];
                    new_frame->num_results = want_results;

                    /* Adjust arg count if needed */
                    while (arg_count < new_proto->num_params)
                    {
                        new_frame->base[arg_count++] = VALUE_NIL();
                    }

                    /* Update stack top */
                    vm->stack_top = new_frame->base + new_proto->max_stack;

                    /* Switch to new frame */
                    frame = new_frame;
                    proto = new_proto;
                    base = frame->base;
                }
                else if (IS_NATIVE_FN(func))
                {
                    /* Convert args to BavValue array */
                    BavValue* bav_args = NULL;
                    if (arg_count > 0)
                    {
                        bav_args = malloc(arg_count * sizeof(BavValue));
                        for (u32 i = 0; i < arg_count; i++)
                        {
                            Value v = base[A + 1 + i];
                            bav_args[i].type = (BavValueType)v.tag;
                            switch (v.tag)
                            {
                                case TAG_NIL:
                                    bav_args[i].type = BAV_VALUE_NIL;
                                    break;
                                case TAG_BOOL:
                                    bav_args[i].as_bool = v.boolean;
                                    break;
                                case TAG_NUMBER:
                                    bav_args[i].as_number = v.number;
                                    break;
                                case TAG_STRING:
                                    bav_args[i].as_string.data = v.string->data;
                                    bav_args[i].as_string.length = v.string->length;
                                    break;
                                default:
                                    break;
                            }
                        }
                    }

                    BavCallResult result = func.native_fn(NULL, bav_args, arg_count, NULL);
                    free(bav_args);

                    if (!result.success)
                    {
                        vm_error(vm, "%s", result.error ? result.error : "native function error");
                        return false;
                    }

                    /* Copy results back */
                    for (u32 i = 0; i < result.value_count && i < want_results; i++)
                    {
                        BavValue* rv = &result.values[i];
                        switch (rv->type)
                        {
                            case BAV_VALUE_NIL:
                                base[A + i] = VALUE_NIL();
                                break;
                            case BAV_VALUE_BOOL:
                                base[A + i] = VALUE_BOOL(rv->as_bool);
                                break;
                            case BAV_VALUE_NUMBER:
                                base[A + i] = VALUE_NUMBER(rv->as_number);
                                break;
                            case BAV_VALUE_STRING:
                            {
                                LuaString* s =
                                    string_create(rv->as_string.data, rv->as_string.length);
                                base[A + i] = VALUE_STRING(s);
                                break;
                            }
                            default:
                                base[A + i] = VALUE_NIL();
                                break;
                        }
                    }

                    bav_call_result_free(&result);
                }
                else
                {
                    vm_error(vm, "attempt to call a %s value",
                             func.tag == TAG_NIL ? "nil" : "non-function");
                    return false;
                }
                break;
            }

            case OP_TAILCALL:
            {
                /* Tail call - reuse current frame */
                Value func = base[A];
                u32 arg_count = (B == 0) ? (u32)(vm->stack_top - &base[A] - 1) : B - 1;

                if (!IS_CLOSURE(func))
                {
                    vm_error(vm, "attempt to call a non-function");
                    return false;
                }

                LuaClosure* closure = func.closure;
                FunctionProto* new_proto = closure->proto;

                /* Move args to base of current frame */
                Value* current_base = frame->base - 1;
                for (u32 i = 0; i <= arg_count; i++)
                {
                    current_base[i] = base[A + i];
                }

                /* Reuse frame */
                frame->closure = closure;
                frame->ip = new_proto->code;
                frame->base = current_base + 1;

                proto = new_proto;
                base = frame->base;
                vm->stack_top = base + new_proto->max_stack;
                break;
            }

            case OP_RETURN:
            {
                /* return R[A], ..., R[A+B-2] */
                u32 num_results = (B == 0) ? (u32)(vm->stack_top - &base[A]) : B - 1;

                if (vm->frame_count == 1)
                {
                    /* Returning from main chunk */
                    return true;
                }

                /* Copy results to caller's expected location */
                CallFrame* caller = &vm->frames[vm->frame_count - 2];
                Value* dest = frame->base - 1;

                u32 want = caller->num_results;
                if (want == 0xFFFF)
                    want = num_results;

                for (u32 i = 0; i < want; i++)
                {
                    dest[i] = (i < num_results) ? base[A + i] : VALUE_NIL();
                }

                /* Pop frame */
                vm->frame_count--;
                frame = &vm->frames[vm->frame_count - 1];
                proto = frame->closure->proto;
                base = frame->base;
                vm->stack_top = base + proto->max_stack;
                break;
            }

            case OP_SELF:
            {
                /* R[A+1] := R[B]; R[A] := R[B][RK(C)] */
                Value obj = base[B];
                base[A + 1] = obj;
                if (!IS_TABLE(obj))
                {
                    vm_error(vm, "attempt to index non-table for method call");
                    return false;
                }
                Value key = get_rk(vm, frame, C);
                base[A] = table_get(obj.table, key);
                break;
            }

            case OP_FORPREP:
            {
                /* R[A] -= R[A+2]; pc += sBx */
                if (!IS_NUMBER(base[A]) || !IS_NUMBER(base[A + 2]))
                {
                    vm_error(vm, "for loop: invalid numeric values");
                    return false;
                }
                base[A].number -= base[A + 2].number;
                i32 sBx = GET_sBx(instr);
                frame->ip += sBx;
                break;
            }

            case OP_FORLOOP:
            {
                /* R[A] += R[A+2]; if R[A] <= R[A+1] then { pc += sBx; R[A+3] := R[A] } */
                f64 step = base[A + 2].number;
                base[A].number += step;

                f64 idx = base[A].number;
                f64 limit = base[A + 1].number;

                b8 continue_loop;
                if (step > 0)
                {
                    continue_loop = (idx <= limit);
                }
                else
                {
                    continue_loop = (idx >= limit);
                }

                if (continue_loop)
                {
                    i32 sBx = GET_sBx(instr);
                    frame->ip += sBx;
                    base[A + 3] = VALUE_NUMBER(idx);
                }
                break;
            }

            case OP_TFORCALL:
            {
                /* R[A+3], ..., R[A+2+C] := R[A](R[A+1], R[A+2]) */
                /* Call iterator function */
                Value func = base[A];
                u32 want_results = C;

                if (!IS_CLOSURE(func) && !IS_NATIVE_FN(func))
                {
                    vm_error(vm, "for iterator is not a function");
                    return false;
                }

                /* Set up args: R[A+1] (state), R[A+2] (var) */
                /* Results go to R[A+3]... */

                /* Simplified: just call the function */
                if (IS_CLOSURE(func))
                {
                    LuaClosure* closure = func.closure;
                    FunctionProto* iter_proto = closure->proto;

                    if (vm->frame_count >= MAX_FRAMES)
                    {
                        vm_error(vm, "call stack overflow");
                        return false;
                    }

                    CallFrame* iter_frame = &vm->frames[vm->frame_count++];
                    iter_frame->closure = closure;
                    iter_frame->ip = iter_proto->code;
                    iter_frame->base = &base[A + 4];
                    iter_frame->num_results = want_results;

                    /* Copy args */
                    iter_frame->base[-1] = base[A + 1]; /* state */
                    iter_frame->base[0] = base[A + 2];  /* var */

                    vm->stack_top = iter_frame->base + iter_proto->max_stack;

                    /* Execute nested - simplified for now */
                    /* In a real impl, we'd save state and continue the loop */
                }
                break;
            }

            case OP_TFORLOOP:
            {
                /* if R[A+1] ~= nil then { R[A] := R[A+1]; pc += sBx } */
                if (!IS_NIL(base[A + 1]))
                {
                    base[A] = base[A + 1];
                    i32 sBx = GET_sBx(instr);
                    frame->ip += sBx;
                }
                break;
            }

            case OP_CLOSURE:
            {
                u16 Bx = GET_Bx(instr);
                FunctionProto* new_proto = proto->protos[Bx];
                LuaClosure* closure = closure_create(new_proto, new_proto->upvalue_count);

                /* Set up upvalues */
                for (u32 i = 0; i < new_proto->upvalue_count; i++)
                {
                    UpvalueInfo* uv_info = &new_proto->upvalues[i];
                    if (uv_info->is_local)
                    {
                        /* Capture from current frame's locals */
                        closure->upvalues[i] = upvalue_create(&base[uv_info->index]);
                    }
                    else
                    {
                        /* Copy from enclosing function's upvalues */
                        closure->upvalues[i] = frame->closure->upvalues[uv_info->index];
                        closure->upvalues[i]->refcount++;
                    }
                }

                base[A] = VALUE_CLOSURE(closure);
                break;
            }

            case OP_CLOSE:
            {
                /* Close all variables from R[A] to top */
                /* For now, just a no-op - proper upvalue closing needs more work */
                break;
            }

            case OP_VARARG:
            {
                /* R[A], ..., R[A+B-2] := vararg */
                /* For simplicity, we're not fully implementing vararg yet */
                u32 want = (B == 0) ? 1 : B - 1;
                for (u32 i = 0; i < want; i++)
                {
                    base[A + i] = VALUE_NIL();
                }
                break;
            }

            case OP_SETLIST:
            {
                /* R[A][(C-1)*50+i] := R[A+i], 1 <= i <= B */
                Value tbl = base[A];
                if (!IS_TABLE(tbl))
                {
                    vm_error(vm, "SETLIST on non-table");
                    return false;
                }
                u32 n = B;
                u32 offset = (C - 1) * 50;
                for (u32 i = 1; i <= n; i++)
                {
                    table_set(tbl.table, VALUE_NUMBER((f64)(offset + i)), base[A + i]);
                }
                break;
            }

            default:
            {
                vm_error(vm, "unknown opcode %d", op);
                return false;
            }
        }
    }

    /* Unreachable - all exit paths are return statements inside the loop.
     * Suppress C4702 warning on MSVC. */
#ifdef _MSC_VER
    __assume(0);
#endif
}

/* =============================================================================
 * Public API
 * ============================================================================= */

struct BavScriptContext
{
    VM* vm;
    FunctionProto* main_proto;
};

BavScriptContext* bav_script_context_create(const BavScriptContextConfig* config)
{
    BavScriptContext* ctx = calloc(1, sizeof(BavScriptContext));
    if (!ctx)
        return NULL;

    ctx->vm = vm_create();
    if (!ctx->vm)
    {
        free(ctx);
        return NULL;
    }

    if (config)
    {
        ctx->vm->max_instructions = config->max_instructions;
    }

    return ctx;
}

void bav_script_context_destroy(BavScriptContext* ctx)
{
    if (!ctx)
        return;
    vm_destroy(ctx->vm);
    free(ctx);
}

BavResult bav_script_context_load(BavScriptContext* ctx, BavCompiledScript* script)
{
    if (!ctx || !script)
        return BAV_ERROR_INVALID_ARG;

    ctx->main_proto = (FunctionProto*)script;
    return BAV_OK;
}

usize bav_script_context_memory_used(BavScriptContext* ctx)
{
    /* Basic approximation for now */
    return ctx ? sizeof(VM) : 0;
}

BavCallResult bav_script_call(BavScriptContext* ctx, const char* name, const BavValue* args,
                              u32 arg_count)
{
    BavCallResult result = {0};

    /* TODO: Use name to call specific function, use args to pass arguments */
    BAV_UNUSED(name);
    BAV_UNUSED(args);
    BAV_UNUSED(arg_count);

    if (!ctx || !ctx->vm || !ctx->main_proto)
    {
        result.error = "invalid context";
        return result;
    }

    VM* vm = ctx->vm;
    vm->has_error = false;
    vm->instruction_count = 0;

    /* Create main closure */
    LuaClosure* main_closure = closure_create(ctx->main_proto, 0);
    if (!main_closure)
    {
        result.error = "out of memory";
        return result;
    }

    /* Set up initial call frame */
    vm->frame_count = 1;
    CallFrame* frame = &vm->frames[0];
    frame->closure = main_closure;
    frame->ip = ctx->main_proto->code;
    frame->base = vm->stack;
    frame->num_results = 0;

    vm->stack_top = vm->stack + ctx->main_proto->max_stack;

    /* Execute */
    b8 ok = vm_execute(vm);

    if (!ok)
    {
        result.error = vm->error_msg;
    }
    else
    {
        result.success = true;
    }

    closure_release(main_closure);
    return result;
}

void bav_call_result_free(BavCallResult* result)
{
    if (result && result->values)
    {
        free(result->values);
        result->values = NULL;
    }
}

void bav_script_set_global(BavScriptContext* ctx, const char* name, BavValue value)
{
    if (!ctx || !ctx->vm || !name)
        return;

    LuaString* key = string_create(name, strlen(name));
    Value v;

    switch (value.type)
    {
        case BAV_VALUE_NIL:
            v = VALUE_NIL();
            break;
        case BAV_VALUE_BOOL:
            v = VALUE_BOOL(value.as_bool);
            break;
        case BAV_VALUE_NUMBER:
            v = VALUE_NUMBER(value.as_number);
            break;
        case BAV_VALUE_STRING:
        {
            LuaString* s = string_create(value.as_string.data, value.as_string.length);
            v = VALUE_STRING(s);
            break;
        }
        default:
            v = VALUE_NIL();
            break;
    }

    table_set(ctx->vm->globals, VALUE_STRING(key), v);
}

BavValue bav_script_get_global(BavScriptContext* ctx, const char* name)
{
    BavValue result = {.type = BAV_VALUE_NIL};
    if (!ctx || !ctx->vm || !name)
        return result;

    LuaString* key = string_create(name, strlen(name));
    Value v = table_get(ctx->vm->globals, VALUE_STRING(key));
    string_release(key);

    switch (v.tag)
    {
        case TAG_NIL:
            result.type = BAV_VALUE_NIL;
            break;
        case TAG_BOOL:
            result.type = BAV_VALUE_BOOL;
            result.as_bool = v.boolean;
            break;
        case TAG_NUMBER:
            result.type = BAV_VALUE_NUMBER;
            result.as_number = v.number;
            break;
        case TAG_STRING:
            result.type = BAV_VALUE_STRING;
            result.as_string.data = v.string->data;
            result.as_string.length = v.string->length;
            break;
        default:
            break;
    }

    return result;
}

void bav_script_register_function(BavScriptContext* ctx, const char* name, BavNativeFn fn,
                                  void* user_data)
{
    /* TODO: Store user_data for native function context */
    BAV_UNUSED(user_data);

    if (!ctx || !ctx->vm || !name || !fn)
        return;

    LuaString* key = string_create(name, strlen(name));
    Value v = VALUE_NATIVE_FN(fn);
    table_set(ctx->vm->globals, VALUE_STRING(key), v);
}

void bav_script_register_module(BavScriptContext* ctx, const char* module_name,
                                const BavNativeFnDef* functions, u32 count, void* user_data)
{
    /* TODO: Store user_data for native function contexts */
    BAV_UNUSED(user_data);

    if (!ctx || !ctx->vm || !module_name || !functions)
        return;

    /* Create module table */
    LuaTable* module = table_create(0, count);

    for (u32 i = 0; i < count; i++)
    {
        LuaString* fn_name = string_create(functions[i].name, strlen(functions[i].name));
        table_set(module, VALUE_STRING(fn_name), VALUE_NATIVE_FN(functions[i].fn));
    }

    /* Register module as global */
    LuaString* mod_key = string_create(module_name, strlen(module_name));
    table_set(ctx->vm->globals, VALUE_STRING(mod_key), VALUE_TABLE(module));
}

/* =============================================================================
 * Script Hot-Reload
 *
 * This is the runtime side of hot-reload. The asset pipeline (hot_reload.c)
 * handles detecting file changes and re-importing. Here we handle recompiling
 * and reloading scripts at runtime while preserving state.
 *
 * The tricky bit is preserving globals - when we reload a script, we want to
 * keep the old global values unless the new script explicitly overrides them.
 * ============================================================================= */

#include <sys/stat.h>

/* Track loaded scripts for hot-reload */
typedef struct LoadedScript
{
    char filename[256];
    u64 load_time;      /* When we last loaded this script */
    b8 valid;
} LoadedScript;

#define MAX_LOADED_SCRIPTS 64
static LoadedScript g_loaded_scripts[MAX_LOADED_SCRIPTS];
static u32 g_loaded_script_count = 0;

/* Get file modification time - platform specific but we'll use stat */
static u64 get_file_mtime(const char* path)
{
    struct stat st;
    if (stat(path, &st) != 0)
        return 0;

#ifdef _WIN32
    return (u64)st.st_mtime;
#else
    return (u64)st.st_mtime;
#endif
}

/* Track a loaded script file */
static void track_loaded_script(const char* filename)
{
    /* Check if already tracking */
    for (u32 i = 0; i < g_loaded_script_count; i++)
    {
        if (g_loaded_scripts[i].valid && strcmp(g_loaded_scripts[i].filename, filename) == 0)
        {
            g_loaded_scripts[i].load_time = get_file_mtime(filename);
            return;
        }
    }

    /* Add new entry */
    if (g_loaded_script_count < MAX_LOADED_SCRIPTS)
    {
        LoadedScript* entry = &g_loaded_scripts[g_loaded_script_count++];
        strncpy(entry->filename, filename, sizeof(entry->filename) - 1);
        entry->filename[sizeof(entry->filename) - 1] = '\0';
        entry->load_time = get_file_mtime(filename);
        entry->valid = true;
    }
}

/* Find loaded script entry */
static LoadedScript* find_loaded_script(const char* filename)
{
    for (u32 i = 0; i < g_loaded_script_count; i++)
    {
        if (g_loaded_scripts[i].valid && strcmp(g_loaded_scripts[i].filename, filename) == 0)
        {
            return &g_loaded_scripts[i];
        }
    }
    return NULL;
}

b8 bav_script_needs_reload(BavScriptContext* ctx, const char* filename)
{
    BAV_UNUSED(ctx); /* Reload check is global, not per-context */

    if (!filename)
        return false;

    LoadedScript* entry = find_loaded_script(filename);
    if (!entry)
        return false; /* Not tracking this file */

    u64 current_mtime = get_file_mtime(filename);
    return current_mtime > entry->load_time;
}

BavResult bav_script_hot_reload(BavScriptContext* ctx, const char* filename)
{
    if (!ctx || !filename)
        return BAV_ERROR_INVALID_ARG;

    /* Read the script file */
    FILE* f = fopen(filename, "rb");
    if (!f)
    {
        fprintf(stderr, "[script] Failed to open %s for hot-reload\n", filename);
        return BAV_ERROR_IO;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0)
    {
        fclose(f);
        return BAV_ERROR_IO;
    }

    char* source = malloc((usize)file_size + 1);
    if (!source)
    {
        fclose(f);
        return BAV_ERROR_OUT_OF_MEMORY;
    }

    usize read_size = fread(source, 1, (usize)file_size, f);
    fclose(f);
    source[read_size] = '\0';

    /* Compile the script */
    BavCompileOptions options = {0};
    options.filename = filename;
    options.debug_info = true;
    options.optimize = true;

    BavCompileResult result = bav_compile_lua(source, read_size, &options);
    free(source);

    if (!result.success)
    {
        fprintf(stderr, "[script] Hot-reload compile errors in %s:\n", filename);
        for (u32 i = 0; i < result.error_count; i++)
        {
            fprintf(stderr, "  %s:%u:%u: %s\n",
                    result.errors[i].filename ? result.errors[i].filename : filename,
                    result.errors[i].line, result.errors[i].column,
                    result.errors[i].message);
        }
        bav_compile_result_free(&result);
        return BAV_ERROR_GENERAL;
    }

    /*
     * Load the new script into the context.
     * This replaces the main proto but preserves the VM state (globals).
     * Scripts that define new functions will add them to globals,
     * but existing global values are kept.
     */
    BavResult load_result = bav_script_context_load(ctx, result.script);
    if (BAV_FAILED(load_result))
    {
        bav_compile_result_free(&result);
        return load_result;
    }

    /* Execute the main chunk to re-register functions and update globals */
    BavCallResult call_result = bav_script_call(ctx, NULL, NULL, 0);
    if (!call_result.success)
    {
        fprintf(stderr, "[script] Hot-reload execution error: %s\n",
                call_result.error ? call_result.error : "unknown");
        return BAV_ERROR_GENERAL;
    }

    /* Update the tracked load time */
    track_loaded_script(filename);

    printf("[script] Hot-reloaded: %s\n", filename);
    return BAV_OK;
}

/* Helper to initially load a script file (and track it for hot-reload) */
BavResult bav_script_load_file(BavScriptContext* ctx, const char* filename)
{
    if (!ctx || !filename)
        return BAV_ERROR_INVALID_ARG;

    /* Read the script file */
    FILE* f = fopen(filename, "rb");
    if (!f)
    {
        return BAV_ERROR_IO;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0)
    {
        fclose(f);
        return BAV_ERROR_IO;
    }

    char* source = malloc((usize)file_size + 1);
    if (!source)
    {
        fclose(f);
        return BAV_ERROR_OUT_OF_MEMORY;
    }

    usize read_size = fread(source, 1, (usize)file_size, f);
    fclose(f);
    source[read_size] = '\0';

    /* Compile the script */
    BavCompileOptions options = {0};
    options.filename = filename;
    options.debug_info = true;
    options.optimize = true;

    BavCompileResult result = bav_compile_lua(source, read_size, &options);
    free(source);

    if (!result.success)
    {
        fprintf(stderr, "[script] Compile errors in %s:\n", filename);
        for (u32 i = 0; i < result.error_count; i++)
        {
            fprintf(stderr, "  %s:%u:%u: %s\n",
                    result.errors[i].filename ? result.errors[i].filename : filename,
                    result.errors[i].line, result.errors[i].column,
                    result.errors[i].message);
        }
        bav_compile_result_free(&result);
        return BAV_ERROR_GENERAL;
    }

    /* Load the script */
    BavResult load_result = bav_script_context_load(ctx, result.script);
    if (BAV_FAILED(load_result))
    {
        bav_compile_result_free(&result);
        return load_result;
    }

    /* Execute the main chunk */
    BavCallResult call_result = bav_script_call(ctx, NULL, NULL, 0);
    if (!call_result.success)
    {
        fprintf(stderr, "[script] Execution error: %s\n",
                call_result.error ? call_result.error : "unknown");
        return BAV_ERROR_GENERAL;
    }

    /* Track for hot-reload */
    track_loaded_script(filename);

    return BAV_OK;
}
