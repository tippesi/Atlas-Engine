#include "SceneWindow.h"
#include "../../Singletons.h"

#include "scene/SceneSerializer.h"
#include "Clock.h"

#include <imgui_internal.h>
#include <ImGuizmo.h>

namespace Atlas::Editor::UI {

    SceneWindow::SceneWindow(ResourceHandle<Scene::Scene> scene, bool show) :
        Window(scene.IsLoaded() ? scene->name : "No scene", show), scene(scene) {
    
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

        auto& camera = cameraEntity.GetComponent<CameraComponent>();
        camera.aspectRatio = float(viewportPanel.viewport->width) / float(viewportPanel.viewport->height);

        // If we're playing we can update here since we don't expect values to change from the UI side
        if (isPlaying) {
            scene->Timestep(deltaTime);
            scene->Update();
        }

    }

    void SceneWindow::Render() {

        if (!Begin())
            return;

        ImGuiID dsID = ImGui::GetID(dockSpaceNameID.c_str());

        if (!ImGui::DockBuilderGetNode(dsID) || resetDockingLayout) {
            ImGui::SetWindowSize(ImVec2(640, 480), ImGuiCond_FirstUseEver);

            auto viewport = ImGui::GetWindowViewport();
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

        ImGui::DockSpace(dsID, ImVec2(0.0f, 0.0f), 0);

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
            else if (sceneHierarchyPanel.selectedProperty.ssgi)
                scenePropertiesPanel.Render(scene->ssgi);
            else if (sceneHierarchyPanel.selectedProperty.sss)
                scenePropertiesPanel.Render(scene->sss);
            else if (sceneHierarchyPanel.selectedProperty.postProcessing)
                scenePropertiesPanel.Render(scene->postProcessing);
            else
                scenePropertiesPanel.Render(sceneHierarchyPanel.selectedEntity, refScene);
        }
        else {
            // Render with invalid entity and invalid scene (will just return, but with window created)
            scenePropertiesPanel.Render(sceneHierarchyPanel.selectedEntity, refScene);
        }

        // We want to update the scene after all panels have update their respective values/changed the scene
        if (!isPlaying) {
            // Temporarily disable all scene cameras, only let editor camera be main
            std::map<ECS::Entity, bool> cameraMainMap;
            auto cameraSubset = scene->GetSubset<CameraComponent>();
            for (auto entity : cameraSubset) {
                if (entity == cameraEntity)
                    continue;

                auto& comp = cameraSubset.Get(entity);
                cameraMainMap[entity] = comp.isMain;
                comp.isMain = false;
            }

            scene->Timestep(Clock::GetDelta());
            scene->Update();

            // Restore all previous camera main values
            for (auto entity : cameraSubset) {
                if (entity == cameraEntity)
                    continue;

                auto& comp = cameraSubset.Get(entity);
                comp.isMain = cameraMainMap[entity];
            }
        }

        RenderEntityBoundingVolumes(sceneHierarchyPanel.selectedEntity);

        viewportPanel.Render(refScene, inFocus);

        End();

    }

    void SceneWindow::RegisterViewportAndGizmoOverlay() {

        viewportPanel.DrawMenuBar([&]() {

            const float padding = 8.0f;

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
                    
                    const auto& comp = cameraSubset.Get(entity);
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

            auto offset = region.x / 2.0f - buttonSize.x - padding;
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

            offset = region.x / 2.0f + padding;
            ImGui::SetCursorPos(ImVec2(offset, 0.0f));
            if (ImGui::ImageButton(set, buttonSize, uvMin, uvMax) && scene.IsLoaded() && isPlaying) {
                // Set camera to main in any case
                auto& camera = cameraEntity.GetComponent<CameraComponent>();
                camera.isMain = true;  

                scene->physicsWorld->RestoreState();
                scene->physicsWorld->pauseSimulation = true;

                isPlaying = false;
            }

            auto& settingsIcon = Singletons::icons->Get(IconType::Settings);
            set = Singletons::imguiWrapper->GetTextureDescriptorSet(settingsIcon);

            uvMin = ImVec2(0.1f, 0.1f);
            uvMax = ImVec2(0.9f, 0.9f);

            ImGui::SetCursorPos(ImVec2(region.x - buttonSize.x - padding, 0.0f));
            if (!isPlaying && ImGui::ImageButton(set, buttonSize, uvMin, uvMax) && scene.IsLoaded()) {
                
            }

            ImGui::PopStyleColor();

            });

        viewportPanel.DrawOverlay([&]() {
            needGuizmoEnabled = false;
            auto selectedEntity = sceneHierarchyPanel.selectedEntity;

            if (cameraEntity.IsValid() && inFocus && !isPlaying) {

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

                ImGuizmo::SetRect(viewport->x, viewport->y, viewport->width, viewport->height);

                if (selectedEntity.IsValid() && selectedEntity.HasComponent<TransformComponent>()) {
                    auto parentEntity = scene->GetParentEntity(selectedEntity);
                    TransformComponent* parentTransform = nullptr;
                    if (parentEntity.IsValid())
                        parentTransform = parentEntity.TryGetComponent<TransformComponent>();

                    auto& transform = selectedEntity.GetComponent<TransformComponent>();

                    auto globalMatrix = parentTransform ? parentTransform->globalMatrix 
                        * transform.matrix : transform.matrix;

                    ImGuizmo::Manipulate(glm::value_ptr(vMatrix), glm::value_ptr(pMatrix),
                        static_cast<ImGuizmo::OPERATION>(guizmoMode), ImGuizmo::MODE::WORLD,
                        glm::value_ptr(globalMatrix));

                    if (parentTransform) {
                        auto inverseParentTransform = glm::inverse(parentTransform->globalMatrix);
                        auto localMatrix = inverseParentTransform * globalMatrix;
                        transform.Set(localMatrix);
                    }
                    else {
                        transform.Set(globalMatrix);
                    }
                }

                const auto& io = ImGui::GetIO();

                auto mousePos = vec2(io.MousePos.x, io.MousePos.y);
                auto windowPos = ImGui::GetWindowPos();

                bool inViewport = mousePos.x > float(viewport->x) && mousePos.y > float(viewport->y)
                    && mousePos.x < float(viewport->width) && mousePos.y < float(viewport->height);
                if (io.MouseDown[ImGuiMouseButton_Right] && inViewport) {

                    auto nearPoint = viewport->Unproject(vec3(mousePos, 0.0f), camera);
                    auto farPoint = viewport->Unproject(vec3(mousePos, 1.0f), camera);

                    Atlas::Volume::Ray ray(camera.GetLocation(), glm::normalize(farPoint - nearPoint));

                    auto rayCastResult = scene->CastRay(ray);
                    if (rayCastResult.valid) {
                        sceneHierarchyPanel.selectedEntity = rayCastResult.data;
                    }
                }

                /*
                const float gridSize = 10.0f;
                auto gridMatrix = mat4(1.0f);
                ImGuizmo::DrawGrid(glm::value_ptr(vMatrix), glm::value_ptr(pMatrix),
                    glm::value_ptr(gridMatrix), gridSize);
                */
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
        if (entity.HasComponent<AudioComponent>()) {
            const auto& audioComponent = entity.GetComponent<AudioComponent>();
            const auto transformComponent = entity.TryGetComponent<TransformComponent>();

            vec3 position = vec3(0.0f);
            if (transformComponent)
                position += transformComponent->Decompose().translation;

            // After this the audio will be cutoff
            float radius = powf(audioComponent.falloffFactor / audioComponent.cutoff, 1.0f / audioComponent.falloffPower);
            viewportPanel.primitiveBatchWrapper.RenderLineSphere(position, radius, vec3(0.0f, 1.0f, 0.0f));
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
        if (entity.HasComponent<LightComponent>()) {
            const auto& lightComponent = entity.GetComponent<LightComponent>();
            if (lightComponent.shadow) {
                for (const auto& component : lightComponent.shadow->views)
                    viewportPanel.primitiveBatchWrapper.RenderLineFrustum(
                        Volume::Frustum(component.frustumMatrix), vec3(1.0f, 0.0f, 0.0f));
            }
        }
        if (entity.HasComponent<TextComponent>()) {
            const auto& textComponent = entity.GetComponent<TextComponent>();
            auto rectangle = textComponent.GetRectangle();
            viewportPanel.primitiveBatchWrapper.RenderLineRectangle(rectangle,vec3(0.0f, 0.0f, 1.0f));
        }

    }

}