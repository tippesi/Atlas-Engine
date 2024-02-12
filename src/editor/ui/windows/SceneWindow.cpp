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
            auto& camera = cameraEntity.AddComponent<CameraComponent>(47.0f, 2.0f, 1.0f, 2000.0f);
            camera.isMain = true;
        }

        // Temporarily disable all scene cameras, only let editor camera be main
        std::map<ECS::Entity, bool> cameraMainMap;
        auto cameraSubset = scene->GetSubset<CameraComponent>();
        for (auto entity : cameraSubset) {
            if (entity == cameraEntity || isPlaying)
                continue;
            
            auto& comp = cameraSubset.Get(entity);
            cameraMainMap[entity] = comp.isMain;
            comp.isMain = false;
        }

        scene->Timestep(deltaTime);
        scene->Update();

        // Restore all previous camera main values
        for (auto entity : cameraSubset) {
            if (entity == cameraEntity || isPlaying)
                continue;
            
            auto& comp = cameraSubset.Get(entity);
            comp.isMain = cameraMainMap[entity];
        }

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

            // we now dock our windows into the docking node we made above
            ImGui::DockBuilderDockWindow(sceneHierarchyPanel.GetNameID(), dockIdLeft);
            ImGui::DockBuilderDockWindow(viewportPanel.GetNameID(), dockIdMiddle);
            ImGui::DockBuilderDockWindow(scenePropertiesPanel.GetNameID(), dockIdRight);
            ImGui::DockBuilderFinish(dsID);

            resetDockingLayout = false;
        }

        // Due to docking it doesn't register child windows as focused as well, need to check in child itself
        inFocus = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows | ImGuiFocusedFlags_RootWindow) ||
            sceneHierarchyPanel.isFocused || scenePropertiesPanel.isFocused || viewportPanel.isFocused;

        Ref<Scene::Scene> refScene = scene.IsLoaded() ? scene.Get() : nullptr;

        sceneHierarchyPanel.Render(refScene, inFocus);

        // Depending on the selection in the scene hierarchy panel, render the properties in a different window
        if (scene.IsLoaded()) {
            if (sceneHierarchyPanel.selectedProperty.fog)
                scenePropertiesPanel.Render(scene->fog);
            else if (sceneHierarchyPanel.selectedProperty.volumetricClouds)
                scenePropertiesPanel.Render(scene->sky.clouds);
            else if (sceneHierarchyPanel.selectedProperty.irradianceVolume)
                scenePropertiesPanel.Render(scene->irradianceVolume);
            else if (sceneHierarchyPanel.selectedProperty.reflection)
                scenePropertiesPanel.Render(scene->reflection);
            else if (sceneHierarchyPanel.selectedProperty.postProcessing)
                scenePropertiesPanel.Render(scene->postProcessing);
            else
                scenePropertiesPanel.Render(sceneHierarchyPanel.selectedEntity, refScene);
        }
        else {
            // Render with invalid entity and invalid scene (will just return, but with window created)
            scenePropertiesPanel.Render(sceneHierarchyPanel.selectedEntity, refScene);
        }

        RenderEntityBoundingVolumes(sceneHierarchyPanel.selectedEntity);

        viewportPanel.Render(refScene, inFocus);

        ImGui::End();

    }

    void SceneWindow::RegisterViewportAndGizmoOverlay() {

        viewportPanel.DrawMenuBar([&]() {

            auto height = ImGui::GetTextLineHeight();

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

            hasMainCamera = false;
            if (scene.IsLoaded()) {
                auto cameraSubset = scene->GetSubset<CameraComponent>();
                for (auto entity : cameraSubset) {
                    if (entity == cameraEntity)
                        continue;
                    
                    auto& comp = cameraSubset.Get(entity);
                    hasMainCamera |= comp.isMain;
                }
            }

            hasPlayer = false;
            Scene::Entity playerEntity;
            if (scene.IsLoaded()) {
                auto playerSubset = scene->GetSubset<PlayerComponent>();
                for (const auto entity : playerSubset) {
                    hasPlayer = true;
                    playerEntity = entity;
                }
            }

            auto offset = region.x / 2.0f - buttonSize.x - 8.0f;
            ImGui::SetCursorPos(ImVec2(offset, 0.0f));
            if (ImGui::ImageButton(set, buttonSize, uvMin, uvMax) && scene.IsLoaded()) {
                if (hasMainCamera) {
                    auto& camera = cameraEntity.GetComponent<CameraComponent>();
                    camera.isMain = false;
                }
                
                scene->physicsWorld->SaveState();
                scene->physicsWorld->pauseSimulation = false;
                // Unselect when starting the simulation/scene (otherwise some physics settings might not
                // be reverted after stopping
                sceneHierarchyPanel.selectedEntity = Scene::Entity();

                isPlaying = true;
            }

            auto& stopIcon = Singletons::icons->Get(IconType::Stop);
            set = Singletons::imguiWrapper->GetTextureDescriptorSet(stopIcon);

            offset = region.x / 2.0f + 8.0f;
            ImGui::SetCursorPos(ImVec2(offset, 0.0f));
            if (ImGui::ImageButton(set, buttonSize, uvMin, uvMax) && scene.IsLoaded() && isPlaying) {
                // Set camera to main in any case
                auto& camera = cameraEntity.GetComponent<CameraComponent>();
                camera.isMain = true;  

                scene->physicsWorld->RestoreState();
                scene->physicsWorld->pauseSimulation = true;

                isPlaying = false;
            }

            ImGui::PopStyleColor();

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

                const auto& camera = cameraEntity.GetComponent<CameraComponent>();

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
                    const auto& parentTransform = parentEntity.GetComponent<TransformComponent>();
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

    void SceneWindow::RenderEntityBoundingVolumes(Scene::Entity entity) {

        if (!entity.IsValid())
            return;

        if (entity.HasComponent<MeshComponent>()) {
            const auto& meshComponent = entity.GetComponent<MeshComponent>();
            auto aabb = meshComponent.aabb;
            viewportPanel.primitiveBatchWrapper.RenderLineAABB(aabb, vec3(1.0f, 1.0f, 0.0f));
        }
        if (entity.HasComponent<AudioVolumeComponent>()) {
            const auto& audioVolumeComponent = entity.GetComponent<AudioVolumeComponent>();
            auto aabb = audioVolumeComponent.GetTransformedAABB();
            viewportPanel.primitiveBatchWrapper.RenderLineAABB(aabb, vec3(0.0f, 1.0f, 0.0f));
        }
        if (entity.HasComponent<CameraComponent>()) {
            const auto& cameraComponent = entity.GetComponent<CameraComponent>();
            viewportPanel.primitiveBatchWrapper.RenderLineFrustum(cameraComponent.frustum, vec3(1.0f, 0.0f, 1.0f));
        }

    }

}