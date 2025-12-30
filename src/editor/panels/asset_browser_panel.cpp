/**
 * @file asset_browser_panel.cpp
 * @brief Asset Browser Panel
 *
 * Displays project assets in a file browser interface.
 * Supports drag-and-drop to scene and asset preview.
 */

#include <bavarian/editor.h>

#include <imgui.h>

void editor_asset_browser_panel_update(BavEditor* editor)
{
    BAV_UNUSED(editor);

    if (!ImGui::Begin("Asset Browser"))
    {
        ImGui::End();
        return;
    }

    /* Toolbar */
    static char search_buffer[128] = "";
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##Search", "Search assets...", search_buffer, sizeof(search_buffer));

    ImGui::SameLine();

    static int view_mode = 0; /* 0 = Grid, 1 = List */
    if (ImGui::Button(view_mode == 0 ? "Grid" : "List"))
    {
        view_mode = 1 - view_mode;
    }

    ImGui::Separator();

    /* Split view: folder tree on left, contents on right */
    if (ImGui::BeginChild("AssetFolders", ImVec2(200, 0), true))
    {
        /* Folder tree placeholder */
        if (ImGui::TreeNode("Assets"))
        {
            if (ImGui::TreeNode("Materials"))
            {
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Meshes"))
            {
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Textures"))
            {
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Scripts"))
            {
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Scenes"))
            {
                ImGui::TreePop();
            }
            ImGui::TreePop();
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    /* Asset contents */
    if (ImGui::BeginChild("AssetContents", ImVec2(0, 0), true))
    {
        if (view_mode == 0) /* Grid view */
        {
            float icon_size = 64.0f;
            float padding = 10.0f;
            float cell_size = icon_size + padding;

            int columns = (int)(ImGui::GetContentRegionAvail().x / cell_size);
            if (columns < 1)
                columns = 1;

            ImGui::Columns(columns, nullptr, false);

            /* Placeholder assets */
            const char* assets[] = {"cube.mesh", "sphere.mesh", "plane.mesh", "default.mat",
                                    "brick.tex",  "wood.tex",    "game.lua",   "level1.scene"};

            for (int i = 0; i < 8; i++)
            {
                ImGui::PushID(i);

                /* Icon placeholder */
                ImGui::Button("##Icon", ImVec2(icon_size, icon_size));

                /* Asset name */
                ImGui::TextWrapped("%s", assets[i]);

                ImGui::NextColumn();
                ImGui::PopID();
            }

            ImGui::Columns(1);
        }
        else /* List view */
        {
            ImGui::Columns(3, "AssetColumns");
            ImGui::Text("Name");
            ImGui::NextColumn();
            ImGui::Text("Type");
            ImGui::NextColumn();
            ImGui::Text("Size");
            ImGui::NextColumn();
            ImGui::Separator();

            /* Placeholder assets */
            const char* assets[][3] = {
                {"cube.mesh", "Mesh", "12 KB"},   {"sphere.mesh", "Mesh", "24 KB"},
                {"plane.mesh", "Mesh", "4 KB"},   {"default.mat", "Material", "1 KB"},
                {"brick.tex", "Texture", "256 KB"}, {"wood.tex", "Texture", "512 KB"},
                {"game.lua", "Script", "8 KB"},   {"level1.scene", "Scene", "32 KB"},
            };

            for (int i = 0; i < 8; i++)
            {
                ImGui::Selectable(assets[i][0], false, ImGuiSelectableFlags_SpanAllColumns);
                ImGui::NextColumn();
                ImGui::Text("%s", assets[i][1]);
                ImGui::NextColumn();
                ImGui::Text("%s", assets[i][2]);
                ImGui::NextColumn();
            }

            ImGui::Columns(1);
        }
    }
    ImGui::EndChild();

    ImGui::End();
}
