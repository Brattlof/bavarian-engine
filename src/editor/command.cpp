/**
 * @file command.cpp
 * @brief Command System for Undo/Redo
 *
 * Implements common editor commands that can be undone/redone.
 */

#include <bavarian/editor.h>
#include <bavarian/ecs.h>

#include <cstdlib>
#include <cstring>

/* =============================================================================
 * Command Data Structures
 * ============================================================================= */

/* Create entity command data */
struct CreateEntityData
{
    BavEntity created_entity;
};

/* Delete entity command data */
struct DeleteEntityData
{
    BavEntity deleted_entity;
    /* TODO: Store component data for restoration */
};

/* Modify component command data */
struct ModifyComponentData
{
    BavEntity entity;
    BavComponentId component_id;
    void* old_data;
    void* new_data;
    usize data_size;
};

/* =============================================================================
 * Create Entity Command
 * ============================================================================= */

static void create_entity_execute(BavEditor* editor, void* user_data)
{
    BAV_UNUSED(editor);

    CreateEntityData* data = (CreateEntityData*)user_data;

    /* Entity was already created - this is for redo */
    /* In a real implementation, we'd recreate the entity */
    BAV_UNUSED(data);
}

static void create_entity_undo(BavEditor* editor, void* user_data)
{
    BAV_UNUSED(editor);

    CreateEntityData* data = (CreateEntityData*)user_data;

    /* Destroy the created entity */
    /* In a real implementation, we'd also preserve component data */
    BAV_UNUSED(data);
}

void editor_cmd_create_entity(BavEditor* editor, BavEntity entity)
{
    CreateEntityData data = {};
    data.created_entity = entity;

    BavCommandDef cmd = {};
    cmd.name = "Create Entity";
    cmd.execute = create_entity_execute;
    cmd.undo = create_entity_undo;
    cmd.user_data = &data;
    cmd.user_data_size = sizeof(CreateEntityData);

    bav_editor_execute_command(editor, &cmd);
}

/* =============================================================================
 * Delete Entity Command
 * ============================================================================= */

static void delete_entity_execute(BavEditor* editor, void* user_data)
{
    BAV_UNUSED(editor);

    DeleteEntityData* data = (DeleteEntityData*)user_data;

    /* Delete the entity */
    BAV_UNUSED(data);
}

static void delete_entity_undo(BavEditor* editor, void* user_data)
{
    BAV_UNUSED(editor);

    DeleteEntityData* data = (DeleteEntityData*)user_data;

    /* Restore the entity with its component data */
    BAV_UNUSED(data);
}

void editor_cmd_delete_entity(BavEditor* editor, BavEntity entity)
{
    DeleteEntityData data = {};
    data.deleted_entity = entity;
    /* TODO: Store component data */

    BavCommandDef cmd = {};
    cmd.name = "Delete Entity";
    cmd.execute = delete_entity_execute;
    cmd.undo = delete_entity_undo;
    cmd.user_data = &data;
    cmd.user_data_size = sizeof(DeleteEntityData);

    bav_editor_execute_command(editor, &cmd);
}

/* =============================================================================
 * Modify Component Command
 * ============================================================================= */

static void modify_component_execute(BavEditor* editor, void* user_data)
{
    BAV_UNUSED(editor);

    ModifyComponentData* data = (ModifyComponentData*)user_data;

    /* Apply new data */
    BAV_UNUSED(data);
}

static void modify_component_undo(BavEditor* editor, void* user_data)
{
    BAV_UNUSED(editor);

    ModifyComponentData* data = (ModifyComponentData*)user_data;

    /* Restore old data */
    BAV_UNUSED(data);
}

void editor_cmd_modify_component(BavEditor* editor, BavEntity entity, BavComponentId component,
                                 const void* old_data, const void* new_data, usize data_size)
{
    ModifyComponentData* data = (ModifyComponentData*)malloc(sizeof(ModifyComponentData) + data_size * 2);
    data->entity = entity;
    data->component_id = component;
    data->data_size = data_size;
    data->old_data = (char*)data + sizeof(ModifyComponentData);
    data->new_data = (char*)data->old_data + data_size;

    memcpy(data->old_data, old_data, data_size);
    memcpy(data->new_data, new_data, data_size);

    BavCommandDef cmd = {};
    cmd.name = "Modify Component";
    cmd.execute = modify_component_execute;
    cmd.undo = modify_component_undo;
    cmd.user_data = data;
    cmd.user_data_size = sizeof(ModifyComponentData) + data_size * 2;

    bav_editor_execute_command(editor, &cmd);

    free(data);
}
