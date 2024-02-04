#include "SceneWindow.h"

#include "scene/SceneSerializer.h"

#include <imgui_internal.h>
#include <ImGuizmo.h>

namespace Atlas::Editor::UI {

    SceneWindow::SceneWindow(ResourceHandle<Scene::Scene> scene) :
        Window(scene.IsLoaded() ? scene->name : "No scene"), scene(scene) {
    
        RegisterViewportAndGizmoOverlay();

    }

    SceneWindow::~SceneWindow() {

        if (!scene.IsLoaded())
            return;

        scene->DestroyEntity(cameraEntity);

        Scene::SceneSerializer::SerializeScene(scene.Get(), "scenes/" + std::string(GetNameID()) + ".aescene");

    }

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

        Ref<Scene::Scene> refScene = scene.IsLoaded() ? scene.Get() : nullptr;

        if (refScene != nullptr && !cameraEntity.IsValid()) {
            cameraEntity = scene->CreateEntity();
            auto& camera = cameraEntity.AddComponent<CameraComponent>(47.0f, 2.0f, 1.0f, 400.0f,
                glm::vec3(30.0f, 25.0f, 0.0f), glm::vec2(-3.14f / 2.0f, 0.0f));
            camera.isMain = true;
        }

        // Due to docking it doesn't register child windows as focused as well, need to check in child itself
        inFocus = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows | ImGuiFocusedFlags_RootWindow) ||
            sceneHierarchyPanel.isFocused || entityPropertiesPanel.isFocused || viewportPanel.isFocused;

        sceneHierarchyPanel.Render(refScene);
        entityPropertiesPanel.Render(sceneHierarchyPanel.selectedEntity);
        viewportPanel.Render(refScene, inFocus);

        ImGui::End();

    }

    void SceneWindow::RegisterViewportAndGizmoOverlay() {

        viewportPanel.DrawOverlay([&]() {
            if (ImGui::RadioButton("T", &guizmoMode, ImGuizmo::OPERATION::TRANSLATE)) {
                guizmoMode = ImGuizmo::OPERATION::TRANSLATE;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("R", &guizmoMode, ImGuizmo::OPERATION::ROTATE)) {
                guizmoMode = ImGuizmo::OPERATION::ROTATE;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("S", &guizmoMode, ImGuizmo::OPERATION::SCALE)) {
                guizmoMode = ImGuizmo::OPERATION::SCALE;
            }

            needGuizmoEnabled = false;
            auto selectedEntity = sceneHierarchyPanel.selectedEntity;

            if (cameraEntity.IsValid() && selectedEntity.IsValid() && 
                selectedEntity.HasComponent<TransformComponent>() && inFocus) {

                needGuizmoEnabled = true;
                ImGuizmo::SetDrawlist();

                const mat4 clip = mat4(1.0f, 0.0f, 0.0f, 0.0f,
                    0.0f, -1.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 0.5f, 0.0f,
                    0.0f, 0.0f, 0.5f, 1.0f);
                const mat4 inverseClip = glm::inverse(clip);

                auto& camera = cameraEntity.GetComponent<CameraComponent>();

                auto vMatrix = camera.viewMatrix;
                auto pMatrix = inverseClip * camera.unjitterdProjection;

                auto viewport = viewportPanel.viewport;

                auto& transform = selectedEntity.GetComponent<TransformComponent>();
                ImGuizmo::SetRect(viewport->x, viewport->y, viewport->width, viewport->height);
                ImGuizmo::Manipulate(glm::value_ptr(vMatrix), glm::value_ptr(pMatrix),
                    static_cast<ImGuizmo::OPERATION>(guizmoMode), ImGuizmo::MODE::LOCAL, 
                    glm::value_ptr(transform.matrix));
                transform.Set(transform.matrix);
            }
            });
           
    }

}