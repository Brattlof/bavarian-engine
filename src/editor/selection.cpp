/**
 * @file selection.cpp
 * @brief Selection System
 *
 * Manages entity selection state and provides selection utilities.
 * The actual selection state is stored in BavEditor; this file provides
 * additional selection-related functionality.
 */

#include <bavarian/editor.h>
#include <bavarian/ecs.h>

/* Forward declaration */
extern BavEntityAdmin* editor_get_ecs_admin(BavEditor* editor);

/* =============================================================================
 * Selection Utilities
 * ============================================================================= */

/* Check if an entity is currently selected */
bool editor_is_entity_selected(BavEditor* editor, BavEntity entity)
{
    u32 count = 0;
    const BavEntity* selected = bav_editor_get_selected_entities(editor, &count);

    for (u32 i = 0; i < count; i++)
    {
        if (selected[i].index == entity.index && selected[i].generation == entity.generation)
        {
            return true;
        }
    }

    return false;
}

/* Add an entity to the current selection (multi-select) */
void editor_add_to_selection(BavEditor* editor, BavEntity entity)
{
    if (editor_is_entity_selected(editor, entity))
        return;

    u32 count = 0;
    const BavEntity* current = bav_editor_get_selected_entities(editor, &count);

    /* Build new selection array */
    BavEntity* new_selection = new BavEntity[count + 1];
    for (u32 i = 0; i < count; i++)
    {
        new_selection[i] = current[i];
    }
    new_selection[count] = entity;

    bav_editor_select_entities(editor, new_selection, count + 1);

    delete[] new_selection;
}

/* Remove an entity from the current selection */
void editor_remove_from_selection(BavEditor* editor, BavEntity entity)
{
    u32 count = 0;
    const BavEntity* current = bav_editor_get_selected_entities(editor, &count);

    if (count == 0)
        return;

    /* Build new selection array without the entity */
    BavEntity* new_selection = new BavEntity[count];
    u32 new_count = 0;

    for (u32 i = 0; i < count; i++)
    {
        if (current[i].index != entity.index || current[i].generation != entity.generation)
        {
            new_selection[new_count++] = current[i];
        }
    }

    bav_editor_select_entities(editor, new_selection, new_count);

    delete[] new_selection;
}

/* Toggle entity in selection */
void editor_toggle_selection(BavEditor* editor, BavEntity entity)
{
    if (editor_is_entity_selected(editor, entity))
    {
        editor_remove_from_selection(editor, entity);
    }
    else
    {
        editor_add_to_selection(editor, entity);
    }
}

/* Select all entities */
void editor_select_all(BavEditor* editor)
{
    BavEntityAdmin* admin = editor_get_ecs_admin(editor);
    if (!admin)
        return;

    /* This is a simplified implementation - real one would iterate all valid entities */
    u32 count = bav_entity_count(admin);
    if (count == 0)
        return;

    /* For now, just clear selection - proper implementation needs entity iteration API */
    BavEntity null_entity = {0, 0, 0};
    bav_editor_select_entity(editor, null_entity);
}

/* Deselect all entities */
void editor_deselect_all(BavEditor* editor)
{
    BavEntity null_entity = {0, 0, 0};
    bav_editor_select_entity(editor, null_entity);
}

/* Invert selection */
void editor_invert_selection(BavEditor* editor)
{
    /* This would require iterating all entities and toggling selection */
    /* For now, just deselect all */
    editor_deselect_all(editor);
}

/* Get selection bounds (AABB containing all selected entities) */
void editor_get_selection_bounds(BavEditor* editor, f32* min_out, f32* max_out)
{
    BAV_UNUSED(editor);

    /* Initialize to invalid bounds */
    min_out[0] = min_out[1] = min_out[2] = 1e10f;
    max_out[0] = max_out[1] = max_out[2] = -1e10f;

    /* TODO: Query transform components of selected entities to compute bounds */
}

/* Get selection center */
void editor_get_selection_center(BavEditor* editor, f32* center_out)
{
    f32 min_bounds[3], max_bounds[3];
    editor_get_selection_bounds(editor, min_bounds, max_bounds);

    center_out[0] = (min_bounds[0] + max_bounds[0]) * 0.5f;
    center_out[1] = (min_bounds[1] + max_bounds[1]) * 0.5f;
    center_out[2] = (min_bounds[2] + max_bounds[2]) * 0.5f;
}
