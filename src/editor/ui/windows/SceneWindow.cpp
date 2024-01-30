#include "SceneWindow.h"

#include <imgui_internal.h>

namespace Atlas::Editor::UI {

    void SceneWindow::Update(float deltaTime) {

        if (!scene.IsLoaded())
            return;

        scene->Timestep(deltaTime);
        scene->Update();

    }

    void SceneWindow::Render() {

        ImGui::Begin(GetNameID());

        ImGuiID dsID = ImGui::GetID(dockSpaceNameID.c_str());
        ImGui::DockSpace(dsID, ImVec2(0.0f, 0.0f), 0);

        auto viewport = ImGui::GetWindowViewport();

        if (resetDockingLayout) {
            ImGui::DockBuilderRemoveNode(dsID);
            ImGui::DockBuilderAddNode(dsID, ImGuiDockNodeFlags_DockSpace);

            ImGui::DockBuilderSetNodeSize(dsID, viewport->Size);

            uint32_t dockIdLeft, dockIdMiddle, dockIdRight;
            ImGui::DockBuilderSplitNode(dsID, ImGuiDir_Left, 0.15f, &dockIdLeft, &dockIdMiddle);
            ImGui::DockBuilderSplitNode(dockIdMiddle, ImGuiDir_Left, 0.8f, &dockIdMiddle, &dockIdRight);

            ImGuiDockNode *leftNode = ImGui::DockBuilderGetNode(dockIdLeft);
            ImGuiDockNode *middleNode = ImGui::DockBuilderGetNode(dockIdMiddle);
            ImGuiDockNode *rightNode = ImGui::DockBuilderGetNode(dockIdRight);
            leftNode->LocalFlags |= ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_NoDockingOverMe;
            middleNode->LocalFlags |= ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_NoDockingOverMe;
            rightNode->LocalFlags |= ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_NoDockingOverMe;

            // we now dock our windows into the docking node we made above
            ImGui::DockBuilderDockWindow(sceneHierarchyPanel.GetNameID(), dockIdLeft);
            ImGui::DockBuilderDockWindow(viewportPanel.GetNameID(), dockIdMiddle);
            ImGui::DockBuilderDockWindow(entityPropertiesPanel.GetNameID(), dockIdRight);
            ImGui::DockBuilderFinish(dsID);

            resetDockingLayout = false;
        }

        ImGui::End();

        Ref<Scene::Scene> refScene = scene.IsLoaded() ? scene.Get() : nullptr;

        if (refScene != nullptr && !cameraEntity.IsValid()) {
            cameraEntity = scene->CreateEntity();
            auto& camera = cameraEntity.AddComponent<CameraComponent>(47.0f, 2.0f, 1.0f, 400.0f,
                glm::vec3(30.0f, 25.0f, 0.0f), glm::vec2(-3.14f / 2.0f, 0.0f));
            camera.isMain = true;
        }

        sceneHierarchyPanel.Render(refScene);
        viewportPanel.Render(refScene);
        entityPropertiesPanel.Render({ ECS::EntityConfig::InvalidEntity, nullptr});

    }

}