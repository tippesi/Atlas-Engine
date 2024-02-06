#include "SceneWindow.h"
#include "../../Singletons.h"

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

        Scene::SceneSerializer::SerializeScene(scene.Get(), "scenes/" + std::string(scene->name) + ".aescene");

    }

    void SceneWindow::Update(float deltaTime) {

        if (!scene.IsLoaded())
            return;

        if (!cameraEntity.IsValid()) {
            cameraEntity = scene->CreateEntity();
            auto& camera = cameraEntity.AddComponent<CameraComponent>(47.0f, 2.0f, 1.0f, 400.0f,
                glm::vec3(30.0f, 25.0f, 0.0f), glm::vec2(-3.14f / 2.0f, 0.0f));
            camera.isMain = true;
        }

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

            /*
            // This locks it all into a fixed window layout
            ImGuiDockNode *leftNode = ImGui::DockBuilderGetNode(dockIdLeft);
            ImGuiDockNode *middleNode = ImGui::DockBuilderGetNode(dockIdMiddle);
            ImGuiDockNode *rightNode = ImGui::DockBuilderGetNode(dockIdRight);
            leftNode->LocalFlags |= ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_NoDockingOverMe;
            middleNode->LocalFlags |= ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_NoDockingOverMe;
            rightNode->LocalFlags |= ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_NoDockingOverMe;
            */

            // we now dock our windows into the docking node we made above
            ImGui::DockBuilderDockWindow(sceneHierarchyPanel.GetNameID(), dockIdLeft);
            ImGui::DockBuilderDockWindow(viewportPanel.GetNameID(), dockIdMiddle);
            ImGui::DockBuilderDockWindow(entityPropertiesPanel.GetNameID(), dockIdRight);
            ImGui::DockBuilderFinish(dsID);

            resetDockingLayout = false;
        }

        // Due to docking it doesn't register child windows as focused as well, need to check in child itself
        inFocus = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows | ImGuiFocusedFlags_RootWindow) ||
            sceneHierarchyPanel.isFocused || entityPropertiesPanel.isFocused || viewportPanel.isFocused;

        Ref<Scene::Scene> refScene = scene.IsLoaded() ? scene.Get() : nullptr;

        sceneHierarchyPanel.Render(refScene);
        entityPropertiesPanel.Render(sceneHierarchyPanel.selectedEntity);

        RenderEntityAABB(sceneHierarchyPanel.selectedEntity);

        viewportPanel.Render(refScene, inFocus);

        ImGui::End();

    }

    void SceneWindow::RegisterViewportAndGizmoOverlay() {

        viewportPanel.DrawMenuBar([&]() {

            auto height = ImGui::GetTextLineHeight();

            ImGui::BeginChild("Menu bar area", ImVec2(0.0f, height + 8.0f));

            ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));

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

            auto region = ImGui::GetContentRegionAvail();
            auto buttonSize = ImVec2(height, height);
            auto uvMin = ImVec2(0.25, 0.25);
            auto uvMax = ImVec2(0.75, 0.75);

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

            auto& playIcon = Singletons::icons->Get(IconType::Play);
            auto set = Singletons::imguiWrapper->GetTextureDescriptorSet(playIcon);

            auto offset = region.x / 2.0f - buttonSize.x - 8.0f;
            ImGui::SetCursorPos(ImVec2(offset, 0.0f));
            if (ImGui::ImageButton(set, buttonSize, uvMin, uvMax)) {

            }

            auto& stopIcon = Singletons::icons->Get(IconType::Stop);
            set = Singletons::imguiWrapper->GetTextureDescriptorSet(stopIcon);

            offset = region.x / 2.0f + 8.0f;
            ImGui::SetCursorPos(ImVec2(offset, 0.0f));
            if (ImGui::ImageButton(set, buttonSize, uvMin, uvMax)) {

            }

            ImGui::PopStyleColor();

            ImGui::EndChild();

            });

        viewportPanel.DrawOverlay([&]() {
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

                auto parentEntity = scene->GetParentEntity(selectedEntity);

                auto& transform = selectedEntity.GetComponent<TransformComponent>();
                ImGuizmo::SetRect(viewport->x, viewport->y, viewport->width, viewport->height);
                ImGuizmo::Manipulate(glm::value_ptr(vMatrix), glm::value_ptr(pMatrix),
                    static_cast<ImGuizmo::OPERATION>(guizmoMode), ImGuizmo::MODE::WORLD,
                    glm::value_ptr(transform.globalMatrix));


                if (parentEntity.IsValid() && parentEntity.HasComponent<TransformComponent>()) {
                    auto& parentTransform = parentEntity.GetComponent<TransformComponent>();
                    auto inverseParentTransform = glm::inverse(parentTransform.globalMatrix);
                    auto localMatrix = inverseParentTransform * transform.globalMatrix;
                    transform.Set(localMatrix);
                }
                else {
                    transform.Set(transform.globalMatrix);
                }

                
            }
            });
           
    }

    void SceneWindow::RenderEntityAABB(Scene::Entity entity) {

        if (!entity.IsValid())
            return;

        bool foundAABB = false;
        Volume::AABB aabb;

        if (entity.HasComponent<MeshComponent>()) {
            auto meshComponent = entity.GetComponent<MeshComponent>();
            aabb = meshComponent.aabb;
            foundAABB = true;
        }

        if (foundAABB) {
            viewportPanel.primitiveBatchWrapper.RenderLineAABB(aabb, vec3(1.0f, 1.0f, 0.0f));
        }

    }

}