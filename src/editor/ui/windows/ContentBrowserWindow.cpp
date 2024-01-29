#include <imgui_internal.h>
#include "ContentBrowserWindow.h"

namespace Atlas::Editor::UI {

    void ContentBrowserWindow::Render() {

        ImGui::Begin(GetNameID());

        ImGuiID dsID = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dsID, ImVec2(0.0f, 0.0f), 0);

        auto viewport = ImGui::GetWindowViewport();

        static auto first_time = true;
        if (first_time) {
            first_time = false;

            ImGui::DockBuilderRemoveNode(dsID);
            ImGui::DockBuilderAddNode(dsID, ImGuiDockNodeFlags_DockSpace);

            ImGui::DockBuilderSetNodeSize(dsID, viewport->Size);

            uint32_t dockIdLeft, dockIdRight;
            ImGui::DockBuilderSplitNode(dsID, ImGuiDir_Left, 0.2f, &dockIdLeft, &dockIdRight);

            ImGuiDockNode* leftNode = ImGui::DockBuilderGetNode(dockIdLeft);
            ImGuiDockNode* rightNode = ImGui::DockBuilderGetNode(dockIdRight);
            leftNode->LocalFlags |= ImGuiDockNodeFlags_NoTabBar  | ImGuiDockNodeFlags_NoDockingOverMe;
            rightNode->LocalFlags |= ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_NoDockingOverMe;

            // we now dock our windows into the docking node we made above
            ImGui::DockBuilderDockWindow("Left", dockIdLeft);
            ImGui::DockBuilderDockWindow("Right", dockIdRight);
            ImGui::DockBuilderFinish(dsID);
        }

        ImGui::End();

        ImGui::Begin("Left", nullptr);
        ImGui::Text("Hello, left!");
        const char* items[] = { "Apple", "Banana", "Cherry", "Kiwi", "Mango", "Orange", "Pineapple", "Strawberry", "Watermelon" };
        static int item_current = 1;
        ImGui::ListBox("listbox", &item_current, items, IM_ARRAYSIZE(items), 4);
        ImGui::End();

        ImGui::Begin("Right", nullptr);
        ImGui::Text("Hello, down!");
        ImGui::End();

    }

}