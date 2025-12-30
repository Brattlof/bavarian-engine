/**
 * @file hierarchy_panel.cpp
 * @brief Scene Hierarchy Panel
 *
 * Displays entity tree with parent-child relationships.
 * Supports entity selection, creation, and deletion.
 */

#include <bavarian/editor.h>
#include <bavarian/ecs.h>

#include <imgui.h>
#include <cstdio>

/* Forward declaration */
extern BavEntityAdmin* editor_get_ecs_admin(BavEditor* editor);

static void draw_entity_node(BavEditor* editor, BavEntityAdmin* admin, BavEntity entity)
{
    /* Get entity name if available, otherwise use ID */
    char label[64];
    snprintf(label, sizeof(label), "Entity %u", entity.index);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

    /* Check if this entity is selected */
    BavEntity selected = bav_editor_get_selected_entity(editor);
    if (selected.index == entity.index && selected.generation == entity.generation)
    {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    /* For now, all entities are leaf nodes (no hierarchy yet) */
    flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    ImGui::TreeNodeEx((void*)(intptr_t)entity.index, flags, "%s", label);

    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
    {
        bav_editor_select_entity(editor, entity);
    }

    /* Context menu */
    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::MenuItem("Delete"))
        {
            bav_entity_destroy(admin, entity);
            BavEntity null_entity = {0, 0, 0};
            bav_editor_select_entity(editor, null_entity);
        }
        if (ImGui::MenuItem("Duplicate"))
        {
            /* TODO: Implement entity duplication */
        }
        ImGui::EndPopup();
    }
}

void editor_hierarchy_panel_update(BavEditor* editor)
{
    if (!ImGui::Begin("Hierarchy"))
    {
        ImGui::End();
        return;
    }

    BavEntityAdmin* admin = editor_get_ecs_admin(editor);
    if (!admin)
    {
        ImGui::Text("No scene loaded");
        ImGui::End();
        return;
    }

    /* Toolbar */
    if (ImGui::Button("+ Entity"))
    {
        BavEntity new_entity = bav_entity_create(admin);
        bav_editor_select_entity(editor, new_entity);
    }

    ImGui::Separator();

    /* Entity count */
    u32 entity_count = bav_entity_count(admin);
    ImGui::Text("Entities: %u", entity_count);

    ImGui::Separator();

    /* Entity list */
    /* For now, iterate all entities - in real implementation we'd have
     * a more efficient way to get the entity list */
    if (entity_count > 0)
    {
        /* We need to query for all entities - use a simple approach */
        /* Since we don't have a direct "get all entities" API, we'll
         * iterate through indices checking validity */
        for (u32 i = 1; i <= entity_count * 2 && i < 10000; i++)
        {
            BavEntity entity;
            entity.index = i;
            entity.generation = 0; /* Will be validated */
            entity.flags = 0;

            /* Check if entity exists at this index */
            /* This is a hack - proper implementation would have
             * an entity iteration API */
            if (bav_entity_valid(admin, entity))
            {
                draw_entity_node(editor, admin, entity);
            }
        }
    }

    /* Right-click on empty space */
    if (ImGui::BeginPopupContextWindow("HierarchyContextMenu", ImGuiPopupFlags_NoOpenOverItems))
    {
        if (ImGui::MenuItem("Create Empty Entity"))
        {
            BavEntity new_entity = bav_entity_create(admin);
            bav_editor_select_entity(editor, new_entity);
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}
