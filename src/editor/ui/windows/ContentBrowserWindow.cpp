#include <imgui_internal.h>
#include "ContentBrowserWindow.h"

#include "mesh/Mesh.h"
#include "scene/Scene.h"
#include "audio/AudioData.h"

namespace Atlas::Editor::UI {

    ContentBrowserWindow::ContentBrowserWindow() : Window("Object browser") {



    }

    void ContentBrowserWindow::Render() {

        ImGui::Begin(GetNameID());

        ImGuiID dsID = ImGui::GetID(dockSpaceNameID.c_str());
        ImGui::DockSpace(dsID, ImVec2(0.0f, 0.0f), 0);

        auto viewport = ImGui::GetWindowViewport();

        if (resetDockingLayout) {
            ImGui::DockBuilderRemoveNode(dsID);
            ImGui::DockBuilderAddNode(dsID, ImGuiDockNodeFlags_DockSpace);

            ImGui::DockBuilderSetNodeSize(dsID, viewport->Size);

            uint32_t dockIdLeft, dockIdRight;
            ImGui::DockBuilderSplitNode(dsID, ImGuiDir_Left, 0.05f, &dockIdLeft, &dockIdRight);

            ImGuiDockNode *leftNode = ImGui::DockBuilderGetNode(dockIdLeft);
            ImGuiDockNode *rightNode = ImGui::DockBuilderGetNode(dockIdRight);
            leftNode->LocalFlags |= ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_NoDockingOverMe;
            rightNode->LocalFlags |= ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_NoDockingOverMe;

            // we now dock our windows into the docking node we made above
            ImGui::DockBuilderDockWindow("ResourceTypeSelection", dockIdLeft);
            ImGui::DockBuilderDockWindow("ResourceTypeOverview", dockIdRight);
            ImGui::DockBuilderFinish(dsID);

            resetDockingLayout = false;
        }

        ImGui::End();

        ImGui::Begin("ResourceTypeSelection", nullptr);
        const char *items[] = {"Audio", "Mesh", "Terrain", "Scene"};
        static int currentSelection = 0;
        ImGui::BeginListBox("##Listbox", ImVec2(-FLT_MIN, 9 * ImGui::GetTextLineHeightWithSpacing()));
        for (int i = 0; i < IM_ARRAYSIZE(items); i++) {
            const bool isSelected = (currentSelection == i);
            if (ImGui::Selectable(items[i], isSelected))
                currentSelection = i;

            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndListBox();
        ImGui::End();

        ImGui::Begin("ResourceTypeOverview", nullptr);

        switch(currentSelection) {
            case 0:
                RenderResourceType<Audio::AudioData>(IconType::Audio);
                break;
            case 1:
                RenderResourceType<Mesh::Mesh>(IconType::Mesh);
                break;
            case 3:
                RenderResourceType<Scene::Scene>(IconType::Scene);
            default:
                break;
        }

        ImGui::End();

    }

}