/**
 * @file ecs_inspector_panel.cpp
 * @brief ECS Inspector Panel
 *
 * Debug view for ECS internals - archetypes, components, queries.
 * Useful for understanding and debugging entity composition.
 */

#include <bavarian/editor.h>
#include <bavarian/ecs.h>

#include <imgui.h>

/* Forward declarations */
extern BavEntityAdmin* editor_get_ecs_admin(BavEditor* editor);

void editor_ecs_inspector_panel_update(BavEditor* editor)
{
    if (!ImGui::Begin("ECS Inspector"))
    {
        ImGui::End();
        return;
    }

    BavEntityAdmin* admin = editor_get_ecs_admin(editor);
    if (!admin)
    {
        ImGui::TextDisabled("No ECS admin available");
        ImGui::End();
        return;
    }

    /* Tab bar for different views */
    if (ImGui::BeginTabBar("ECSInspectorTabs"))
    {
        /* Entities tab */
        if (ImGui::BeginTabItem("Entities"))
        {
            u32 entity_count = bav_entity_count(admin);
            ImGui::Text("Total Entities: %u", entity_count);
            ImGui::Separator();

            /* Entity list would go here */
            ImGui::TextDisabled("Entity list not implemented");

            ImGui::EndTabItem();
        }

        /* Archetypes tab */
        if (ImGui::BeginTabItem("Archetypes"))
        {
            u32 archetype_count = bav_archetype_count(admin);
            ImGui::Text("Total Archetypes: %u", archetype_count);
            ImGui::Separator();

            /* Archetype details would go here */
            ImGui::TextDisabled("Archetype details not implemented");

            ImGui::EndTabItem();
        }

        /* Components tab */
        if (ImGui::BeginTabItem("Components"))
        {
            ImGui::Text("Registered Components");
            ImGui::Separator();

            /* Component list would go here */
            ImGui::TextDisabled("Component list not implemented");

            ImGui::EndTabItem();
        }

        /* Queries tab */
        if (ImGui::BeginTabItem("Queries"))
        {
            ImGui::Text("Active Queries");
            ImGui::Separator();

            /* Query info would go here */
            ImGui::TextDisabled("Query info not implemented");

            ImGui::EndTabItem();
        }

        /* Systems tab */
        if (ImGui::BeginTabItem("Systems"))
        {
            ImGui::Text("Registered Systems");
            ImGui::Separator();

            /* System list would go here */
            ImGui::TextDisabled("System list not implemented");

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}
