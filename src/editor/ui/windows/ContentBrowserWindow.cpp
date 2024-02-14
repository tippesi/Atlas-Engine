#include <imgui_internal.h>
#include "ContentBrowserWindow.h"

#include "mesh/Mesh.h"
#include "scene/Scene.h"
#include "audio/AudioData.h"

namespace Atlas::Editor::UI {

    ContentBrowserWindow::ContentBrowserWindow(bool show) : Window("Object browser", show) {



    }

    void ContentBrowserWindow::Render() {

        if (!Begin())
            return;

        ImGuiID dsID = ImGui::GetID(dockSpaceNameID.c_str());
        auto viewport = ImGui::GetWindowViewport();

        if (!ImGui::DockBuilderGetNode(dsID) || resetDockingLayout) {
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

        ImGui::DockSpace(dsID, ImVec2(0.0f, 0.0f), 0);

        ImGui::End();

        ImGui::Begin("ResourceTypeSelection", nullptr);
        ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
            ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_Framed;

        const char *items[] = {"Audio", "Mesh", "Terrain", "Scene"};
        static int currentSelection = 0;
        for (int i = 0; i < IM_ARRAYSIZE(items); i++) {
            ImGui::TreeNodeEx(items[i], nodeFlags);
            if (ImGui::IsItemClicked())
                currentSelection = i;
        }

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

        End();

    }

}