#include "SceneWindow.h"
#include "common/MathHelper.h"
#include "../../Singletons.h"
#include "../../Notifications.h"
#include "../../tools/ResourcePayloadHelper.h"
#include "../../Theme.h"
#include "../Widgets.h"

#include "Serializer.h"
#include "common/Hash.h"
#include "tools/TerrainTool.h"
#include "Clock.h"
#include "profiler/CPUProfiler.h"
#include "ImguiExtension/panels/PostProcessingPanel.h"

#include <imgui_internal.h>
#include <ImGuizmo.h>
#include <glm/gtx/quaternion.hpp>

namespace Atlas::Editor::UI {

    // Front looks along -z like a new camera
    const ViewDirection SceneWindow::viewDirections[] = {
        { "Top", "top", vec2(glm::pi<float>(), -glm::half_pi<float>()) },
        { "Front", "front", vec2(glm::pi<float>(), 0.0f) },
        { "Right", "right", vec2(-glm::half_pi<float>(), 0.0f) },
        { "Bottom", "bottom", vec2(glm::pi<float>(), glm::half_pi<float>()) },
        { "Back", "back", vec2(0.0f, 0.0f) },
        { "Left", "left", vec2(glm::half_pi<float>(), 0.0f) }
    };

    SceneWindow::SceneWindow(ResourceHandle<Scene::Scene> scene, bool show) :
        Window(scene.IsLoaded() ? GetSceneLabel(scene) : "No scene", show), scene(scene) {
    
        RegisterViewportAndGizmoOverlay();

        // Overwrite the window name, the file identifies the scene: two files can contain scenes with the same name.
        // The id stays the same while the label gets room for the unsaved marker
        SetTitle(GetSceneLabel(scene), GetScenePath(scene));

        sceneHierarchyPanel.history = &history;

    }

    std::string SceneWindow::GetScenePath(const ResourceHandle<Scene::Scene>& scene) {

        if (!scene.IsLoaded())
            return "";
        // Scenes created in the editor without a file are saved by their name
        if (scene.IsGenerated())
            return "scenes/" + scene->name + ".aescene";
        return scene.GetResource()->path;

    }

    std::string SceneWindow::GetSceneLabel(const ResourceHandle<Scene::Scene>& scene) {

        if (!scene.IsLoaded())
            return "No scene";
        if (scene.IsGenerated())
            return scene->name;
        return Common::Path::GetFileNameWithoutExtension(scene.GetResource()->path);

    }

    SceneWindow::~SceneWindow() {

        

    }

    void SceneWindow::Update(float deltaTime) {

        if (!scene.IsLoaded() || !show)
            return;

        scene->buildMeshShapesInBackground = true;

        // Playing removes the editor camera until the scene state is restored
        if (!cameraEntity.IsValid() && !isPlaying) {
            // Scenes opened for the first time start with the view of their camera, later on the window config has the last view
            auto startLocation = vec3(0.0f);
            auto startRotation = vec2(0.0f);
            auto cameraSubset = scene->GetSubset<CameraComponent>();
            for (auto entity : cameraSubset) {
                const auto& sceneCamera = cameraSubset.Get(entity);
                if (sceneCamera.isMain) {
                    startLocation = sceneCamera.location;
                    startRotation = sceneCamera.rotation;
                    break;
                }
            }

            cameraEntity = scene->CreateEntity();
            auto& camera = cameraEntity.AddComponent<CameraComponent>(47.0f, 2.0f, 1.0f, 2000.0f);
            camera.location = startLocation;
            camera.rotation = startRotation;
            camera.isMain = true;
        }

        // Deleting the viewed scene camera returns to the editor camera
        bool viewingCamera = ECS::Entity(viewedCameraEntity) != ECS::EntityConfig::InvalidEntity;
        if (viewingCamera && !isPlaying && (!viewedCameraEntity.IsValid() || !viewedCameraEntity.HasComponent<CameraComponent>()))
            ReturnToEditorCamera();

        sceneHierarchyPanel.Update(scene.Get());

        scene->viewAspectRatio = float(viewportPanel.viewport->width) / std::max(float(viewportPanel.viewport->height), 1.0f);

        // If we're playing we can update here since we don't expect values to change from the UI side
        if (isPlaying) {
            auto stepping = stepRequested;
            stepRequested = false;
            if (stepping) {
                scene->paused = false;
                deltaTime = stepTime;
            }

            if (mainScript && !scene->paused) {
                mainScript->Update(deltaTime);
                // The editor plays the scene of its window, the player of a project changes scenes
                if (!mainScript->requestedScene.empty()) {
                    Notifications::Push({ .message = "Host.LoadScene(\"" + mainScript->requestedScene +
                        "\") changes the scene in the player, the editor keeps playing this one", .type = NotificationType::Warning });
                    mainScript->requestedScene.clear();
                }
            }
            scene->Update(deltaTime);
            if (stepping)
                scene->paused = true;
            if (mainScript && mainScript->quitRequested) {
                StopPlaying();
                return;
            }
        }
        else {
            ApplyBrush();
            ApplyNavigationTools();
        }

    }

    void SceneWindow::Render() {

        if (scene.IsLoaded())
            SetTitle(GetSceneLabel(scene), GetScenePath(scene));

        if (!Begin())
            return;

        if (ImGui::IsDragDropActive() && ImGui::IsWindowHovered()) {
            ImGui::SetWindowFocus();
        }

        bool isBlocked = Singletons::blockingOperation->block;

        // Get rid of the selection and don't allow new one
        if (lockSelection || brushActive) {
            sceneHierarchyPanel.selectedEntity = Scene::Entity();
            sceneHierarchyPanel.selectedEntities.clear();
        }

        ImGuiID dsID = ImGui::GetID(dockSpaceNameID.c_str());

        if (!ImGui::DockBuilderGetNode(dsID) || resetDockingLayout) {
            ImGui::SetWindowSize(ImVec2(640, 480), ImGuiCond_FirstUseEver);

            auto viewport = ImGui::GetWindowViewport();
            ImGui::DockBuilderRemoveNode(dsID);
            ImGui::DockBuilderAddNode(dsID, ImGuiDockNodeFlags_DockSpace);

            ImGui::DockBuilderSetNodeSize(dsID, viewport->Size);

            uint32_t dockIdLeft, dockIdMiddle, dockIdRight;
            ImGui::DockBuilderSplitNode(dsID, ImGuiDir_Left, 0.15f, &dockIdLeft, &dockIdMiddle);
            ImGui::DockBuilderSplitNode(dockIdMiddle, ImGuiDir_Left, 0.77f, &dockIdMiddle, &dockIdRight);

            // we now dock our windows into the docking node we made above
            ImGui::DockBuilderDockWindow(sceneHierarchyPanel.GetNameID(), dockIdLeft);
            ImGui::DockBuilderDockWindow(viewportPanel.GetNameID(), dockIdMiddle);
            ImGui::DockBuilderDockWindow(scenePropertiesPanel.GetNameID(), dockIdRight);
            ImGui::DockBuilderFinish(dsID);

            resetDockingLayout = false;
        }

        // Panels which are alone in their node don't need a tab bar
        ImGui::DockSpace(dsID, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_AutoHideTabBar);

        // Due to docking it doesn't register child windows as focused as well, need to check in child itself
        inFocus = ImGui::IsWindowFocused() ||
            sceneHierarchyPanel.isFocused || scenePropertiesPanel.isFocused || viewportPanel.isFocused;

        // A blocking operation changes the scene in a job, the panels would read it meanwhile
        if (isBlocked) {
            End();
            lockSelection = false;
            return;
        }

        Ref<Scene::Scene> refScene = scene.IsLoaded() ? scene.Get() : nullptr;

        CPUProfiler::Scope sectionScope("Properties panel");
        // Depending on the selection in the scene hierarchy panel, render the properties in a different window
        if (scene.IsLoaded()) {
            const auto& target = viewportPanel.renderTarget;

            EntityProperties entityProperties = {
                .entity = sceneHierarchyPanel.selectedEntity,
                .editorCameraEntity = cameraEntity,
                .history = &history
            };
            // Each of them is compared, larger selections only show the last one
            if (!isPlaying && sceneHierarchyPanel.selectedEntities.size() <= maxSelectedHistoryEntities) {
                for (auto ecsEntity : sceneHierarchyPanel.selectedEntities) {
                    Scene::Entity entity(ecsEntity, &scene->entityManager);
                    if (entity.IsValid() && entity != sceneHierarchyPanel.selectedEntity)
                        entityProperties.otherEntities.push_back(entity);
                }
            }

            if (sceneHierarchyPanel.selectedProperty.terrain)
                scenePropertiesPanel.Render(scene->terrain, refScene, target);
            else if (sceneHierarchyPanel.selectedProperty.ocean)
                scenePropertiesPanel.Render(scene->ocean);
            else if (sceneHierarchyPanel.selectedProperty.navigation)
                scenePropertiesPanel.Render(scene->navigation);
            else if (sceneHierarchyPanel.selectedProperty.fog)
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

        sectionScope.Next("Hierarchy panel");
        sceneHierarchyPanel.Render(refScene);

        // Widgets of the properties panel edit the scene, the hierarchy reports its own changes
        auto& context = *ImGui::GetCurrentContext();
        auto propertiesWindow = ImGui::FindWindowByName(scenePropertiesPanel.GetNameID());
        bool propertiesEdited = context.ActiveIdHasBeenEditedThisFrame && context.ActiveIdWindow &&
            context.ActiveIdWindow->RootWindow == propertiesWindow;
        if (!isPlaying && (propertiesEdited || sceneHierarchyPanel.sceneChanged))
            hasUnsavedChanges = true;
        sceneHierarchyPanel.sceneChanged = false;

        // Only the window the user works in has edits, an interaction which moved the focus away still has to finish
        if (!isPlaying && scene.IsLoaded() && (inFocus || history.history.IsInteracting()))
            history.Update(scene.Get(), sceneHierarchyPanel.selectedEntities);
        if (scene.IsLoaded())
            scenePropertiesPanel.navigationPanel.Update(scene->navigation);
        sectionScope.Next("Overlays");

        if (sceneHierarchyPanel.focusRequested) {
            sceneHierarchyPanel.focusRequested = false;
            FocusSelection();
        }

        if (sceneHierarchyPanel.createRequest.pending) {
            auto request = sceneHierarchyPanel.createRequest;
            sceneHierarchyPanel.createRequest.pending = false;
            CreateEntityFromTemplate(request.type, request.parent);
        }

        if (scene.IsLoaded() && !isPlaying && cameraEntity.IsValid())
            overlays.Render({ *scene.Get(), viewportPanel, cameraEntity.GetComponent<CameraComponent>(), cameraEntity,
                GetViewLocation(), sceneHierarchyPanel.selectedEntities });
        else
            overlays.ClearIcons();

        for (auto entity : sceneHierarchyPanel.selectedEntities)
            overlays.RenderSelected(*scene.Get(), viewportPanel, Scene::Entity(entity, &scene->entityManager));

        if (scene.IsLoaded() && sceneHierarchyPanel.selectedEntity.IsValid())
            scenePropertiesPanel.entityPropertiesPanel.softBodyComponentPanel.RenderOverlay(scene.Get(),
                sceneHierarchyPanel.selectedEntity, viewportPanel.primitiveBatchWrapper);

        if (showGrid && !isPlaying && scene.IsLoaded() && cameraEntity.IsValid())
            viewportPanel.RenderGrid(cameraEntity.GetComponent<CameraComponent>());

        if (showPhysics && scene.IsLoaded())
            RenderPhysics();

        sectionScope.Next("Update scene");
        // We want to update the scene after all panels have update their respective values/changed the scene
        if (!isPlaying) {
            // Path tracing needs history while ray tracing
            scene->rayTracingWorld->includeObjectHistory = Singletons::config->pathTrace;

            ApplyRequestedHistorySteps();
            scene->Update(Clock::GetDelta(), cameraEntity);

            if (focusAfterUpdate) {
                FocusSelection();
                focusAfterUpdate = false;
            }
        }

        sectionScope.Next("Viewport panel");
        viewportPanel.frameColor = !isPlaying ? ImVec4(0.0f, 0.0f, 0.0f, 0.0f) :
            scene->paused ? Theme::Warning() : Theme::Success();
        viewportPanel.Render(refScene);

        auto path = ResourcePayloadHelper::AcceptDropResourceAndGetPath<Scene::Entity>();
        if (!path.empty()) {
            history.BeginChange(scene.Get(), {});
            auto entity = Serializer::DeserializePrefab(scene.Get(), path);
            PushEntityIntoSceneHierarchy(entity);
            history.EndChange("Add " + Common::Path::GetFileNameWithoutExtension(path), { entity });
            hasUnsavedChanges = true;
        }

        End();

        
        

        lockSelection = false;

    }

    void SceneWindow::RegisterViewportAndGizmoOverlay() {

        viewportPanel.DrawMenuBar([&]() { RenderViewportToolbar(); });
        viewportPanel.DrawOverlay([&]() { RenderViewportOverlay(); });

    }

    void SceneWindow::RenderViewportToolbar() {

        const float margin = ViewportPanel::toolbarMargin;

        auto region = ImGui::GetContentRegionAvail();
        auto windowPos = ImGui::GetWindowPos();
        auto drawList = ImGui::GetWindowDrawList();

        RenderGizmoToolbar(margin);
        RenderPlayControls(region.x);
        RenderViewportSettingsToolbar(margin, region.x);

        if (ImGui::BeginPopup("Scene cameras")) {
            ImGui::SeparatorText("View through");

            bool hasSceneCamera = false;
            auto cameraSubset = scene->GetSubset<CameraComponent>();
            for (auto ecsEntity : cameraSubset) {
                if (ecsEntity == cameraEntity)
                    continue;

                Scene::Entity entity(ecsEntity, &scene->entityManager);
                auto nameComponent = entity.TryGetComponent<NameComponent>();
                auto name = nameComponent ? nameComponent->name : "Camera " + std::to_string(entity.GetID());

                ImGui::PushID(int32_t(entity.GetID()));
                if (ImGui::Selectable(name.c_str(), entity == viewedCameraEntity))
                    ViewThroughCamera(entity);
                ImGui::PopID();
                hasSceneCamera = true;
            }
            if (!hasSceneCamera)
                ImGui::TextDisabled("The scene has no cameras");

            if (viewedCameraEntity.IsValid()) {
                ImGui::Separator();
                if (ImGui::Selectable("Apply view to camera"))
                    ApplyViewToCamera();
                ImGui::SetItemTooltip("Moves the scene camera to the current view, including field of view, planes and exposure");
                if (ImGui::Selectable("Back to editor camera"))
                    ReturnToEditorCamera();
            }

            ImGui::SeparatorText("View");
            for (size_t i = 0; i < std::size(viewDirections); i++) {
                if (ImGui::Button(viewDirections[i].label, ImVec2(5.0f * ImGui::GetFontSize(), 0.0f)))
                    SetViewRotation(viewDirections[i].rotation);
                if (i % 3 != 2)
                    ImGui::SameLine();
            }
            auto orthographic = orthographicView.active;
            if (ImGui::Checkbox("Orthographic", &orthographic))
                SetOrthographic(orthographic);
            ImGui::SetItemTooltip("Looks from far away with a small field of view, the mouse wheel zooms");

            ImGui::SeparatorText("Bookmarks");
            std::optional<int32_t> removedBookmark;
            for (const auto& [index, bookmark] : bookmarks) {
                ImGui::PushID(index);
                if (Widgets::IconButton("Remove bookmark", Singletons::icons->Get(IconType::Trash), ImGui::GetTextLineHeight(), "Remove bookmark"))
                    removedBookmark = index;
                ImGui::SameLine();
                char label[64];
                std::snprintf(label, sizeof(label), "%d (%.0f, %.0f, %.0f)", index, bookmark.location.x, bookmark.location.y, bookmark.location.z);
                if (ImGui::Selectable(label))
                    GoToBookmark(index);
                ImGui::PopID();
            }
            if (removedBookmark)
                bookmarks.erase(*removedBookmark);
            if (int32_t(bookmarks.size()) < maxBookmarks && ImGui::Selectable("Add bookmark")) {
                auto index = 1;
                while (bookmarks.contains(index))
                    index++;
                SetBookmark(index);
            }

            ImGui::EndPopup();
        }

        if (ImGui::BeginPopup("Viewport settings")) {
            ImGui::SeparatorText("Movement");

            ImGui::DragFloat("Movement speed", &cameraMovementSpeed, 0.1f, 0.1f, 500.0f);
            ImGui::SetItemTooltip("The mouse wheel over the viewport changes the speed as well");
            ImGui::DragFloat("Rotation speed", &cameraRotationSpeed, 0.1f, 0.1f, 10.0f);

            auto& camera = cameraEntity.GetComponent<CameraComponent>();

            ImGui::SeparatorText("Editor camera");

            ImGui::DragFloat("Exposure", &camera.exposure, 0.1f, 0.01f, 180.0f);
            ImGui::DragFloat("Field of view", &camera.fieldOfView, 0.1f, 1.0f, 180.0f);

            ImGui::DragFloat("Near plane", &camera.nearPlane, 0.01f, 0.01f, 10.0f);
            ImGui::DragFloat("Far plane", &camera.farPlane, 1.0f, 1.0f, 20000.0f);

            ImGui::SeparatorText("Rendering");

            ImGui::DragFloat("Resolution scale##Rendering", &resolutionScale, 0.01f, 0.1f, 1.0f);

            ImGui::SeparatorText("Levels of detail");
            auto getLodLabel = [](int32_t lod) { return lod < 0 ? std::string("Automatic") : "LOD " + std::to_string(lod); };
            if (ImGui::BeginCombo("Shown level##Mesh LODs", getLodLabel(scene->forcedMeshLod).c_str())) {
                for (int32_t lod = -1; lod < Mesh::MeshData::maxLodCount; lod++) {
                    if (ImGui::Selectable(getLodLabel(lod).c_str(), lod == scene->forcedMeshLod))
                        scene->forcedMeshLod = lod;
                }
                ImGui::EndCombo();
            }
            ImGui::SetItemTooltip("Shows all meshes in this level or their coarsest one");
            ImGui::Checkbox("Color by level##Mesh LODs", &scene->debugMeshLods);
            ImGui::SetItemTooltip("White, green, blue, yellow and red from the full detail on");

            ImGui::SeparatorText("Path tracer samples");
            // ImGui::Checkbox("Realtime", &viewportPanel.mainRenderer->pathTracingRenderer.realTime);
            ImGui::DragInt("Sample count", &viewportPanel.mainRenderer->pathTracingRenderer.realTimeSamplesPerFrame, 1, 1, 16);
            ImGui::DragFloat("Max clipping", &viewportPanel.mainRenderer->pathTracingRenderer.historyClipMax, 0.001f, 0.0f, 1.0f);
            ImGui::DragFloat("Spatial filter strength", &viewportPanel.mainRenderer->pathTracingRenderer.spatialFilterStrength, 0.01f, 0.0f, 16.0f);
            ImGui::BeginDisabled(!Graphics::GraphicsDevice::DefaultDevice->support.shaderExecutionReordering);
            ImGui::Checkbox("Shader execution reordering##PathTracer", &viewportPanel.mainRenderer->pathTracingRenderer.shaderExecutionReordering);
            ImGui::Checkbox("Material graphs##PathTracer", &viewportPanel.mainRenderer->pathTracingRenderer.materialGraphs);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("Without them the hits of graph materials use their baked values");
            ImGui::EndDisabled();

            ImGui::SeparatorText("Playing");
            ImGui::Checkbox("Play maximized", &playMaximized);
            ImGui::Checkbox("Show performance overlay", &perfOverlayMaximized);

            ImGui::EndPopup();
        }

        if (viewportPanel.renderTarget->GetScalingFactor() != resolutionScale)
            viewportPanel.renderTarget->SetScalingFactor(resolutionScale);

        viewportPanel.RenderVisualizationPopup([&]() {
            ImGui::Checkbox(Singletons::commandPalette->GetTooltip("Grid", "Viewport: Toggle grid").c_str(), &showGrid);
            overlays.RenderKindSettings();
            ImGui::Checkbox("Navigation mesh", &scenePropertiesPanel.navigationPanel.showMesh);
            ImGui::Checkbox("Physics", &showPhysics);
            ImGui::SetItemTooltip("Static bodies are gray, dynamic green (dark when sleeping), kinematic orange, "
                "triggers cyan, fluids blue, contacts red and constraints magenta");

            if (ImGui::TreeNode("Overlay settings")) {
                overlays.RenderSettings(viewportPanel);
                ImGui::TreePop();
            }
            if (showPhysics && ImGui::TreeNode("Physics settings")) {
                auto& settings = physicsDebugDrawSettings;
                ImGui::Checkbox("Depth test physics", &depthTestPhysics);
                ImGui::Checkbox("Shapes", &settings.drawShapes);
                ImGui::Checkbox("Bounds", &settings.drawBounds);
                ImGui::Checkbox("Static bodies", &settings.drawStaticBodies);
                ImGui::Checkbox("Contacts", &settings.drawContacts);
                ImGui::Checkbox("Constraints", &settings.drawConstraints);
                ImGui::DragFloat("Distance", &settings.distance, 1.0f, 1.0f, 10000.0f);
                ImGui::TreePop();
            }
            });

        // Short hint after the camera speed was changed with the mouse wheel
        auto timeSinceSpeedChange = Clock::Get() - cameraSpeedChangeTime;
        if (timeSinceSpeedChange < 1.5f) {
            auto alpha = Helper::Math::Saturate((1.5f - timeSinceSpeedChange) / 0.3f);
            char speedText[64];
            std::snprintf(speedText, sizeof(speedText), "Camera speed %.1f", cameraMovementSpeed);

            auto textSize = ImGui::CalcTextSize(speedText);
            auto pillPadding = ImVec2(12.0f, 6.0f);
            auto pillMin = ImVec2(windowPos.x + (region.x - textSize.x) * 0.5f - pillPadding.x,
                windowPos.y + region.y - textSize.y - 2.0f * pillPadding.y - 16.0f);
            auto pillMax = ImVec2(pillMin.x + textSize.x + 2.0f * pillPadding.x, pillMin.y + textSize.y + 2.0f * pillPadding.y);

            drawList->AddRectFilled(pillMin, pillMax, ImGui::GetColorU32(Theme::OverlayBackground(alpha)), Theme::overlayRounding);
            drawList->AddText(ImVec2(pillMin.x + pillPadding.x, pillMin.y + pillPadding.y),
                ImGui::GetColorU32(ImGuiCol_Text, alpha), speedText);
        }

        // Don't leave the cursor on the toolbar line
        ImGui::NewLine();

    }

    void SceneWindow::RenderGizmoToolbar(float margin) {

        if (!isPlaying) {
            auto getTooltip = [](const std::string& mode) {
                return Singletons::commandPalette->GetTooltip(mode, "Viewport: " + mode + " gizmo");
                };
            viewportPanel.BeginToolbarGroup(margin, 4);

            if (viewportPanel.ToolbarButton("Translate button", IconType::Move, guizmo.mode == ImGuizmo::OPERATION::TRANSLATE,
                true, getTooltip("Translate").c_str()))
                guizmo.mode = ImGuizmo::OPERATION::TRANSLATE;
            if (viewportPanel.ToolbarButton("Rotate button", IconType::Rotate, guizmo.mode == ImGuizmo::OPERATION::ROTATE,
                true, getTooltip("Rotate").c_str()))
                guizmo.mode = ImGuizmo::OPERATION::ROTATE;
            if (viewportPanel.ToolbarButton("Scale button", IconType::Scale, guizmo.mode == ImGuizmo::OPERATION::SCALE,
                true, getTooltip("Scale").c_str()))
                guizmo.mode = ImGuizmo::OPERATION::SCALE;
            if (viewportPanel.ToolbarButton("More button", IconType::MoreHorizontal, false, true, "Gizmo settings"))
                ImGui::OpenPopup("Guizmo settings");

            if (ImGui::BeginPopup("Guizmo settings")) {
                ImGui::SeparatorText("Snapping");

                float translationSnap = guizmo.translationSnap.x;
                ImGui::Checkbox("Enabled", &guizmo.snappingEnabled);
                ImGui::DragFloat("Translation snap", &translationSnap, 0.01f, 0.001f, 1000.0f, "%.3f m", ImGuiSliderFlags_AlwaysClamp);
                ImGui::DragFloat("Rotation snap", &guizmo.rotationSnap, 0.5f, 0.1f, 180.0f, "%.1f\xc2\xb0", ImGuiSliderFlags_AlwaysClamp);
                for (auto angle : { 5, 15, 45, 90 }) {
                    if (angle != 5)
                        ImGui::SameLine();
                    if (ImGui::SmallButton((std::to_string(angle) + "\xc2\xb0").c_str()))
                        guizmo.rotationSnap = float(angle);
                }
                ImGui::DragFloat("Scale snap", &guizmo.scaleSnap, 0.01f, 0.001f, 10.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);

                guizmo.translationSnap = vec3(translationSnap);

                ImGui::SeparatorText("Manipulate");
                ImGui::Checkbox("Object space", &guizmo.objectSpace);

                ImGui::SeparatorText("Surface snapping");
                ImGui::TextDisabled("Hold Shift while translating to place on surfaces");
                ImGui::Checkbox("Align to surface normal", &guizmo.alignToSurface);

                ImGui::EndPopup();
            }
        }

    }

    void SceneWindow::RenderPlayControls(float width) {

        auto playUvMin = ImVec2(0.2f, 0.2f);
        auto playUvMax = ImVec2(0.8f, 0.8f);

        viewportPanel.BeginToolbarGroup((width - viewportPanel.GetToolbarGroupWidth(3)) * 0.5f, 3);

        // While playing, the play button pauses and resumes
        bool running = isPlaying && !scene->paused;
        if (viewportPanel.ToolbarButton("Play button", running ? IconType::Pause : IconType::Play, false, scene.IsLoaded(),
            !isPlaying ? "Play scene, with Shift from the view" : running ? "Pause playing" : "Resume playing", playUvMin, playUvMax)) {
            if (isPlaying)
                TogglePause();
            else
                StartPlaying(ImGui::GetIO().KeyShift);
        }
        if (viewportPanel.ToolbarButton("Step button", IconType::ArrowRight, false, scene.IsLoaded() && isPlaying,
            running ? "Pause playing" : "Play one step", playUvMin, playUvMax))
            StepFrame();
        if (viewportPanel.ToolbarButton("Stop button", IconType::Stop, false, scene.IsLoaded() && isPlaying,
            Singletons::commandPalette->GetTooltip("Stop playing", "Scene: Stop playing").c_str(), playUvMin, playUvMax))
            StopPlaying();

    }

    void SceneWindow::RenderViewportSettingsToolbar(float margin, float width) {

        if (!isPlaying) {
            auto uvMin = ImVec2(0.1f, 0.1f);
            auto uvMax = ImVec2(0.9f, 0.9f);

            viewportPanel.BeginToolbarGroup(width - margin - viewportPanel.GetToolbarGroupWidth(3), 3);

            if (viewportPanel.ToolbarButton("Scene cameras button", IconType::Camera, viewedCameraEntity.IsValid(),
                scene.IsLoaded(), "View through a scene camera", uvMin, uvMax))
                ImGui::OpenPopup("Scene cameras");
            if (viewportPanel.ToolbarButton("Viewport settings button", IconType::Settings, false, scene.IsLoaded(),
                "Viewport settings", uvMin, uvMax))
                ImGui::OpenPopup("Viewport settings");
            if (viewportPanel.ToolbarButton("Visualization settings button", IconType::Visibility,
                viewportPanel.visualization != ViewportVisualization::Lit, scene.IsLoaded(),
                "Show and visualization", uvMin, uvMax))
                ImGui::OpenPopup("Visualization settings");
        }

    }

    void SceneWindow::RenderViewportOverlay() {

        // Mouse wheel over the viewport changes the camera speed
        const auto& wheelIO = ImGui::GetIO();
        if (!isPlaying && ImGui::IsWindowHovered() && wheelIO.MouseWheel != 0.0f && !wheelIO.KeyCtrl && orthographicView.active) {
            const auto& camera = cameraEntity.GetComponent<CameraComponent>();
            auto focusPoint = camera.location + camera.direction * GetOrthographicDistance();
            orthographicView.focusDistance = glm::clamp(orthographicView.focusDistance * std::pow(1.25f, -wheelIO.MouseWheel), 0.5f, 5000.0f);
            UpdateOrthographicCamera(focusPoint);
        }
        else if (!isPlaying && ImGui::IsWindowHovered() && wheelIO.MouseWheel != 0.0f && !wheelIO.KeyCtrl) {
            cameraMovementSpeed = std::clamp(cameraMovementSpeed * std::pow(1.25f, wheelIO.MouseWheel), 0.1f, 500.0f);
            cameraSpeedChangeTime = Clock::Get();
        }

        guizmo.needEnabled = false;
        auto selectedEntity = sceneHierarchyPanel.selectedEntity;

        if (cameraEntity.IsValid() && isActiveWindow && !isPlaying) {

            guizmo.needEnabled = true;
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

            bool renderGuizmo = true;
            auto& entityPropPanel = scenePropertiesPanel.entityPropertiesPanel;
            auto selectionType = sceneHierarchyPanel.selectedType;

            auto globalMatrix = scene->GetGlobalMatrix(selectedEntity);

            // The selected pin bounds of a soft body take the place of the entity gizmo
            auto pinGizmo = selectedEntity.IsValid() && selectionType == EntitySelectionType::Entity &&
                entityPropPanel.softBodyComponentPanel.RenderPinGizmo(scene.Get(), selectedEntity, viewport, camera);

            if (pinGizmo) {
                renderGuizmo = false;
            }
            else if (selectedEntity.IsValid() && selectionType == EntitySelectionType::Entity && selectedEntity.HasComponent<TransformComponent>()) {
                auto& transform = selectedEntity.GetComponent<TransformComponent>();
                auto mesh = selectedEntity.TryGetComponent<MeshComponent>();

                auto globalDecomp = Common::MatrixDecomposition(transform.globalMatrix);
                if (mesh) {
                    // The bounds of animated meshes change with their pose, the gizmo stays at the center of the rest pose
                    auto bounds = mesh->mesh.IsLoaded() ? mesh->mesh->data.aabb.Transform(transform.globalMatrix) : mesh->aabb;
                    guizmo.offset = (0.5f * (bounds.min + bounds.max)) - globalDecomp.translation;
                }
                else {
                    guizmo.offset = vec3(0.0f);
                }
                guizmo.transform = transform.globalMatrix;
            }
            else if (selectedEntity.IsValid() && selectionType == EntitySelectionType::Spline && selectedEntity.HasComponent<SplineComponent>()) {
                const auto& spline = selectedEntity.GetComponent<SplineComponent>();

                // Undo can remove the selected control point
                auto splineControlIdx = entityPropPanel.splineComponentPanel.selectControlPointIdx;
                if (splineControlIdx >= 0 && splineControlIdx < int32_t(spline.controlPoints.size())) {
                    const auto& controlPoint = spline.controlPoints[splineControlIdx];

                    guizmo.offset = vec3(0.0f);
                    guizmo.transform = globalMatrix * controlPoint.transform;
                }
                else {
                    renderGuizmo = false;
                }
            }
            else {
                renderGuizmo = false;
            }

            if (renderGuizmo) {
                guizmo.ApplyOffsetToTransform();

                ImDrawList* drawList = ImGui::GetWindowDrawList();
                auto size = drawList->VtxBuffer.Size;

                auto snappingPtr = guizmo.GetSnappingPtr();
                ImGuizmo::Manipulate(glm::value_ptr(vMatrix), glm::value_ptr(pMatrix),
                    static_cast<ImGuizmo::OPERATION>(guizmo.mode), guizmo.objectSpace ? ImGuizmo::MODE::LOCAL : ImGuizmo::MODE::WORLD,
                    glm::value_ptr(guizmo.transform), nullptr, snappingPtr);

                guizmo.RemoveOffsetFromTransform();

                // Only visible if something was drawn
                bool visible = drawList->VtxBuffer.Size != size;

                // Need to disable here, otherwise we can't move around anymore
                if (!visible) {
                    guizmo.needEnabled = false;
                }
            }

            if (ImGuizmo::IsUsing())
                hasUnsavedChanges = true;

            if (ImGuizmo::IsUsing() && !pinGizmo && selectionType == EntitySelectionType::Entity && selectedEntity.HasComponent<TransformComponent>()) {
                auto& transform = selectedEntity.GetComponent<TransformComponent>();
                // Update both the local and global matrix, since e.g. the transform component
                // panel recovers the local matrix from the global one (needs to do this after
                // e.g. physics only updated the global matrix
                auto prevGlobalMatrix = transform.globalMatrix;
                auto invPrevGlobalMatrix = glm::inverse(transform.globalMatrix);
                auto newGlobalMatrix = guizmo.transform;

                // Shift places the bottom of the selection on the surface under the cursor
                auto bounds = scene->GetHierarchyBounds(selectedEntity);
                if (guizmo.mode == ImGuizmo::OPERATION::TRANSLATE && ImGui::GetIO().KeyShift && bounds) {
                    auto mousePosition = ImGui::GetMousePos();
                    Volume::Ray ray(viewport, camera, vec2(mousePosition.x, mousePosition.y));
                    auto result = scene->CastRay(ray, Scene::SceneQueryComponentBits::AllComponentsBit,
                        [&](const Scene::Entity& entity) { return IsInSelection(entity); });

                    if (result.valid) {
                        auto hit = ray.Get(result.hitDistance);
                        auto bottom = vec3(0.5f * (bounds->min.x + bounds->max.x), bounds->min.y,
                            0.5f * (bounds->min.z + bounds->max.z));
                        newGlobalMatrix = glm::translate(hit - bottom) * prevGlobalMatrix;

                        if (guizmo.alignToSurface && result.IsNormalValid()) {
                            auto up = Helper::Math::NormalizeOrZero(vec3(prevGlobalMatrix[1]));
                            auto rotation = glm::mat4_cast(glm::rotation(up, result.normal));
                            newGlobalMatrix = glm::translate(hit) * rotation * glm::translate(-hit) * newGlobalMatrix;
                        }
                    }
                }

                // Children of selected entities follow their parent through the hierarchy
                auto hasSelectedAncestor = [&](Scene::Entity entity) {
                    for (auto parent = scene->GetParentEntity(entity); parent.IsValid(); parent = scene->GetParentEntity(parent))
                        if (sceneHierarchyPanel.selectedEntities.contains(parent) && parent.HasComponent<TransformComponent>())
                            return true;
                    return false;
                };

                for (auto& entity : sceneHierarchyPanel.selectedEntities) {
                    Scene::Entity transformEntity(entity, &scene->entityManager);

                    if (!transformEntity.HasComponent<TransformComponent>() || hasSelectedAncestor(transformEntity))
                        continue;

                    auto& selectedTransform = transformEntity.GetComponent<TransformComponent>();
                    selectedTransform.SetGlobal(newGlobalMatrix * invPrevGlobalMatrix * selectedTransform.globalMatrix,
                        scene->GetParentEntity(transformEntity));
                }
            }
            else if (ImGuizmo::IsUsing() && selectionType == EntitySelectionType::Spline && selectedEntity.HasComponent<SplineComponent>()) {
                auto& spline = selectedEntity.GetComponent<SplineComponent>();

                auto splineControlIdx = entityPropPanel.splineComponentPanel.selectControlPointIdx;
                if (splineControlIdx >= 0 && splineControlIdx < int32_t(spline.controlPoints.size()))
                    spline.controlPoints[splineControlIdx].transform = glm::inverse(globalMatrix) * guizmo.transform;
            }

            const auto& io = ImGui::GetIO();

            auto windowPos = ImGui::GetWindowPos();
            auto mousePos = vec2(io.MousePos.x, io.MousePos.y);

            bool inViewport = mousePos.x > float(viewport->x)
                && mousePos.y > float(viewport->y)
                && mousePos.x < float(viewport->x + viewport->width)
                && mousePos.y < float(viewport->y + viewport->height);

            overlays.RenderIconLabels(*scene.Get(), inViewport && ImGui::IsWindowHovered() && !ImGuizmo::IsOver(), mousePos);

            if (ImGui::IsKeyPressed(ImGuiKey_MouseRight, false) && inViewport && !lockSelection && !brushActive) {

                PerformEntitySelection();

            }
        }

    }

    void SceneWindow::PushEntityIntoSceneHierarchy(Scene::Entity entity, bool changeSelection) {
        auto parentEntity = sceneHierarchyPanel.selectedEntity;

        if (!parentEntity.IsValid())
            parentEntity = scene->GetRootEntity();

        entity.SetParent(parentEntity);

        // We don't know where this entity came from, so disable root hierarchy in any case to
        // avoid setting the matrices twice and getting unwanted movements
        auto hierarchy = entity.TryGetComponent<HierarchyComponent>();
        if (hierarchy)
            hierarchy->root = false;

        if (changeSelection) {
            sceneHierarchyPanel.SelectEntity(entity);
        }
    }

    bool SceneWindow::StartPlaying(bool fromView) {

        // Playing starts with the next update
        if (isPlaying || Singletons::blockingOperation->IsPending())
            return false;

        bool hasMainPlayingCamera = false;
        auto cameraSubset = scene->GetSubset<CameraComponent>();
        for (auto entity : cameraSubset) {
            if (entity == cameraEntity)
                continue;

            const auto& comp = cameraSubset.Get(entity);
            hasMainPlayingCamera |= comp.isMain;
        }

        if (!hasMainPlayingCamera)
            Notifications::Push({ .message = "The scene has no main camera, it plays with the view of the editor" });

        // Steps which weren't applied yet would change the scene while playing
        requestedHistorySteps = 0;

        SetOrthographic(false);

        Singletons::blockingOperation->Block("Preparing to play. Please wait...", [this, fromView, hasMainPlayingCamera]() {
            auto view = cameraEntity.GetComponent<CameraComponent>();
            SaveSceneState();

            if (fromView || !hasMainPlayingCamera)
                PlaceInView(view, hasMainPlayingCamera);

            scene->physicsWorld->pauseSimulation = false;
            scene->paused = false;

            const auto& project = Singletons::project;
            if (project && !project->mainScript.empty()) {
                mainScript = std::make_unique<Scripting::LuaHost>();
                if (mainScript->Load(project->mainScript))
                    mainScript->SetScene(scene.Get());
                if (mainScript->HasFailed())
                    Notifications::Push({ .message = "The main script " + project->mainScript + " failed, the log has the error",
                        .type = NotificationType::Error });
            }
            // Unselect when starting the simulation/scene (otherwise some physics settings might not
            // be reverted after stopping
            sceneHierarchyPanel.ClearSelection();

            isPlaying = true;

            if (playMaximized) {
                Notifications::Push({ "To stop playing, press " + Singletons::commandPalette->GetShortcutText("Scene: Stop playing") });
            }
            });        

        return true;

    }

    Scene::Entity SceneWindow::FindPlayer() {

        // Other characters move on their own, without input everything could be the player
        Scene::Entity player;
        auto players = scene->GetSubset<PlayerComponent, TransformComponent>();
        for (auto ecsEntity : players) {
            Scene::Entity entity(ecsEntity, &scene->entityManager);
            if (entity.GetComponent<PlayerComponent>().allowInput)
                return entity;
            if (!player.IsValid())
                player = entity;
        }
        return player;

    }

    void SceneWindow::PlaceInView(const CameraComponent& view, bool hasMainCamera) {

        auto player = hasMainCamera ? FindPlayer() : Scene::Entity();
        if (player.IsValid()) {
            // On the ground below the view, a fall could hurt the player. Its own colliders are in its hierarchy
            auto position = view.GetLocation();
            Volume::Ray ray(position, vec3(0.0f, -1.0f, 0.0f));
            auto result = scene->CastRay(ray, Scene::SceneQueryComponentBits::AllComponentsBit, [&](const Scene::Entity& entity) {
                for (auto ancestor = entity; ancestor.IsValid(); ancestor = scene->GetParentEntity(ancestor))
                    if (ancestor == player)
                        return true;
                return false;
                });
            if (result.valid)
                position = ray.Get(result.hitDistance);

            auto& transform = player.GetComponent<TransformComponent>();
            auto matrix = transform.globalMatrix;
            matrix[3] = vec4(position, 1.0f);
            transform.SetGlobal(matrix, scene->GetParentEntity(player));

            if (auto camera = player.TryGetComponent<CameraComponent>())
                camera->rotation = view.rotation;
            return;
        }

        auto cameraSubset = scene->GetSubset<CameraComponent>();
        for (auto ecsEntity : cameraSubset)
            cameraSubset.Get(ecsEntity).isMain = false;

        playCameraEntity = scene->CreateEntity();
        auto& camera = playCameraEntity.AddComponent<CameraComponent>(view.fieldOfView, view.aspectRatio, view.nearPlane,
            view.farPlane, view.location, view.rotation);
        camera.exposure = view.exposure;
        camera.isMain = true;

    }

    void SceneWindow::StepFrame() {

        if (!isPlaying)
            return;

        // The first step of a running scene stops it
        stepRequested = scene->paused;
        scene->paused = true;

    }

    void SceneWindow::TogglePause() {

        if (!isPlaying)
            return;

        scene->paused = !scene->paused;

    }

    void SceneWindow::StopPlaying() {

        Singletons::blockingOperation->Block("Restoring scene state. Please wait...", [&]() {
            // The script refers to the scene which is replaced
            mainScript.reset();
            RestoreSceneState();

            isPlaying = false;
            });        

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
        // Entities selected while playing belong to the replaced scene
        sceneHierarchyPanel.ClearSelection();
        playCameraEntity = Scene::Entity();

        cameraEntity = Scene::Entity::Restore(scene.Get(), cameraState);
        if (ECS::Entity(viewedCameraEntity) != ECS::EntityConfig::InvalidEntity)
            viewedCameraEntity = Scene::Entity(viewedCameraEntity, &scene->entityManager);

        sceneState.clear();
        cameraState.clear();

    }

    void SceneWindow::SaveScene() {

        if (!scene.IsLoaded())
            return;

        if (IsPrefab()) {
            Scene::Entity root(prefabRoot, &scene->entityManager);
            if (!root.IsValid()) {
                Notifications::Push({ .message = "The root entity of the prefab was deleted", .type = NotificationType::Error });
                return;
            }
            auto hierarchy = root.TryGetComponent<HierarchyComponent>();
            if (hierarchy)
                hierarchy->root = prefabRootIsHierarchyRoot;
            auto saved = Serializer::SerializePrefab(scene.Get(), root, prefabPath);
            if (hierarchy)
                hierarchy->root = false;
            NotifySaved(saved, "prefab " + Common::Path::GetFileName(prefabPath));
            return;
        }

        Singletons::blockingOperation->Block("Saving scene. Please wait...",
            [&]() {
                cameraState = Scene::Entity::Backup(scene.Get(), cameraEntity);

                scene->DestroyEntity(cameraEntity);

                // Back into the file the scene came from, its name can differ from the file name
                auto saved = Serializer::SerializeScene(scene.Get(), GetScenePath(scene), true, true);

                // Here we can finally reset the in-editing status, as it was now saved and we could reload from disk
                if (saved && scene->terrain.IsLoaded())
                    scene->terrain->storage->inEditing = false;

                cameraEntity = Scene::Entity::Restore(scene.Get(), cameraState);

                cameraState.clear();

                NotifySaved(saved, "scene " + GetSceneLabel(scene));
            });       

    }

    void SceneWindow::NotifySaved(bool saved, const std::string& label) {

        if (saved)
            Notifications::Push({ .message = "Saved " + label });
        else
            Notifications::Push({ .message = "Couldn't save " + label + ", the file on disk is unchanged",
                .type = NotificationType::Error });

        hasUnsavedChanges = !saved;

    }

    void SceneWindow::OpenPrefab(const std::string& path) {

        prefabPath = path;
        sceneHierarchyPanel.showSceneSettings = false;

        auto root = Serializer::DeserializePrefab(scene.Get(), path);
        auto hierarchy = root.TryGetComponent<HierarchyComponent>();
        prefabRootIsHierarchyRoot = hierarchy && hierarchy->root;
        PushEntityIntoSceneHierarchy(root);
        prefabRoot = root;

        // The bounds of the prefab are known after the scene was updated
        focusAfterUpdate = true;

    }

    void SceneWindow::RenderPhysics() {

        if (!scene->physicsWorld || !scene->HasMainCamera())
            return;

        physicsDebugDrawSettings.center = scene->GetMainCamera().GetLocation();

        auto& wrapper = viewportPanel.primitiveBatchWrapper;
        scene->physicsWorld->DebugDraw(wrapper.GetBatch(depthTestPhysics).get(), physicsDebugDrawSettings);

    }

    Scene::Entity SceneWindow::CreateEntityFromTemplate(SceneHierarchyPanel::EntityTemplate type, Scene::Entity parent) {

        using Template = SceneHierarchyPanel::EntityTemplate;

        if (!scene.IsLoaded() || isPlaying)
            return Scene::Entity();

        hasUnsavedChanges = true;

        if (!parent.IsValid())
            parent = scene->GetRootEntity();

        const char* name = "Entity";
        switch (type) {
        case Template::PointLight: name = "Point light"; break;
        case Template::SpotLight: name = "Spot light"; break;
        case Template::DirectionalLight: name = "Directional light"; break;
        case Template::Camera: name = "Camera"; break;
        case Template::AudioSource: name = "Audio source"; break;
        case Template::Text: name = "Text"; break;
        case Template::FogVolume: name = "Fog volume"; break;
        default: break;
        }

        history.BeginChange(scene.Get(), {});

        auto entity = scene->CreateEntity();
        entity.AddComponent<NameComponent>(name);

        entity.SetParent(parent);

        // Place it in front of the editor camera, such that it is directly visible
        auto worldPosition = vec3(0.0f);
        auto cameraRotation = vec2(0.0f);
        if (cameraEntity.IsValid()) {
            const auto& camera = cameraEntity.GetComponent<CameraComponent>();
            worldPosition = camera.GetLocation() + camera.direction * 5.0f;
            cameraRotation = camera.rotation;
        }

        // Lights, cameras etc. take their position from the transform, their own position stays at the origin
        auto worldMatrix = glm::translate(mat4(1.0f), worldPosition);
        auto& transform = entity.AddComponent<TransformComponent>(worldMatrix, false);
        transform.SetGlobal(worldMatrix, parent);

        switch (type) {
        case Template::PointLight:
            entity.AddComponent<LightComponent>(LightType::PointLight);
            break;
        case Template::SpotLight: {
            auto& light = entity.AddComponent<LightComponent>(LightType::SpotLight);
            light.properties.spot.direction = vec3(0.0f, -1.0f, 0.0f);
            light.properties.spot.innerConeAngle = 0.5f;
            light.properties.spot.outerConeAngle = 0.7f;
            break;
        }
        case Template::DirectionalLight: {
            auto& light = entity.AddComponent<LightComponent>(LightType::DirectionalLight);
            light.properties.directional.direction = glm::normalize(vec3(-0.4f, -1.0f, -0.3f));
            break;
        }
        case Template::Camera: {
            // Same lens as the editor camera and looking into the same direction
            auto& camera = entity.AddComponent<CameraComponent>(47.0f, 2.0f, 1.0f, 2000.0f);
            camera.rotation = cameraRotation;
            break;
        }
        case Template::AudioSource:
            entity.AddComponent<AudioComponent>();
            break;
        case Template::Text:
            entity.AddComponent<TextComponent>();
            break;
        case Template::FogVolume:
            entity.AddComponent<FogVolumeComponent>();
            // The scale is the half size of the volume
            transform.SetGlobal(glm::scale(worldMatrix, vec3(8.0f, 4.0f, 8.0f)), parent);
            break;
        default:
            break;
        }

        history.EndChange("Create " + std::string(name), { entity });

        sceneHierarchyPanel.SelectEntity(entity);

        return entity;

    }

    void SceneWindow::Undo() {

        requestedHistorySteps--;

    }

    void SceneWindow::Redo() {

        requestedHistorySteps++;

    }

    void SceneWindow::ApplyRequestedHistorySteps() {

        bool changed = false;
        for (; requestedHistorySteps < 0; requestedHistorySteps++)
            changed |= history.Undo(scene.Get());
        for (; requestedHistorySteps > 0; requestedHistorySteps--)
            changed |= history.Redo(scene.Get());

        if (changed)
            ApplyHistoryChange();

    }

    void SceneWindow::ApplyHistoryChange() {

        hasUnsavedChanges = true;

        // Selecting the entities of a large script step would make every following frame compare all of them
        auto changedEntities = history.GetChangedEntities();
        if (changedEntities.size() > maxSelectedHistoryEntities) {
            sceneHierarchyPanel.ClearSelection();
        }
        else if (!changedEntities.empty()) {
            sceneHierarchyPanel.ClearSelection();
            for (auto entity : changedEntities)
                sceneHierarchyPanel.SelectEntity(entity, true);
        }
        else {
            std::erase_if(sceneHierarchyPanel.selectedEntities, [&](ECS::Entity entity) {
                return !scene->entityManager.Valid(entity);
                });
            if (!sceneHierarchyPanel.selectedEntity.IsValid())
                sceneHierarchyPanel.ClearSelection();
        }

        scenePropertiesPanel.entityPropertiesPanel.transformComponentPanel.Refresh();

    }

    void SceneWindow::FocusSelection() {

        if (!scene.IsLoaded() || !cameraEntity.IsValid())
            return;

        auto bounds = GetSelectionBounds();
        if (!bounds) {
            Notifications::Push({ .message = "Select an entity to focus it" });
            return;
        }

        auto& camera = cameraEntity.GetComponent<CameraComponent>();

        // Keep the view direction and back off until the bounding sphere fits into the field of view
        auto center = 0.5f * (bounds->min + bounds->max);
        auto radius = std::max(0.5f * glm::length(bounds->max - bounds->min), 0.5f);
        auto fieldOfView = orthographicView.active ? orthographicView.fieldOfView : camera.fieldOfView;
        auto distance = radius / std::sin(glm::radians(fieldOfView) * 0.5f);

        if (orthographicView.active) {
            orthographicView.focusDistance = distance;
            UpdateOrthographicCamera(center);
            return;
        }

        camera.location = center - camera.direction * distance;

    }

    std::optional<Volume::AABB> SceneWindow::GetSelectionBounds() {

        std::optional<Volume::AABB> bounds;

        for (auto ecsEntity : sceneHierarchyPanel.selectedEntities) {
            Scene::Entity entity(ecsEntity, &scene->entityManager);
            if (!entity.IsValid())
                continue;

            // Meshes of the hierarchy have proper world space bounds, everything else is treated as a small box around its origin
            auto entityBounds = scene->GetHierarchyBounds(entity);
            if (!entityBounds) {
                auto position = vec3(scene->GetGlobalMatrix(entity)[3]);
                entityBounds = Volume::AABB(position - vec3(0.5f), position + vec3(0.5f));
            }

            if (bounds)
                bounds->Grow(*entityBounds);
            else
                bounds = entityBounds;
        }

        return bounds;

    }

    vec3 SceneWindow::GetFocusPoint() {

        if (auto bounds = GetSelectionBounds())
            return 0.5f * (bounds->min + bounds->max);

        const auto& camera = cameraEntity.GetComponent<CameraComponent>();
        if (orthographicView.active)
            return camera.location + camera.direction * GetOrthographicDistance();

        Volume::Ray ray(camera.location, camera.direction);
        auto result = scene->CastRay(ray);
        auto distance = result.valid ? std::min(result.hitDistance, 500.0f) : 20.0f;
        return camera.location + camera.direction * distance;

    }

    void SceneWindow::SetViewRotation(vec2 rotation) {

        if (!scene.IsLoaded() || isPlaying || !cameraEntity.IsValid())
            return;

        ReturnToEditorCamera();

        auto& camera = cameraEntity.GetComponent<CameraComponent>();
        auto focusPoint = GetFocusPoint();
        auto distance = glm::clamp(glm::distance(camera.location, focusPoint), 2.0f, 500.0f);

        camera.rotation = rotation;
        camera.direction = DirectionFromRotation(rotation);

        if (orthographicView.active)
            UpdateOrthographicCamera(focusPoint);
        else
            camera.location = focusPoint - camera.direction * distance;

    }

    float SceneWindow::GetOrthographicDistance() const {

        auto halfAngle = [](float fieldOfView) { return std::tan(glm::radians(fieldOfView) * 0.5f); };
        return orthographicView.focusDistance * halfAngle(orthographicView.fieldOfView) / halfAngle(orthographicFieldOfView);

    }

    void SceneWindow::UpdateOrthographicCamera(vec3 focusPoint) {

        auto& camera = cameraEntity.GetComponent<CameraComponent>();

        auto distance = GetOrthographicDistance();
        camera.location = focusPoint - camera.direction * distance;
        camera.fieldOfView = orthographicFieldOfView;
        // The depth only has to be precise around the point
        camera.nearPlane = std::max(orthographicView.nearPlane, 0.1f * distance);
        camera.farPlane = distance + orthographicView.farPlane;

    }

    void SceneWindow::SetOrthographic(bool orthographic) {

        if (!scene.IsLoaded() || isPlaying || !cameraEntity.IsValid() || orthographic == orthographicView.active)
            return;

        ReturnToEditorCamera();

        auto& camera = cameraEntity.GetComponent<CameraComponent>();
        if (orthographic) {
            auto focusPoint = GetFocusPoint();
            orthographicView = {
                .active = true,
                .focusDistance = glm::clamp(glm::dot(focusPoint - camera.location, camera.direction), 2.0f, 500.0f),
                .fieldOfView = camera.fieldOfView,
                .nearPlane = camera.nearPlane,
                .farPlane = camera.farPlane
            };
            UpdateOrthographicCamera(camera.location + camera.direction * orthographicView.focusDistance);
            return;
        }

        camera.location = GetViewLocation();
        camera.fieldOfView = orthographicView.fieldOfView;
        camera.nearPlane = orthographicView.nearPlane;
        camera.farPlane = orthographicView.farPlane;
        orthographicView.active = false;

    }

    vec3 SceneWindow::GetViewLocation() {

        const auto& camera = cameraEntity.GetComponent<CameraComponent>();
        if (!orthographicView.active)
            return camera.GetLocation();

        return camera.location + camera.direction * (GetOrthographicDistance() - orthographicView.focusDistance);

    }

    void SceneWindow::SetBookmark(int32_t index) {

        if (!cameraEntity.IsValid())
            return;

        const auto& camera = cameraEntity.GetComponent<CameraComponent>();
        bookmarks[index] = { .location = GetViewLocation(), .rotation = camera.rotation };
        Notifications::Push({ .message = "Set camera bookmark " + std::to_string(index) });

    }

    void SceneWindow::GoToBookmark(int32_t index) {

        auto bookmark = bookmarks.find(index);
        if (bookmark == bookmarks.end() || isPlaying || !cameraEntity.IsValid())
            return;

        SetOrthographic(false);
        ReturnToEditorCamera();

        auto& camera = cameraEntity.GetComponent<CameraComponent>();
        camera.location = bookmark->second.location;
        camera.rotation = bookmark->second.rotation;

    }

    void SceneWindow::DropSelectionToGround() {

        if (!scene.IsLoaded() || isPlaying)
            return;

        for (auto ecsEntity : sceneHierarchyPanel.selectedEntities) {
            Scene::Entity entity(ecsEntity, &scene->entityManager);
            if (!entity.IsValid() || !entity.HasComponent<TransformComponent>())
                continue;

            auto parent = scene->GetParentEntity(entity);
            if (parent.IsValid() && IsInSelection(parent))
                continue;

            auto& transform = entity.GetComponent<TransformComponent>();
            auto position = vec3(transform.globalMatrix[3]);
            auto bounds = scene->GetHierarchyBounds(entity).value_or(Volume::AABB(position, position));

            // From above the bounds, such that entities sunk into the ground come back up and the ray doesn't start
            // inside of what the entity rests on
            auto center = 0.5f * (bounds.min + bounds.max);
            auto height = std::max(bounds.max.y - bounds.min.y, 1.0f);
            Volume::Ray ray(vec3(center.x, bounds.max.y + height, center.z), vec3(0.0f, -1.0f, 0.0f));
            auto result = scene->CastRay(ray, Scene::SceneQueryComponentBits::AllComponentsBit,
                [&](const Scene::Entity& hitEntity) { return IsInSelection(hitEntity); });
            if (!result.valid)
                continue;

            auto offset = ray.Get(result.hitDistance).y - bounds.min.y;
            transform.SetGlobal(glm::translate(vec3(0.0f, offset, 0.0f)) * transform.globalMatrix, parent);
            hasUnsavedChanges = true;
        }

        scenePropertiesPanel.entityPropertiesPanel.transformComponentPanel.Refresh();

    }

    // Inverse of the direction CameraComponent::UpdateView computes from the rotation
    vec3 SceneWindow::DirectionFromRotation(vec2 rotation) {

        return glm::normalize(vec3(std::cos(rotation.y) * std::sin(rotation.x), std::sin(rotation.y),
            std::cos(rotation.y) * std::cos(rotation.x)));

    }

    static vec2 RotationFromDirection(vec3 direction) {

        return vec2(std::atan2(direction.x, direction.z), std::asin(glm::clamp(direction.y, -1.0f, 1.0f)));

    }

    void SceneWindow::ViewThroughCamera(Scene::Entity entity) {

        if (!scene.IsLoaded() || isPlaying || !cameraEntity.IsValid() || entity == cameraEntity ||
            !entity.IsValid() || !entity.HasComponent<CameraComponent>())
            return;

        SetOrthographic(false);

        auto& camera = cameraEntity.GetComponent<CameraComponent>();
        if (!viewedCameraEntity.IsValid())
            editorCameraBackup = camera;

        const auto& sceneCamera = entity.GetComponent<CameraComponent>();

        camera.location = sceneCamera.globalLocation;
        camera.rotation = RotationFromDirection(sceneCamera.globalDirection);
        camera.fieldOfView = sceneCamera.fieldOfView;
        camera.nearPlane = sceneCamera.nearPlane;
        camera.farPlane = sceneCamera.farPlane;
        camera.exposure = sceneCamera.exposure;
        camera.thirdPerson = sceneCamera.thirdPerson;
        camera.thirdPersonDistance = sceneCamera.thirdPersonDistance;
        // The view shows the bars of the camera as well
        camera.aspectRatio = sceneCamera.aspectRatio;
        camera.constrainAspectRatio = sceneCamera.constrainAspectRatio;

        viewedCameraEntity = entity;

    }

    void SceneWindow::ApplyViewToCamera() {

        if (!viewedCameraEntity.IsValid() || !viewedCameraEntity.HasComponent<CameraComponent>())
            return;

        history.BeginEdit(scene.Get(), { viewedCameraEntity });

        const auto& camera = cameraEntity.GetComponent<CameraComponent>();
        auto& sceneCamera = viewedCameraEntity.GetComponent<CameraComponent>();

        // The scene camera is relative to its entity, depending on which parts of the entity transform it uses
        auto transform = viewedCameraEntity.TryGetComponent<TransformComponent>();
        auto inverseMatrix = transform ? glm::inverse(transform->globalMatrix) : mat4(1.0f);

        sceneCamera.location = sceneCamera.useEntityTranslation ?
            vec3(inverseMatrix * vec4(camera.location, 1.0f)) : camera.location;
        auto direction = sceneCamera.useEntityRotation ?
            glm::normalize(mat3(inverseMatrix) * camera.direction) : camera.direction;
        sceneCamera.rotation = RotationFromDirection(direction);

        sceneCamera.fieldOfView = camera.fieldOfView;
        sceneCamera.nearPlane = camera.nearPlane;
        sceneCamera.farPlane = camera.farPlane;
        sceneCamera.exposure = camera.exposure;
        sceneCamera.thirdPersonDistance = camera.thirdPersonDistance;
        hasUnsavedChanges = true;

        history.EndEdit("Apply view to camera");

        auto nameComponent = viewedCameraEntity.TryGetComponent<NameComponent>();
        Notifications::Push({ .message = "Applied the view to " + (nameComponent ? nameComponent->name : std::string("the camera")) });

    }

    void SceneWindow::ReturnToEditorCamera() {

        if (!cameraEntity.IsValid() || ECS::Entity(viewedCameraEntity) == ECS::EntityConfig::InvalidEntity)
            return;

        auto& camera = cameraEntity.GetComponent<CameraComponent>();
        camera.location = editorCameraBackup.location;
        camera.rotation = editorCameraBackup.rotation;
        camera.fieldOfView = editorCameraBackup.fieldOfView;
        camera.nearPlane = editorCameraBackup.nearPlane;
        camera.farPlane = editorCameraBackup.farPlane;
        camera.exposure = editorCameraBackup.exposure;
        camera.thirdPerson = editorCameraBackup.thirdPerson;
        camera.thirdPersonDistance = editorCameraBackup.thirdPersonDistance;
        camera.aspectRatio = editorCameraBackup.aspectRatio;
        camera.constrainAspectRatio = editorCameraBackup.constrainAspectRatio;
        // Needed before the next update of the scene
        camera.direction = DirectionFromRotation(camera.rotation);

        viewedCameraEntity = Scene::Entity();

    }

    bool SceneWindow::IsInSelection(Scene::Entity entity) {

        for (; entity.IsValid(); entity = scene->GetParentEntity(entity))
            if (sceneHierarchyPanel.selectedEntities.contains(entity))
                return true;
        return false;

    }

    void SceneWindow::ApplyBrush() {

        auto& terrainPanel = scenePropertiesPanel.terrainPanel;

        brushActive = terrainPanel.editingMode;

        // Rebuilding the height field is expensive, it's done once a stroke ends
        bool mousePressed = ImGui::GetIO().MouseDown[ImGuiMouseButton_Right];
        if (terrainHeightsEdited)
            hasUnsavedChanges = true;
        if (terrainHeightsEdited && (!mousePressed || !brushActive)) {
            terrainPanel.RebuildTerrainBody(scene.Get());
            terrainHeightsEdited = false;
        }

        if (!brushActive || !cameraEntity.IsValid())
            return;

        auto& viewport = viewportPanel.viewport;
        auto camera = cameraEntity.GetComponent<CameraComponent>();

        auto mousePos = ImGui::GetMousePos();
        Volume::Ray ray(viewport, camera, vec2(mousePos.x, mousePos.y));
        
        Scene::SceneQueryComponents componentBits = Scene::SceneQueryComponentBits::TerrainComponentBit;
        if (terrainPanel.brushFlattenQueryRegularGeometry &&
            terrainPanel.brushFunction == TerrainPanel::TerrainBrushFunction::Flatten)
            componentBits = Scene::SceneQueryComponentBits::RigidBodyComponentBit;

        auto result = scene->CastRay(ray, componentBits);

        bool brushTerrain = mousePressed && result.valid && terrainPanel.editingMode;

        if (!mousePressed)
            terrainFlattenHeight = Terrain::Terrain::invalidHeight;

        float brushRadius = terrainPanel.brushSize * scene->terrain->resolution;

        auto intersection = ray.Get(result.hitDistance);
        viewportPanel.primitiveBatchWrapper.GetBatch(true)->AddLineSphere(intersection, brushRadius, vec3(1.0f));

        // Check for right mouse button down
        if (brushTerrain) {
            uint32_t brushSize = uint32_t(terrainPanel.brushSize);
            // This should only trigger once when the button is started to be pressed
            if (terrainFlattenHeight == Terrain::Terrain::invalidHeight ||
                terrainPanel.brushFlattenQueryRegularGeometry)
                terrainFlattenHeight = intersection.y;

            if (terrainPanel.brushType == TerrainPanel::TerrainBrushType::Height) {
                Filter filter;
                
                float brushStrength = terrainPanel.brushStrength;

                if (terrainPanel.brushFunction == TerrainPanel::TerrainBrushFunction::Gauss) {
                    auto sigma = (float)brushSize / 6.0f;
                    filter.CalculateGaussianFilter(sigma, brushSize);
                }
                if (terrainPanel.brushFunction == TerrainPanel::TerrainBrushFunction::Box) {
                    filter.CalculateBoxFilter(brushSize);
                }

                if (terrainPanel.brushFunction == TerrainPanel::TerrainBrushFunction::Gauss ||
                    terrainPanel.brushFunction == TerrainPanel::TerrainBrushFunction::Box) {
                    Tools::TerrainTool::BrushHeight(scene->terrain.Get(), &filter,
                        brushStrength, vec2(intersection.x, intersection.z));
                }
                else if(terrainPanel.brushFunction == TerrainPanel::TerrainBrushFunction::Smooth) {
                    brushSize = 2 * brushSize + 1;
                    Tools::TerrainTool::SmoothHeight(scene->terrain.Get(), brushSize,
                        1, brushStrength, vec2(intersection.x, intersection.z));
                }
                else if (terrainPanel.brushFunction == TerrainPanel::TerrainBrushFunction::Flatten &&
                    terrainFlattenHeight != Terrain::Terrain::invalidHeight) {
                    brushSize = 2 * brushSize + 1;
                    Tools::TerrainTool::FlattenHeight(scene->terrain.Get(), brushSize,
                        brushStrength, vec2(intersection.x, intersection.z), terrainFlattenHeight);
                }
                terrainHeightsEdited = true;

            }
            else if (terrainPanel.brushType == TerrainPanel::TerrainBrushType::Material) {
                brushSize = 2 * brushSize + 1;
                Atlas::Tools::TerrainTool::BrushMaterial(scene->terrain.Get(),
                    vec2(intersection.x, intersection.z),
                    brushSize, terrainPanel.materialBrushSelection);
            }
            else if (terrainPanel.brushType == TerrainPanel::TerrainBrushType::Hole) {
                brushSize = 2 * brushSize + 1;
                Atlas::Tools::TerrainTool::BrushHole(scene->terrain.Get(),
                    vec2(intersection.x, intersection.z), brushSize);
                terrainHeightsEdited = true;
            }

            
        }

    }

    void SceneWindow::ApplyNavigationTools() {

        auto& navigationPanel = scenePropertiesPanel.navigationPanel;
        const auto& navigation = scene->navigation;
        if (!navigation || !navigation->mesh || !cameraEntity.IsValid())
            return;

        const auto& mesh = navigation->mesh;
        const auto& camera = cameraEntity.GetComponent<CameraComponent>();
        const auto& batch = viewportPanel.primitiveBatchWrapper.GetBatch(true);
        // Lifted a little, the ground would hide the lines in the depth test
        const auto lift = vec3(0.0f, 0.05f, 0.0f);

        if (navigationPanel.showMesh) {
            // All built tiles, collected again when the mesh changes
            if (navigationPanel.meshEdgesMesh != mesh.get() || navigationPanel.meshEdgesRevision != mesh->GetRevision()) {
                navigationPanel.meshEdges = mesh->GetPolygonEdges(Volume::AABB(vec3(-1e30f), vec3(1e30f)));
                navigationPanel.meshEdgesMesh = mesh.get();
                navigationPanel.meshEdgesRevision = mesh->GetRevision();
            }

            const auto& edges = navigationPanel.meshEdges;
            for (size_t i = 0; i + 1 < edges.size(); i += 2)
                batch->AddLine(edges[i] + lift, edges[i + 1] + lift, vec3(0.0f, 0.6f, 1.0f), vec3(0.0f, 0.6f, 1.0f));
        }

        if (!navigationPanel.pathTest || !sceneHierarchyPanel.selectedProperty.navigation)
            return;

        // Right clicks set the points instead of selecting entities
        brushActive = true;

        const auto& viewport = viewportPanel.viewport;
        auto mousePos = ImGui::GetMousePos();
        bool inViewport = mousePos.x > float(viewport->x) && mousePos.y > float(viewport->y) &&
            mousePos.x < float(viewport->x + viewport->width) && mousePos.y < float(viewport->y + viewport->height);

        if (inViewport && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            Volume::Ray ray(viewport, camera, vec2(mousePos.x, mousePos.y));
            auto result = scene->CastRay(ray);
            if (result.valid) {
                auto point = ray.Get(result.hitDistance);
                if (!navigationPanel.pathStart || navigationPanel.pathGoal) {
                    navigationPanel.pathStart = point;
                    navigationPanel.pathGoal.reset();
                }
                else {
                    navigationPanel.pathGoal = point;
                }
            }
        }

        // The tiles change while they are built, the path is searched again every frame
        navigationPanel.path.clear();
        if (navigationPanel.pathStart && navigationPanel.pathGoal)
            navigationPanel.path = mesh->FindPath(*navigationPanel.pathStart, *navigationPanel.pathGoal);

        if (navigationPanel.pathStart)
            batch->AddLineCross(*navigationPanel.pathStart, 0.5f, vec3(0.0f, 1.0f, 0.0f));
        if (navigationPanel.pathGoal)
            batch->AddLineCross(*navigationPanel.pathGoal, 0.5f, vec3(1.0f, 0.0f, 0.0f));
        for (size_t i = 1; i < navigationPanel.path.size(); i++)
            batch->AddLine(navigationPanel.path[i - 1] + 2.0f * lift, navigationPanel.path[i] + 2.0f * lift,
                vec3(1.0f, 0.9f, 0.1f), vec3(1.0f, 0.9f, 0.1f));

    }

    void SceneWindow::PerformEntitySelection() {       

        const auto& io = ImGui::GetIO();
        auto mousePos = vec2(io.MousePos.x, io.MousePos.y);

        auto viewport = viewportPanel.viewport;
        const auto& camera = cameraEntity.GetComponent<CameraComponent>();
     
        auto nearPoint = viewport->Unproject(vec3(mousePos, 0.0f), camera);
        auto farPoint = viewport->Unproject(vec3(mousePos, 1.0f), camera);

        Atlas::Volume::Ray ray(camera.GetLocation(), glm::normalize(farPoint - nearPoint));

        auto rayCastResult = scene->CastRay(ray);

        // Icons are drawn over the scene
        auto iconEntity = overlays.GetIconAt(*scene.Get(), mousePos);
        if (iconEntity.IsValid()) {
            rayCastResult.valid = true;
            rayCastResult.hitDistance = 0.0f;
            rayCastResult.data = iconEntity;
        }

        EntitySelectionType selectionType = EntitySelectionType::Entity;

        auto selectedEntity = sceneHierarchyPanel.selectedEntity;
        auto parentEntity = scene->GetParentEntity(selectedEntity);
        auto selectedIdx = -1;

        // Check if current selected entity has a component that can be selected
        if (selectedEntity.IsValid() && selectedEntity.HasComponent<SplineComponent>()) {
            auto& splineComponent = selectedEntity.GetComponent<SplineComponent>();
            
            auto globalMatrix = scene->GetGlobalMatrix(selectedEntity);
            for (int32_t i = 0; i < int32_t(splineComponent.controlPoints.size()); i++) {
                auto controlPoint = splineComponent.controlPoints[i];
                controlPoint.transform = globalMatrix * controlPoint.transform;

                auto pos = vec3(controlPoint.transform[3]);

                Volume::AABB aabb(pos - vec3(0.025f), pos + vec3(0.025f));
                float distance;
                bool intersects = ray.Intersects(aabb, distance);
                if (intersects && distance < rayCastResult.hitDistance) {
                    selectedIdx = i;
                    selectionType = EntitySelectionType::Spline;
                }
            }
        }

        auto& entityPropPanel = scenePropertiesPanel.entityPropertiesPanel;

        // Reset previous selection where possible
        entityPropPanel.splineComponentPanel.selectControlPointIdx = -1;

        if (rayCastResult.valid && selectionType == EntitySelectionType::Entity) {
            bool shiftPressed = ImGui::IsKeyDown(ImGuiKey_LeftShift);
            sceneHierarchyPanel.SelectEntity(rayCastResult.data, shiftPressed);
            sceneHierarchyPanel.RevealEntity(scene.Get(), rayCastResult.data);
        }
        else if (selectionType == EntitySelectionType::Spline) {
            entityPropPanel.splineComponentPanel.selectControlPointIdx = selectedIdx;
        }
        
        sceneHierarchyPanel.selectedType = selectionType;

    }

}
