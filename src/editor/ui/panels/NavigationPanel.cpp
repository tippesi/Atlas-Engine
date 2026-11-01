#include "NavigationPanel.h"
#include "../../Theme.h"
#include "../Widgets.h"

namespace Atlas::Editor::UI {

    void NavigationPanel::Render(Ref<Navigation::Navigation>& navigation) {

        if (!navigation) {
            Widgets::Callout("The scene has no navigation mesh. It is built around the camera from the static bodies and the terrain.",
                Theme::TextMuted());
            Theme::PushPrimaryButton();
            if (ImGui::Button("Create navigation", ImVec2(-FLT_MIN, 0.0f)))
                navigation = CreateRef<Navigation::Navigation>();
            Theme::PopPrimaryButton();
            return;
        }

        if (Widgets::SectionHeader("General", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Enable", &navigation->enable);
            ImGui::SetItemTooltip("Builds the navigation mesh around the camera while the scene updates");
            auto maxRadius = navigation->mesh ? navigation->mesh->GetMaxRadius() : 500.0f;
            ImGui::DragFloat("Radius", &navigation->radius, 1.0f, 16.0f, maxRadius, "%.0f m", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SetItemTooltip("Tiles are built within this distance around the camera, tiles further away are removed.\n"
                "The mesh holds 1024 tiles, which limits the radius to %.0f m with the current tile size", maxRadius);

            if (navigation->mesh) {
                ImGui::TextColored(Theme::TextMuted(), "%d tiles built, %d pending", navigation->mesh->GetTileCount(),
                    navigation->mesh->GetPendingTileCount());
                if (ImGui::Button("Rebuild", ImVec2(-FLT_MIN, 0.0f)))
                    navigation->mesh->Invalidate();
                ImGui::SetItemTooltip("Builds all tiles again. Tiles also rebuild by themselves where static bodies or terrain heights change");
            }
        }

        auto& settings = navigation->settings;
        if (Widgets::SectionHeader("Agent")) {
            ImGui::SliderFloat("Radius##Agent", &settings.agentRadius, 0.05f, 3.0f, "%.2f m");
            ImGui::SetItemTooltip("How far the mesh stays away from walls and edges, about the radius of a character");
            ImGui::SliderFloat("Height", &settings.agentHeight, 0.2f, 5.0f, "%.2f m");
            ImGui::SetItemTooltip("The free space a character needs above the floor, lower passages aren't walkable");
            ImGui::SliderFloat("Max climb", &settings.agentMaxClimb, 0.0f, 2.0f, "%.2f m");
            ImGui::SetItemTooltip("Steps up to this height are walked over, higher ledges separate the areas");
            ImGui::SliderFloat("Max slope", &settings.agentMaxSlope, 0.0f, 85.0f, "%.0f deg");
            ImGui::SetItemTooltip("Steeper ground isn't walkable");
        }

        if (Widgets::SectionHeader("Voxelization")) {
            ImGui::SliderFloat("Cell size", &settings.cellSize, 0.05f, 1.0f, "%.2f m");
            ImGui::SetItemTooltip("Horizontal resolution of the voxels the mesh is built from. Smaller cells follow walls and\n"
                "gaps more exactly, but take longer to build and produce more polygons");
            ImGui::SliderFloat("Cell height", &settings.cellHeight, 0.05f, 1.0f, "%.2f m");
            ImGui::SetItemTooltip("Vertical resolution of the voxels, steps and the agent height are rounded to it");
            ImGui::SliderFloat("Tile size", &settings.tileSize, 8.0f, 128.0f, "%.0f m");
            ImGui::SetItemTooltip("Side length of the tiles which are built and replaced on their own. Larger tiles allow a larger\n"
                "radius, but take longer to rebuild and can exceed the 4096 polygons of a tile in dense areas\n"
                "(the tile then stays empty and the log names it). It doesn't change the precision");
            ImGui::TextColored(Theme::TextMuted(), "Changing the agent or voxelization settings rebuilds the mesh");
        }

        if (Widgets::SectionHeader("Building")) {
            ImGui::SliderInt("Tiles per frame", &navigation->maxTileStartsPerUpdate, 1, 16, "%d", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SetItemTooltip("Tiles whose geometry (static bodies and terrain) is collected on the main thread each frame.\n"
                "Fewer keep the frame time steady, more fill a new area faster");
            ImGui::SliderInt("Concurrent builds", &navigation->maxConcurrentBuilds, 1, 32, "%d", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SetItemTooltip("Tiles built in jobs at the same time. Each build takes one worker of the pool below");

            const char* priorities[] = { "High", "Medium", "Low" };
            auto priority = int32_t(navigation->buildPriority);
            if (ImGui::Combo("Job priority", &priority, priorities, IM_ARRAYSIZE(priorities)))
                navigation->buildPriority = JobPriority(priority);
            ImGui::SetItemTooltip("The worker pool of the job system the tiles are built in. High shares the workers of the\n"
                "frame (scene update, rendering), medium and low have their own workers with a lower thread priority");
        }

        if (Widgets::SectionHeader("Debug", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Show mesh", &showMesh);
            ImGui::SetItemTooltip("Draws the edges of the polygons of all built tiles");
            auto pathTestChanged = ImGui::Checkbox("Path test", &pathTest);
            ImGui::SetItemTooltip("Finds a path between two points in the viewport, like the characters of a game would");
            if (pathTestChanged && !pathTest) {
                pathStart.reset();
                pathGoal.reset();
                path.clear();
            }

            if (pathTest) {
                ImGui::TextColored(Theme::TextMuted(), "Right click sets the start, the next one the goal");
                if (pathStart && pathGoal) {
                    if (path.empty()) {
                        Widgets::Callout("No path between the points on the built tiles", Theme::Warning());
                    }
                    else {
                        auto length = 0.0f;
                        for (size_t i = 1; i < path.size(); i++)
                            length += glm::distance(path[i - 1], path[i]);
                        ImGui::Text("%d corners, %.1f m", int32_t(path.size()), length);
                    }
                }
            }
        }

        ImGui::Separator();
        if (ImGui::Button("Remove navigation", ImVec2(-FLT_MIN, 0.0f)))
            navigation = nullptr;
        ImGui::SetItemTooltip("Removes the navigation from the scene, scripts which use it don't find paths anymore");

    }

}
