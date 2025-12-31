/**
 * @file hierarchy_panel.cpp
 * @brief Scene Hierarchy Panel
 *
 * Displays entity tree with parent-child relationships.
 * Supports entity selection, creation, and deletion.
 */

#include <bavarian/ecs.h>
#include <bavarian/editor.h>
#include <cstdio>
#include <imgui.h>

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
    if (entity_count > 0)
    {
        /* Get all live entities */
        static BavEntity entities[1024];
        u32 count = bav_entity_get_all(admin, entities, 1024);

        for (u32 i = 0; i < count; i++)
        {
            draw_entity_node(editor, admin, entities[i]);
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
