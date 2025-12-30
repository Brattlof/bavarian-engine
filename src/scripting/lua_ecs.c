/**
 * @file lua_ecs.c
 * @brief ECS bindings for Lua
 *
 * This is where scripts get to create entities and mess with components.
 * The API is intentionally limited - we don't want scripts doing things
 * like manually iterating archetypes or touching raw component memory.
 *
 * Entity handles are packed into BavHandle for Lua, component IDs are just
 * numbers. Component data is trickier - for now we support a table-based
 * approach for known component types, with the option to add raw byte
 * access later if someone really needs it.
 */

#include <bavarian/ecs.h>
#include <bavarian/scripting.h>
#include <bavarian/types.h>
#include <stdlib.h>
#include <string.h>

/* =============================================================================
 * Global State
 *
 * Yeah yeah, globals are bad. But the alternative is threading this through
 * every Lua binding function and that's a pain in the ass. The ECS admin
 * is basically a singleton anyway.
 * ============================================================================= */

static BavEntityAdmin* g_ecs_admin = NULL;

/* =============================================================================
 * Component Field Descriptors
 *
 * To marshal between Lua tables and C structs, we need to know the layout
 * of each component type. This is a simple descriptor system - nothing fancy.
 * ============================================================================= */

typedef enum BavFieldType
{
    BAV_FIELD_F32,
    BAV_FIELD_F64,
    BAV_FIELD_I32,
    BAV_FIELD_U32,
    BAV_FIELD_BOOL,
    BAV_FIELD_VEC3, /* 3x f32 */
    BAV_FIELD_VEC4, /* 4x f32 */
    BAV_FIELD_MAT4, /* 16x f32 */
} BavFieldType;

typedef struct BavFieldDesc
{
    const char* name;
    BavFieldType type;
    usize offset;
} BavFieldDesc;

typedef struct BavComponentDesc
{
    BavComponentId id;
    const char* name;
    usize size;
    usize alignment;
    const BavFieldDesc* fields;
    u32 field_count;
} BavComponentDesc;

/* Storage for registered component descriptors */
#define MAX_COMPONENT_DESCS 64
static BavComponentDesc g_component_descs[MAX_COMPONENT_DESCS];
static u32 g_component_desc_count = 0;

/* =============================================================================
 * Helper Functions
 * ============================================================================= */

static BavCallResult make_error(const char* msg)
{
    BavCallResult r = {0};
    r.success = false;
    r.error = msg;
    return r;
}

static BavCallResult make_number(f64 n)
{
    BavCallResult r = {0};
    r.success = true;
    r.value_count = 1;
    r.values = malloc(sizeof(BavValue));
    r.values[0].type = BAV_VALUE_NUMBER;
    r.values[0].as_number = n;
    return r;
}

static BavCallResult make_bool(b8 b)
{
    BavCallResult r = {0};
    r.success = true;
    r.value_count = 1;
    r.values = malloc(sizeof(BavValue));
    r.values[0].type = BAV_VALUE_BOOL;
    r.values[0].as_bool = b;
    return r;
}

static BavCallResult make_nil(void)
{
    BavCallResult r = {0};
    r.success = true;
    r.value_count = 0;
    return r;
}

static BavCallResult make_handle(BavHandle h)
{
    BavCallResult r = {0};
    r.success = true;
    r.value_count = 1;
    r.values = malloc(sizeof(BavValue));
    r.values[0].type = BAV_VALUE_HANDLE;
    r.values[0].as_handle = h;
    return r;
}

/* Pack BavEntity into BavHandle for Lua */
static BavHandle entity_to_handle(BavEntity e)
{
    return bav_handle_make(e.index, e.generation, e.flags);
}

/* Unpack BavHandle to BavEntity */
static BavEntity handle_to_entity(BavHandle h)
{
    BavEntity e;
    e.index = bav_handle_index(h);
    e.generation = bav_handle_generation(h);
    e.flags = bav_handle_type(h);
    return e;
}

/* Get entity from Lua argument (handles both HANDLE and NUMBER types) */
static b8 get_entity_arg(const BavValue* args, u32 arg_count, u32 idx, BavEntity* out)
{
    if (idx >= arg_count)
        return false;

    if (args[idx].type == BAV_VALUE_HANDLE)
    {
        *out = handle_to_entity(args[idx].as_handle);
        return true;
    }
    else if (args[idx].type == BAV_VALUE_NUMBER)
    {
        /* Support passing entity as packed u64 number */
        u64 packed = (u64)args[idx].as_number;
        out->index = (u32)(packed >> 32);
        out->generation = (u16)(packed >> 16);
        out->flags = (u16)(packed);
        return true;
    }
    return false;
}

/* Get component ID from Lua argument */
static b8 get_component_arg(const BavValue* args, u32 arg_count, u32 idx, BavComponentId* out)
{
    if (idx >= arg_count)
        return false;
    if (args[idx].type != BAV_VALUE_NUMBER)
        return false;
    *out = (BavComponentId)args[idx].as_number;
    return true;
}

/* Find component descriptor by ID */
static const BavComponentDesc* find_component_desc(BavComponentId id)
{
    for (u32 i = 0; i < g_component_desc_count; i++)
    {
        if (g_component_descs[i].id == id)
            return &g_component_descs[i];
    }
    return NULL;
}

/* =============================================================================
 * Entity Functions
 * ============================================================================= */

/* ecs.create() -> entity handle */
static BavCallResult lua_ecs_create(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                    void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(args);
    BAV_UNUSED(arg_count);
    BAV_UNUSED(user_data);

    if (!g_ecs_admin)
        return make_error("ECS not initialized");

    BavEntity e = bav_entity_create(g_ecs_admin);
    return make_handle(entity_to_handle(e));
}

/* ecs.destroy(entity) */
static BavCallResult lua_ecs_destroy(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                     void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    if (!g_ecs_admin)
        return make_error("ECS not initialized");

    BavEntity e;
    if (!get_entity_arg(args, arg_count, 0, &e))
        return make_error("ecs.destroy requires entity argument");

    bav_entity_destroy(g_ecs_admin, e);
    return make_nil();
}

/* ecs.valid(entity) -> bool */
static BavCallResult lua_ecs_valid(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                   void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    if (!g_ecs_admin)
        return make_error("ECS not initialized");

    BavEntity e;
    if (!get_entity_arg(args, arg_count, 0, &e))
        return make_bool(false);

    return make_bool(bav_entity_valid(g_ecs_admin, e));
}

/* ecs.count() -> number of live entities */
static BavCallResult lua_ecs_count(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                   void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(args);
    BAV_UNUSED(arg_count);
    BAV_UNUSED(user_data);

    if (!g_ecs_admin)
        return make_error("ECS not initialized");

    return make_number((f64)bav_entity_count(g_ecs_admin));
}

/* ecs.flush() - process deferred operations */
static BavCallResult lua_ecs_flush(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                   void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(args);
    BAV_UNUSED(arg_count);
    BAV_UNUSED(user_data);

    if (!g_ecs_admin)
        return make_error("ECS not initialized");

    bav_entity_admin_flush(g_ecs_admin);
    return make_nil();
}

/* =============================================================================
 * Component Functions
 * ============================================================================= */

/* ecs.has(entity, component_id) -> bool */
static BavCallResult lua_ecs_has(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                 void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    if (!g_ecs_admin)
        return make_error("ECS not initialized");

    BavEntity e;
    BavComponentId comp;
    if (!get_entity_arg(args, arg_count, 0, &e))
        return make_error("ecs.has requires entity argument");
    if (!get_component_arg(args, arg_count, 1, &comp))
        return make_error("ecs.has requires component_id argument");

    return make_bool(bav_entity_has_component(g_ecs_admin, e, comp));
}

/* ecs.add(entity, component_id, data_table) */
static BavCallResult lua_ecs_add(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                 void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    if (!g_ecs_admin)
        return make_error("ECS not initialized");

    BavEntity e;
    BavComponentId comp;
    if (!get_entity_arg(args, arg_count, 0, &e))
        return make_error("ecs.add requires entity argument");
    if (!get_component_arg(args, arg_count, 1, &comp))
        return make_error("ecs.add requires component_id argument");

    /* Look up component descriptor for size */
    const BavComponentDesc* desc = find_component_desc(comp);
    if (!desc)
    {
        /* No descriptor - use component info from ECS directly */
        const BavComponentInfo* info = bav_component_get_info(g_ecs_admin, comp);
        if (!info)
            return make_error("unknown component type");

        /* Add with zeroed data - caller can use ecs.set to fill it */
        void* data = calloc(1, info->size);
        bav_entity_add_component(g_ecs_admin, e, comp, data);
        free(data);
    }
    else
    {
        /* Have descriptor - could marshal table data in the future */
        /* For now, just add zeroed component */
        void* data = calloc(1, desc->size);
        bav_entity_add_component(g_ecs_admin, e, comp, data);
        free(data);
    }

    return make_nil();
}

/* ecs.remove(entity, component_id) */
static BavCallResult lua_ecs_remove(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                    void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    if (!g_ecs_admin)
        return make_error("ECS not initialized");

    BavEntity e;
    BavComponentId comp;
    if (!get_entity_arg(args, arg_count, 0, &e))
        return make_error("ecs.remove requires entity argument");
    if (!get_component_arg(args, arg_count, 1, &comp))
        return make_error("ecs.remove requires component_id argument");

    bav_entity_remove_component(g_ecs_admin, e, comp);
    return make_nil();
}

/* =============================================================================
 * Component Data Access
 *
 * These are lower-level functions for getting/setting component field values.
 * Until we have proper table marshaling, scripts use these to read/write
 * individual fields by name or offset.
 * ============================================================================= */

/* ecs.get_field(entity, component_id, field_name_or_offset) -> value */
static BavCallResult lua_ecs_get_field(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                       void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    if (!g_ecs_admin)
        return make_error("ECS not initialized");

    if (arg_count < 3)
        return make_error("ecs.get_field requires entity, component_id, field");

    BavEntity e;
    BavComponentId comp;
    if (!get_entity_arg(args, arg_count, 0, &e))
        return make_error("invalid entity argument");
    if (!get_component_arg(args, arg_count, 1, &comp))
        return make_error("invalid component_id argument");

    void* data = bav_entity_get_component(g_ecs_admin, e, comp);
    if (!data)
        return make_error("entity doesn't have this component");

    const BavComponentDesc* desc = find_component_desc(comp);

    /* Field access by offset (number) */
    if (args[2].type == BAV_VALUE_NUMBER)
    {
        usize offset = (usize)args[2].as_number;

        /* Default to reading f32 */
        const BavComponentInfo* info = bav_component_get_info(g_ecs_admin, comp);
        if (info && offset + sizeof(f32) <= info->size)
        {
            f32* ptr = (f32*)((u8*)data + offset);
            return make_number((f64)*ptr);
        }
        return make_error("offset out of bounds");
    }

    /* Field access by name (string) */
    if (args[2].type == BAV_VALUE_STRING && desc)
    {
        const char* field_name = args[2].as_string.data;
        usize field_len = args[2].as_string.length;

        for (u32 i = 0; i < desc->field_count; i++)
        {
            if (strlen(desc->fields[i].name) == field_len &&
                strncmp(desc->fields[i].name, field_name, field_len) == 0)
            {
                const BavFieldDesc* field = &desc->fields[i];
                u8* ptr = (u8*)data + field->offset;

                switch (field->type)
                {
                    case BAV_FIELD_F32:
                        return make_number((f64)*(f32*)ptr);
                    case BAV_FIELD_F64:
                        return make_number(*(f64*)ptr);
                    case BAV_FIELD_I32:
                        return make_number((f64)*(i32*)ptr);
                    case BAV_FIELD_U32:
                        return make_number((f64)*(u32*)ptr);
                    case BAV_FIELD_BOOL:
                        return make_bool(*(b8*)ptr);
                    default:
                        return make_error("unsupported field type for single value read");
                }
            }
        }
        return make_error("field not found");
    }

    return make_error("field argument must be number (offset) or string (name)");
}

/* ecs.set_field(entity, component_id, field_name_or_offset, value) */
static BavCallResult lua_ecs_set_field(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                       void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    if (!g_ecs_admin)
        return make_error("ECS not initialized");

    if (arg_count < 4)
        return make_error("ecs.set_field requires entity, component_id, field, value");

    BavEntity e;
    BavComponentId comp;
    if (!get_entity_arg(args, arg_count, 0, &e))
        return make_error("invalid entity argument");
    if (!get_component_arg(args, arg_count, 1, &comp))
        return make_error("invalid component_id argument");

    void* data = bav_entity_get_component(g_ecs_admin, e, comp);
    if (!data)
        return make_error("entity doesn't have this component");

    const BavComponentDesc* desc = find_component_desc(comp);

    /* Field access by offset (number) */
    if (args[2].type == BAV_VALUE_NUMBER)
    {
        usize offset = (usize)args[2].as_number;

        const BavComponentInfo* info = bav_component_get_info(g_ecs_admin, comp);
        if (info && offset + sizeof(f32) <= info->size)
        {
            if (args[3].type == BAV_VALUE_NUMBER)
            {
                f32* ptr = (f32*)((u8*)data + offset);
                *ptr = (f32)args[3].as_number;
                return make_nil();
            }
            return make_error("value must be a number");
        }
        return make_error("offset out of bounds");
    }

    /* Field access by name (string) */
    if (args[2].type == BAV_VALUE_STRING && desc)
    {
        const char* field_name = args[2].as_string.data;
        usize field_len = args[2].as_string.length;

        for (u32 i = 0; i < desc->field_count; i++)
        {
            if (strlen(desc->fields[i].name) == field_len &&
                strncmp(desc->fields[i].name, field_name, field_len) == 0)
            {
                const BavFieldDesc* field = &desc->fields[i];
                u8* ptr = (u8*)data + field->offset;

                switch (field->type)
                {
                    case BAV_FIELD_F32:
                        if (args[3].type != BAV_VALUE_NUMBER)
                            return make_error("expected number value");
                        *(f32*)ptr = (f32)args[3].as_number;
                        return make_nil();

                    case BAV_FIELD_F64:
                        if (args[3].type != BAV_VALUE_NUMBER)
                            return make_error("expected number value");
                        *(f64*)ptr = args[3].as_number;
                        return make_nil();

                    case BAV_FIELD_I32:
                        if (args[3].type != BAV_VALUE_NUMBER)
                            return make_error("expected number value");
                        *(i32*)ptr = (i32)args[3].as_number;
                        return make_nil();

                    case BAV_FIELD_U32:
                        if (args[3].type != BAV_VALUE_NUMBER)
                            return make_error("expected number value");
                        *(u32*)ptr = (u32)args[3].as_number;
                        return make_nil();

                    case BAV_FIELD_BOOL:
                        if (args[3].type != BAV_VALUE_BOOL)
                            return make_error("expected boolean value");
                        *(b8*)ptr = args[3].as_bool;
                        return make_nil();

                    default:
                        return make_error("unsupported field type for single value write");
                }
            }
        }
        return make_error("field not found");
    }

    return make_error("field argument must be number (offset) or string (name)");
}

/* =============================================================================
 * Vector Field Helpers
 *
 * These are convenience functions for reading/writing vec3 fields, since
 * transform components use them a lot.
 * ============================================================================= */

/* ecs.get_vec3(entity, component_id, field_name_or_offset) -> x, y, z */
static BavCallResult lua_ecs_get_vec3(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                      void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    if (!g_ecs_admin)
        return make_error("ECS not initialized");

    if (arg_count < 3)
        return make_error("ecs.get_vec3 requires entity, component_id, field");

    BavEntity e;
    BavComponentId comp;
    if (!get_entity_arg(args, arg_count, 0, &e))
        return make_error("invalid entity argument");
    if (!get_component_arg(args, arg_count, 1, &comp))
        return make_error("invalid component_id argument");

    void* data = bav_entity_get_component(g_ecs_admin, e, comp);
    if (!data)
        return make_error("entity doesn't have this component");

    f32* vec = NULL;

    if (args[2].type == BAV_VALUE_NUMBER)
    {
        usize offset = (usize)args[2].as_number;
        vec = (f32*)((u8*)data + offset);
    }
    else if (args[2].type == BAV_VALUE_STRING)
    {
        const BavComponentDesc* desc = find_component_desc(comp);
        if (!desc)
            return make_error("component has no descriptor, use offset");

        const char* field_name = args[2].as_string.data;
        usize field_len = args[2].as_string.length;

        for (u32 i = 0; i < desc->field_count; i++)
        {
            if (strlen(desc->fields[i].name) == field_len &&
                strncmp(desc->fields[i].name, field_name, field_len) == 0)
            {
                if (desc->fields[i].type != BAV_FIELD_VEC3)
                    return make_error("field is not a vec3");
                vec = (f32*)((u8*)data + desc->fields[i].offset);
                break;
            }
        }
        if (!vec)
            return make_error("field not found");
    }
    else
    {
        return make_error("field argument must be number (offset) or string (name)");
    }

    BavCallResult r = {0};
    r.success = true;
    r.value_count = 3;
    r.values = malloc(3 * sizeof(BavValue));
    r.values[0].type = BAV_VALUE_NUMBER;
    r.values[0].as_number = (f64)vec[0];
    r.values[1].type = BAV_VALUE_NUMBER;
    r.values[1].as_number = (f64)vec[1];
    r.values[2].type = BAV_VALUE_NUMBER;
    r.values[2].as_number = (f64)vec[2];
    return r;
}

/* ecs.set_vec3(entity, component_id, field_name_or_offset, x, y, z) */
static BavCallResult lua_ecs_set_vec3(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                      void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    if (!g_ecs_admin)
        return make_error("ECS not initialized");

    if (arg_count < 6)
        return make_error("ecs.set_vec3 requires entity, component_id, field, x, y, z");

    BavEntity e;
    BavComponentId comp;
    if (!get_entity_arg(args, arg_count, 0, &e))
        return make_error("invalid entity argument");
    if (!get_component_arg(args, arg_count, 1, &comp))
        return make_error("invalid component_id argument");

    void* data = bav_entity_get_component(g_ecs_admin, e, comp);
    if (!data)
        return make_error("entity doesn't have this component");

    f32* vec = NULL;

    if (args[2].type == BAV_VALUE_NUMBER)
    {
        usize offset = (usize)args[2].as_number;
        vec = (f32*)((u8*)data + offset);
    }
    else if (args[2].type == BAV_VALUE_STRING)
    {
        const BavComponentDesc* desc = find_component_desc(comp);
        if (!desc)
            return make_error("component has no descriptor, use offset");

        const char* field_name = args[2].as_string.data;
        usize field_len = args[2].as_string.length;

        for (u32 i = 0; i < desc->field_count; i++)
        {
            if (strlen(desc->fields[i].name) == field_len &&
                strncmp(desc->fields[i].name, field_name, field_len) == 0)
            {
                if (desc->fields[i].type != BAV_FIELD_VEC3)
                    return make_error("field is not a vec3");
                vec = (f32*)((u8*)data + desc->fields[i].offset);
                break;
            }
        }
        if (!vec)
            return make_error("field not found");
    }
    else
    {
        return make_error("field argument must be number (offset) or string (name)");
    }

    if (args[3].type != BAV_VALUE_NUMBER || args[4].type != BAV_VALUE_NUMBER ||
        args[5].type != BAV_VALUE_NUMBER)
    {
        return make_error("x, y, z must be numbers");
    }

    vec[0] = (f32)args[3].as_number;
    vec[1] = (f32)args[4].as_number;
    vec[2] = (f32)args[5].as_number;

    return make_nil();
}

/* =============================================================================
 * Query Functions
 * ============================================================================= */

/* ecs.query_count(component_id, ...) -> number of matching entities */
static BavCallResult lua_ecs_query_count(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                         void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    if (!g_ecs_admin)
        return make_error("ECS not initialized");

    if (arg_count == 0)
        return make_error("ecs.query_count requires at least one component_id");

    /* Build component ID array */
    BavComponentId comp_ids[BAV_MAX_COMPONENTS];
    u32 comp_count = 0;

    for (u32 i = 0; i < arg_count && comp_count < BAV_MAX_COMPONENTS; i++)
    {
        if (args[i].type == BAV_VALUE_NUMBER)
        {
            comp_ids[comp_count++] = (BavComponentId)args[i].as_number;
        }
    }

    if (comp_count == 0)
        return make_error("no valid component IDs provided");

    BavQuery query = bav_query_require(comp_ids, comp_count);
    u32 count = bav_query_count(g_ecs_admin, &query);

    return make_number((f64)count);
}

/* =============================================================================
 * Component Registration Functions
 * ============================================================================= */

/* ecs.register_component(name, size, alignment) -> component_id */
static BavCallResult lua_ecs_register_component(BavScriptContext* ctx, const BavValue* args,
                                                u32 arg_count, void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    if (!g_ecs_admin)
        return make_error("ECS not initialized");

    if (arg_count < 3)
        return make_error("ecs.register_component requires name, size, alignment");

    if (args[0].type != BAV_VALUE_STRING)
        return make_error("name must be a string");
    if (args[1].type != BAV_VALUE_NUMBER)
        return make_error("size must be a number");
    if (args[2].type != BAV_VALUE_NUMBER)
        return make_error("alignment must be a number");

    /* We need a persistent copy of the name since the string might be temporary */
    static char name_storage[MAX_COMPONENT_DESCS][64];
    if (g_component_desc_count >= MAX_COMPONENT_DESCS)
        return make_error("too many component types registered");

    usize name_len = args[0].as_string.length;
    if (name_len >= 64)
        name_len = 63;
    memcpy(name_storage[g_component_desc_count], args[0].as_string.data, name_len);
    name_storage[g_component_desc_count][name_len] = '\0';

    usize size = (usize)args[1].as_number;
    usize alignment = (usize)args[2].as_number;

    BavComponentId id =
        bav_component_register(g_ecs_admin, name_storage[g_component_desc_count], size, alignment);

    if (id == BAV_COMPONENT_INVALID)
        return make_error("failed to register component");

    /* Store minimal descriptor (no fields for script-registered components) */
    g_component_descs[g_component_desc_count].id = id;
    g_component_descs[g_component_desc_count].name = name_storage[g_component_desc_count];
    g_component_descs[g_component_desc_count].size = size;
    g_component_descs[g_component_desc_count].alignment = alignment;
    g_component_descs[g_component_desc_count].fields = NULL;
    g_component_descs[g_component_desc_count].field_count = 0;
    g_component_desc_count++;

    return make_number((f64)id);
}

/* ecs.archetype_count() -> number of archetypes (for debugging) */
static BavCallResult lua_ecs_archetype_count(BavScriptContext* ctx, const BavValue* args,
                                             u32 arg_count, void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(args);
    BAV_UNUSED(arg_count);
    BAV_UNUSED(user_data);

    if (!g_ecs_admin)
        return make_error("ECS not initialized");

    return make_number((f64)bav_archetype_count(g_ecs_admin));
}

/* =============================================================================
 * Internal API (for engine use)
 * ============================================================================= */

void bav_lua_set_ecs_admin(BavEntityAdmin* admin)
{
    g_ecs_admin = admin;
}

BavEntityAdmin* bav_lua_get_ecs_admin(void)
{
    return g_ecs_admin;
}

void bav_lua_register_component_desc(BavComponentId id, const char* name, usize size,
                                     usize alignment, const BavFieldDesc* fields, u32 field_count)
{
    if (g_component_desc_count >= MAX_COMPONENT_DESCS)
        return;

    g_component_descs[g_component_desc_count].id = id;
    g_component_descs[g_component_desc_count].name = name;
    g_component_descs[g_component_desc_count].size = size;
    g_component_descs[g_component_desc_count].alignment = alignment;
    g_component_descs[g_component_desc_count].fields = fields;
    g_component_descs[g_component_desc_count].field_count = field_count;
    g_component_desc_count++;
}

/* =============================================================================
 * Registration
 * ============================================================================= */

void bav_lua_register_ecs(BavScriptContext* ctx)
{
    static BavNativeFnDef ecs_funcs[] = {
        /* Entity operations */
        {"create", lua_ecs_create},
        {"destroy", lua_ecs_destroy},
        {"valid", lua_ecs_valid},
        {"count", lua_ecs_count},
        {"flush", lua_ecs_flush},

        /* Component operations */
        {"has", lua_ecs_has},
        {"add", lua_ecs_add},
        {"remove", lua_ecs_remove},

        /* Field access */
        {"get_field", lua_ecs_get_field},
        {"set_field", lua_ecs_set_field},
        {"get_vec3", lua_ecs_get_vec3},
        {"set_vec3", lua_ecs_set_vec3},

        /* Queries */
        {"query_count", lua_ecs_query_count},

        /* Registration */
        {"register_component", lua_ecs_register_component},

        /* Debug */
        {"archetype_count", lua_ecs_archetype_count},
    };

    bav_script_register_module(ctx, "ecs", ecs_funcs, BAV_ARRAY_COUNT(ecs_funcs), NULL);
}
