#include "SceneWindow.h"
#include "../../Singletons.h"
#include "../../Notifications.h"
#include "../../tools/ResourcePayloadHelper.h"

#include "Serializer.h"
#include "common/Hash.h"
#include "Clock.h"

#include <imgui_internal.h>
#include <ImGuizmo.h>

namespace Atlas::Editor::UI {

    SceneWindow::SceneWindow(ResourceHandle<Scene::Scene> scene, bool show) :
        Window(scene.IsLoaded() ? scene->name : "No scene", show), scene(scene) {
    
        RegisterViewportAndGizmoOverlay();

        // Overwrite the window name, there should only be one scene with the same name
        nameID = scene->name;

    }

    SceneWindow::~SceneWindow() {

        

    }

    void SceneWindow::Update(float deltaTime) {

        if (!scene.IsLoaded() || !show)
            return;

        if (!cameraEntity.IsValid()) {
            cameraEntity = scene->CreateEntity();
            auto& camera = cameraEntity.AddComponent<CameraComponent>(47.0f, 2.0f, 1.0f, 2000.0f);
            camera.isMain = true;
        }

        sceneHierarchyPanel.Update(scene.Get());

        // If we're playing we can update here since we don't expect values to change from the UI side
        if (isPlaying) {
            scene->Timestep(deltaTime);
            scene->Update();
        }
        else {
            auto& camera = cameraEntity.GetComponent<CameraComponent>();
            camera.aspectRatio = float(viewportPanel.viewport->width) / std::max(float(viewportPanel.viewport->height), 1.0f);
        }

        bool controlDown;        
#ifdef AE_OS_MACOS
        controlDown = ImGui::IsKeyDown(ImGuiKey_LeftSuper);
#else
        controlDown = ImGui::IsKeyDown(ImGuiKey_LeftCtrl);
#endif
        if (inFocus && controlDown && ImGui::IsKeyPressed(ImGuiKey_S, false) && !isPlaying) {
            SaveScene();
        }

    }

    void SceneWindow::Render() {

        if (!Begin())
            return;

        if (ImGui::IsDragDropActive() && ImGui::IsWindowHovered()) {
            ImGui::SetWindowFocus();
        }

        bool isBlocked = Singletons::blockingOperation->block;

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
        inFocus = ImGui::IsWindowFocused() ||
            sceneHierarchyPanel.isFocused || scenePropertiesPanel.isFocused || viewportPanel.isFocused;

        Ref<Scene::Scene> refScene = scene.IsLoaded() ? scene.Get() : nullptr;

        // Depending on the selection in the scene hierarchy panel, render the properties in a different window
        if (scene.IsLoaded()) {
            const auto& target = Singletons::renderTarget;

            EntityProperties entityProperties = {
                .entity = sceneHierarchyPanel.selectedEntity,
                .editorCameraEntity = cameraEntity
            };

            if (sceneHierarchyPanel.selectedProperty.fog)
                scenePropertiesPanel.Render(scene->fog, nullptr, target);
            else if (sceneHierarchyPanel.selectedProperty.volumetricClouds)
                scenePropertiesPanel.Render(scene->sky.clouds, nullptr, target);
            else if (sceneHierarchyPanel.selectedProperty.irradianceVolume)
                scenePropertiesPanel.Render(scene->irradianceVolume, refScene);
            else if (sceneHierarchyPanel.selectedProperty.rtgi)
                scenePropertiesPanel.Render(scene->rtgi, refScene);
            else if (sceneHierarchyPanel.selectedProperty.reflection)
                scenePropertiesPanel.Render(scene->reflection, nullptr, target);
            else if (sceneHierarchyPanel.selectedProperty.ssgi)
                scenePropertiesPanel.Render(scene->ssgi, nullptr, target);
            else if (sceneHierarchyPanel.selectedProperty.sss)
                scenePropertiesPanel.Render(scene->sss);
            else if (sceneHierarchyPanel.selectedProperty.wind)
                scenePropertiesPanel.Render(scene->wind);
            else if (sceneHierarchyPanel.selectedProperty.sky)
                scenePropertiesPanel.Render(scene->sky);
            else if (sceneHierarchyPanel.selectedProperty.postProcessing)
                scenePropertiesPanel.Render(scene->postProcessing);
            else if (sceneHierarchyPanel.selectedEntity.IsValid())
                scenePropertiesPanel.Render(entityProperties, refScene);
            else
                scenePropertiesPanel.Render(refScene);
        }
        else {
            // Render with invalid entity and invalid scene (will just return, but with window created)
            scenePropertiesPanel.Render(sceneHierarchyPanel.selectedEntity, refScene);
        }

        // We want to update the scene after all panels have update their respective values/changed the scene
        if (!isPlaying && !isBlocked) {
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

            // Path tracing needs history while ray tracing
            scene->rayTracingWorld->includeObjectHistory = Singletons::config->pathTrace;

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

        sceneHierarchyPanel.Render(refScene, inFocus);
        RenderEntityBoundingVolumes(sceneHierarchyPanel.selectedEntity);

        viewportPanel.Render(refScene, isActiveWindow);

        auto path = ResourcePayloadHelper::AcceptDropResourceAndGetPath<Scene::Entity>();
        if (!path.empty()) {
            auto entity = Serializer::DeserializePrefab(scene.Get(), path);
            PushEntityIntoSceneHierarchy(entity);
        }

        End();

    }

    void SceneWindow::RegisterViewportAndGizmoOverlay() {

        viewportPanel.DrawMenuBar([&]() {

            const float padding = 8.0f;

            auto height = ImGui::GetTextLineHeight();
            auto region = ImGui::GetContentRegionAvail();
            auto buttonSize = ImVec2(height, height);

            ImVec4 selectedColor = ImGui::GetStyleColorVec4(ImGuiCol_Button);

            auto barBackgroundColor = Singletons::config->darkMode ? IM_COL32(0, 0, 0, 75) : IM_COL32(255, 255, 255, 75);

            auto windowPos = ImGui::GetWindowPos();
            auto drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(windowPos, ImVec2(region.x + windowPos.x, height + windowPos.y + padding), barBackgroundColor);

            ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));

            auto uvMin = ImVec2(0.15f, 0.15f);
            auto uvMax = ImVec2(0.85f, 0.85f);

            if (!isPlaying) {
                auto& moveIcon = Singletons::icons->Get(IconType::Move);
                auto set = Singletons::imguiWrapper->GetTextureDescriptorSet(&moveIcon);
                bool selected = guizmoMode == ImGuizmo::OPERATION::TRANSLATE;
                ImVec4 backgroundColor = selected ? selectedColor : ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
                ImGui::PushStyleColor(ImGuiCol_Button, backgroundColor);
                if (ImGui::ImageButton(set, buttonSize, uvMin, uvMax)) {
                    guizmoMode = ImGuizmo::OPERATION::TRANSLATE;
                }
                ImGui::SetItemTooltip("Sets the gizmo into translation mode");
                ImGui::PopStyleColor();

                ImGui::SameLine();
                auto& rotateIcon = Singletons::icons->Get(IconType::Rotate);
                set = Singletons::imguiWrapper->GetTextureDescriptorSet(&rotateIcon);
                selected = guizmoMode == ImGuizmo::OPERATION::ROTATE;
                backgroundColor = selected ? selectedColor : ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
                ImGui::PushStyleColor(ImGuiCol_Button, backgroundColor);
                if (ImGui::ImageButton(set, buttonSize, uvMin, uvMax)) {
                    guizmoMode = ImGuizmo::OPERATION::ROTATE;
                }
                ImGui::SetItemTooltip("Sets the gizmo into rotation mode");
                ImGui::PopStyleColor();

                ImGui::SameLine();
                auto& scaleIcon = Singletons::icons->Get(IconType::Scale);
                set = Singletons::imguiWrapper->GetTextureDescriptorSet(&scaleIcon);
                selected = guizmoMode == ImGuizmo::OPERATION::SCALE;
                backgroundColor = selected ? selectedColor : ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
                ImGui::PushStyleColor(ImGuiCol_Button, backgroundColor);
                if (ImGui::ImageButton(set, buttonSize, uvMin, uvMax)) {
                    guizmoMode = ImGuizmo::OPERATION::SCALE;
                }
                ImGui::SetItemTooltip("Sets the gizmo into scaling mode");
                ImGui::PopStyleColor();

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

                auto& moreHorizIcon = Singletons::icons->Get(IconType::MoreHorizontal);
                set = Singletons::imguiWrapper->GetTextureDescriptorSet(&moreHorizIcon);
                ImGui::SameLine();
                if (ImGui::ImageButton(set, buttonSize, uvMin, uvMax)) {
                    ImGui::OpenPopup("Guizmo settings");
                }

                if (ImGui::BeginPopup("Guizmo settings")) {
                    ImGui::Text("Snapping");

                    ImGui::Checkbox("Enabled", &snappingEnabled);
                    ImGui::DragFloat("Translation snap", &translationSnap, 0.01f, 0.1f, 100.0f);
                    ImGui::DragFloat("Rotation snap", &rotationSnap, 0.1f, 0.1f, 10.0f);
                    ImGui::DragFloat("Scale snap", &scaleSnap, 0.01f, 0.1f, 10.0f);

                    ImGui::Text("Bounding volumes");
                    ImGui::Checkbox("Test depth", &depthTestBoundingVolumes);

                    ImGui::EndPopup();
                }
            }
            else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            }

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

            uvMin = ImVec2(0.25f, 0.25f);
            uvMax = ImVec2(0.75f, 0.75f);

            auto& playIcon = Singletons::icons->Get(IconType::Play);
            auto set = Singletons::imguiWrapper->GetTextureDescriptorSet(&playIcon);

            auto offset = region.x / 2.0f - buttonSize.x - padding;
            ImGui::SetCursorPos(ImVec2(offset, 0.0f));
            if (ImGui::ImageButton(set, buttonSize, uvMin, uvMax) && scene.IsLoaded() && !isPlaying) {
                StartPlaying();
            }

            auto& stopIcon = Singletons::icons->Get(IconType::Stop);
            set = Singletons::imguiWrapper->GetTextureDescriptorSet(&stopIcon);

            offset = region.x / 2.0f + padding;
            ImGui::SetCursorPos(ImVec2(offset, 0.0f));
            if (ImGui::ImageButton(set, buttonSize, uvMin, uvMax) && scene.IsLoaded() && isPlaying) {
                StopPlaying();
            }

            auto& settingsIcon = Singletons::icons->Get(IconType::Settings);
            set = Singletons::imguiWrapper->GetTextureDescriptorSet(&settingsIcon);

            uvMin = ImVec2(0.1f, 0.1f);
            uvMax = ImVec2(0.9f, 0.9f);

            ImGui::SetCursorPos(ImVec2(region.x - 2.0f * (buttonSize.x + 2.0f * padding), 0.0f));
            if (!isPlaying && ImGui::ImageButton(set, buttonSize, uvMin, uvMax) && scene.IsLoaded()) {
                ImGui::OpenPopup("Viewport settings");
            }

            if (ImGui::BeginPopup("Viewport settings")) {
                ImGui::Text("Movement");

                ImGui::DragFloat("Movement speed", &cameraMovementSpeed, 0.1f, 0.1f, 100.0f);
                ImGui::DragFloat("Rotation speed", &cameraRotationSpeed, 0.1f, 0.1f, 10.0f);

                auto& camera = cameraEntity.GetComponent<CameraComponent>();

                ImGui::Text("Editor camera");

                ImGui::DragFloat("Exposure", &camera.exposure, 0.1f, 0.01f, 180.0f);
                ImGui::DragFloat("Field of view", &camera.fieldOfView, 0.1f, 1.0f, 180.0f);

                ImGui::DragFloat("Near plane", &camera.nearPlane, 0.01f, 0.01f, 10.0f);
                ImGui::DragFloat("Far plane", &camera.farPlane, 1.0f, 1.0f, 20000.0f);

                ImGui::Text("Rendering scale");

                ImGui::DragFloat("Resolution scale##Rendering", &resolutionScale, 0.01f, 0.1f, 1.0f);

                if (Singletons::renderTarget->GetScalingFactor() != resolutionScale)
                    Singletons::renderTarget->SetScalingFactor(resolutionScale);

                ImGui::Text("Path traces samples");
                ImGui::DragInt("Sample count", &Singletons::mainRenderer->pathTracingRenderer.realTimeSamplesPerFrame, 1, 1, 16);

                ImGui::EndPopup();
            }

            auto& eyeIcon = Singletons::icons->Get(IconType::Eye);
            set = Singletons::imguiWrapper->GetTextureDescriptorSet(&eyeIcon);

            ImGui::SetCursorPos(ImVec2(region.x - (buttonSize.x + 2.0f * padding), 0.0f));
            if (!isPlaying && ImGui::ImageButton(set, buttonSize, uvMin, uvMax) && scene.IsLoaded()) {
                ImGui::OpenPopup("Visualization settings");
            }

            auto menuItem = [&](const char* name, ViewportVisualization vis) {
                bool selected = viewportPanel.visualization == vis;
                ImGui::MenuItem(name, nullptr, &selected);
                if (selected)
                    viewportPanel.visualization = vis;
            };

            if (ImGui::BeginPopup("Visualization settings")) {
                ImGui::Text("Visualization");
                ImGui::Separator();

                menuItem("Lit", ViewportVisualization::Lit);

                if (ImGui::BeginMenu("GBuffer")) {
                    menuItem("Base color", ViewportVisualization::GBufferBaseColor);
                    menuItem("Roughness/Metalness/Ao", ViewportVisualization::GBufferRoughnessMetalnessAo);
                    menuItem("Depth", ViewportVisualization::GBufferDepth);
                    menuItem("Normals", ViewportVisualization::GBufferNormals);
                    menuItem("Geometry normals", ViewportVisualization::GBufferGeometryNormals);
                    menuItem("Velocity", ViewportVisualization::GBufferVelocity);
                    ImGui::EndMenu();
                }

                menuItem("Clouds", ViewportVisualization::Clouds);
                menuItem("Reflections", ViewportVisualization::Reflections);
                menuItem("SSS", ViewportVisualization::SSS);
                menuItem("SSGI", ViewportVisualization::SSGI);

                ImGui::EndPopup();
            }

            ImGui::PopStyleColor();

            });

        viewportPanel.DrawOverlay([&]() {
            needGuizmoEnabled = false;
            auto selectedEntity = sceneHierarchyPanel.selectedEntity;

            if (cameraEntity.IsValid() && isActiveWindow && !isPlaying) {

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
                    auto& transform = selectedEntity.GetComponent<TransformComponent>();
                    auto mesh = selectedEntity.TryGetComponent<MeshComponent>();

                    auto globalDecomp = Common::MatrixDecomposition(transform.globalMatrix);

                    glm::vec3 offset = vec3(0.0f);
                    if (mesh)
                        offset = (0.5f * (mesh->aabb.min + mesh->aabb.max)) - globalDecomp.translation;

                    globalDecomp.translation += offset;
                    auto globalMatrix = globalDecomp.Compose();

                    float* snappingPtr = nullptr;
                    glm::vec3 translation = vec3(translationSnap);
                    // Expects a 3-comp vector for translation
                    if (guizmoMode == ImGuizmo::OPERATION::TRANSLATE && snappingEnabled)
                        snappingPtr = glm::value_ptr(translation);
                    else if (guizmoMode == ImGuizmo::OPERATION::ROTATE && snappingEnabled)
                        snappingPtr = &rotationSnap;
                    else if (guizmoMode == ImGuizmo::OPERATION::SCALE && snappingEnabled)
                        snappingPtr = &scaleSnap;
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    auto size = drawList->VtxBuffer.Size;

                    bool manipulated = ImGuizmo::Manipulate(glm::value_ptr(vMatrix), glm::value_ptr(pMatrix),
                        static_cast<ImGuizmo::OPERATION>(guizmoMode), ImGuizmo::MODE::WORLD,
                        glm::value_ptr(globalMatrix), nullptr, snappingPtr);

                    // Only visible if something was drawn
                    bool visible = drawList->VtxBuffer.Size != size;

                    if (ImGuizmo::IsUsing()) {
                        globalDecomp = Common::MatrixDecomposition(globalMatrix);
                        globalDecomp.translation -= offset;

                        // Update both the local and global matrix, since e.g. the transform component
                        // panel recovers the local matrix from the global one (needs to do this after
                        // e.g. physics only updated the global matrix
                        transform.globalMatrix = globalDecomp.Compose();

                        auto parentEntity = scene->GetParentEntity(selectedEntity);
                        transform.ReconstructLocalMatrix(parentEntity);
                    }

                    // Need to disable here, otherwise we can't move around anymore
                    if (!visible) {
                        needGuizmoEnabled = false;
                    }
                }

                const auto& io = ImGui::GetIO();
                
                auto windowPos = ImGui::GetWindowPos();
                auto mousePos = vec2(io.MousePos.x, io.MousePos.y);

                bool inViewport = mousePos.x > float(viewport->x) 
                    && mousePos.y > float(viewport->y)
                    && mousePos.x < float(viewport->x + viewport->width)
                    && mousePos.y < float(viewport->y + viewport->height);

                if (io.MouseDown[ImGuiMouseButton_Right] && inViewport && !lockSelection) {

                    auto nearPoint = viewport->Unproject(vec3(mousePos, 0.0f), camera);
                    auto farPoint = viewport->Unproject(vec3(mousePos, 1.0f), camera);

                    Atlas::Volume::Ray ray(camera.GetLocation(), glm::normalize(farPoint - nearPoint));

                    auto rayCastResult = scene->CastRay(ray);
                    if (rayCastResult.valid) {
                        sceneHierarchyPanel.selectedEntity = rayCastResult.data;
                        sceneHierarchyPanel.selectedProperty = SelectedProperty();
                    }
                }
            }
            });
           
    }

    void SceneWindow::RenderEntityBoundingVolumes(Scene::Entity entity) {

        if (!entity.IsValid())
            return;

        viewportPanel.primitiveBatchWrapper.primitiveBatch->testDepth = depthTestBoundingVolumes;

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

    void SceneWindow::PushEntityIntoSceneHierarchy(Scene::Entity entity, bool changeSelection) {
        auto parentEntity = sceneHierarchyPanel.selectedEntity;

        if (!parentEntity.IsValid())
            parentEntity = scene->GetEntityByName("Root");

        if (!parentEntity.HasComponent<HierarchyComponent>())
            parentEntity.AddComponent<HierarchyComponent>();

        auto& parentHierarchy = parentEntity.GetComponent<HierarchyComponent>();
        parentHierarchy.AddChild(entity);

        // We don't know where this entity came from, so disable root hierarchy in any case to
        // avoid setting the matrices twice and getting unwanted movements
        auto hierarchy = entity.TryGetComponent<HierarchyComponent>();
        if (hierarchy)
            hierarchy->root = false;

        if (changeSelection) {
            sceneHierarchyPanel.selectedEntity = entity;
            sceneHierarchyPanel.selectedProperty = SelectedProperty();
        }
    }

    void SceneWindow::StartPlaying() {

        bool hasMainPlayingCamera = false;
        auto cameraSubset = scene->GetSubset<CameraComponent>();
        for (auto entity : cameraSubset) {
            if (entity == cameraEntity)
                continue;

            const auto& comp = cameraSubset.Get(entity);
            hasMainPlayingCamera |= comp.isMain;
        }

        if (!hasMainPlayingCamera) {
            Notifications::Push({ .message = "No main camera in scene. Please add one to start playing", .color = vec3(1.0f, 1.0f, 0.0f) });
            return;
        }

        SaveSceneState();

        scene->physicsWorld->pauseSimulation = false;
        // Unselect when starting the simulation/scene (otherwise some physics settings might not
        // be reverted after stopping
        sceneHierarchyPanel.selectedEntity = Scene::Entity();

        isPlaying = true;

    }

    void SceneWindow::StopPlaying() {

        RestoreSceneState(); 

        isPlaying = false;

    }

    void SceneWindow::SaveSceneState() {

        cameraState = Scene::Entity::Backup(scene.Get(), cameraEntity);

        scene->DestroyEntity(cameraEntity);

        sceneState = Scene::Scene::Backup(scene.Get());

    }

    void SceneWindow::RestoreSceneState() {

        if (sceneState.empty())
            return;

        auto backupScene = Scene::Scene::Restore(sceneState);
        scene.GetResource()->Swap(backupScene);

        cameraEntity = Scene::Entity::Restore(scene.Get(), cameraState);

        sceneState.clear();
        cameraState.clear();

    }

    void SceneWindow::SaveScene() {

        if (!scene.IsLoaded())
            return;

        Singletons::blockingOperation->Block("Saving scene. Please wait...",
            [&]() {
                cameraState = Scene::Entity::Backup(scene.Get(), cameraEntity);

                scene->DestroyEntity(cameraEntity);

                Serializer::SerializeScene(scene.Get(), "scenes/" + std::string(scene->name) + ".aescene", true);

                cameraEntity = Scene::Entity::Restore(scene.Get(), cameraState);

                cameraState.clear();

                Notifications::Push({ .message = "Saved scene " + scene->name });
            });       

    }

}
