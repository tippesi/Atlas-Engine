#include "SceneWindow.h"
#include "common/MathHelper.h"
#include "../../Singletons.h"
#include "../../Notifications.h"
#include "../../tools/ResourcePayloadHelper.h"
#include "../../Theme.h"

#include "Serializer.h"
#include "common/Hash.h"
#include "tools/TerrainTool.h"
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
            // Scripts create and destroy entities, which would race with the hierarchy search
            sceneHierarchyPanel.WaitForSearch();
            scene->Update(deltaTime);
        }
        else {
            auto& camera = cameraEntity.GetComponent<CameraComponent>();
            camera.aspectRatio = float(viewportPanel.viewport->width) / std::max(float(viewportPanel.viewport->height), 1.0f);

            ApplyBrush();
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
        
        if (controlDown && isPlaying && ImGui::IsKeyPressed(ImGuiKey_Q, false)) {
            StopPlaying();
        }   

        // Viewport shortcuts. W/A/S/D/Q/E are used for camera movement, therefore gizmo modes are on 1/2/3.
        // Ignore them while typing into a text field.
        bool typing = ImGui::GetIO().WantTextInput;
        if (inFocus && !isPlaying && !typing && !controlDown) {
            if (ImGui::IsKeyPressed(ImGuiKey_1, false))
                guizmo.mode = ImGuizmo::OPERATION::TRANSLATE;
            if (ImGui::IsKeyPressed(ImGuiKey_2, false))
                guizmo.mode = ImGuizmo::OPERATION::ROTATE;
            if (ImGui::IsKeyPressed(ImGuiKey_3, false))
                guizmo.mode = ImGuizmo::OPERATION::SCALE;
            if (ImGui::IsKeyPressed(ImGuiKey_F, false))
                FocusSelection();
            if (ImGui::IsKeyPressed(ImGuiKey_G, false))
                showGrid = !showGrid;
        }

    }

    void SceneWindow::Render() {

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

            if (sceneHierarchyPanel.selectedProperty.terrain)
                scenePropertiesPanel.Render(scene->terrain, refScene, target);
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

        sceneHierarchyPanel.Render(refScene, inFocus);

        if (sceneHierarchyPanel.focusRequested) {
            sceneHierarchyPanel.focusRequested = false;
            FocusSelection();
        }

        if (sceneHierarchyPanel.createRequest.pending) {
            auto request = sceneHierarchyPanel.createRequest;
            sceneHierarchyPanel.createRequest.pending = false;
            CreateEntityFromTemplate(request.type, request.parent);
        }

        for (auto entity : sceneHierarchyPanel.selectedEntities) {
            Scene::Entity selectedEntity(entity, &scene->entityManager);
            RenderEntityBoundingVolumes(selectedEntity);
        }

        if (showGrid && !isPlaying && scene.IsLoaded())
            RenderGrid();

        // We want to update the scene after all panels have update their respective values/changed the scene
        if (!isPlaying && !isBlocked) {
            // Path tracing needs history while ray tracing
            scene->rayTracingWorld->includeObjectHistory = Singletons::config->pathTrace;

            scene->Update(Clock::GetDelta(), cameraEntity);
        }

        viewportPanel.Render(refScene, isActiveWindow);

        auto path = ResourcePayloadHelper::AcceptDropResourceAndGetPath<Scene::Entity>();
        if (!path.empty()) {
            auto entity = Serializer::DeserializePrefab(scene.Get(), path);
            PushEntityIntoSceneHierarchy(entity);
        }

        End();

        
        

        lockSelection = false;

    }

    void SceneWindow::RegisterViewportAndGizmoOverlay() {

        viewportPanel.DrawMenuBar([&]() {

            // Toolbar is drawn as floating, rounded button groups on top of the viewport
            const float margin = 8.0f;
            const float groupPadding = 3.0f;
            const float buttonSpacing = 2.0f;
            const ImVec2 framePadding = ImVec2(5.0f, 5.0f);

            auto iconSize = ImGui::GetTextLineHeight() * 1.15f;
            auto region = ImGui::GetContentRegionAvail();
            auto windowPos = ImGui::GetWindowPos();
            auto drawList = ImGui::GetWindowDrawList();

            auto buttonExtent = iconSize + 2.0f * framePadding.x;
            auto groupHeight = buttonExtent + 2.0f * groupPadding;
            auto groupWidth = [&](int32_t buttonCount) {
                return float(buttonCount) * buttonExtent + float(buttonCount - 1) * buttonSpacing + 2.0f * groupPadding;
                };

            auto beginGroup = [&](float x, int32_t buttonCount) {
                auto min = ImVec2(windowPos.x + x, windowPos.y + margin);
                auto max = ImVec2(min.x + groupWidth(buttonCount), min.y + groupHeight);
                drawList->AddRectFilled(min, max, ImGui::GetColorU32(Theme::OverlayBackground()), Theme::overlayRounding);
                drawList->AddRect(min, max, ImGui::GetColorU32(Theme::OverlayBorder()), Theme::overlayRounding);
                ImGui::SetCursorPos(ImVec2(x + groupPadding, margin + groupPadding));
                };

            auto iconButton = [&](const char* id, IconType iconType, bool selected, bool enabled,
                const char* tooltip, ImVec2 uvMin = ImVec2(0.15f, 0.15f), ImVec2 uvMax = ImVec2(0.85f, 0.85f)) {
                auto& icon = Singletons::icons->Get(iconType);
                auto set = Singletons::imguiWrapper->GetTextureId(&icon);

                // Light mode icons are dark, so they would vanish on a solid accent fill, use a tint there
                bool darkMode = Singletons::config->darkMode;
                auto selectedColor = darkMode ? Theme::AccentFill() : Theme::Accent(0.22f);
                auto selectedHoverColor = darkMode ? Theme::AccentFillHovered() : Theme::Accent(0.32f);

                auto hoverColor = ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered);
                ImGui::PushStyleColor(ImGuiCol_Button, selected ? selectedColor : ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, selected ? selectedHoverColor : hoverColor);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, selectedColor);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, framePadding);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

                ImGui::BeginDisabled(!enabled);
                bool pressed = ImGui::ImageButton(id, set, ImVec2(iconSize, iconSize), uvMin, uvMax);
                ImGui::EndDisabled();

                ImGui::PopStyleVar(2);
                ImGui::PopStyleColor(3);

                ImGui::SetItemTooltip("%s", tooltip);
                ImGui::SameLine(0.0f, buttonSpacing);

                return pressed && enabled;
                };

            // Left: gizmo operations
            if (!isPlaying) {
                beginGroup(margin, 4);

                if (iconButton("Translate button", IconType::Move, guizmo.mode == ImGuizmo::OPERATION::TRANSLATE,
                    true, "Translate (1)"))
                    guizmo.mode = ImGuizmo::OPERATION::TRANSLATE;
                if (iconButton("Rotate button", IconType::Rotate, guizmo.mode == ImGuizmo::OPERATION::ROTATE,
                    true, "Rotate (2)"))
                    guizmo.mode = ImGuizmo::OPERATION::ROTATE;
                if (iconButton("Scale button", IconType::Scale, guizmo.mode == ImGuizmo::OPERATION::SCALE,
                    true, "Scale (3)"))
                    guizmo.mode = ImGuizmo::OPERATION::SCALE;
                if (iconButton("More button", IconType::MoreHorizontal, false, true, "Gizmo settings"))
                    ImGui::OpenPopup("Guizmo settings");

                if (ImGui::BeginPopup("Guizmo settings")) {
                    ImGui::SeparatorText("Snapping");

                    float translationSnap = guizmo.translationSnap.x;
                    ImGui::Checkbox("Enabled", &guizmo.snappingEnabled);
                    ImGui::DragFloat("Translation snap", &translationSnap, 0.01f, 0.1f, 100.0f);
                    ImGui::DragFloat("Rotation snap", &guizmo.rotationSnap, 0.1f, 0.1f, 10.0f);
                    ImGui::DragFloat("Scale snap", &guizmo.scaleSnap, 0.01f, 0.1f, 10.0f);

                    guizmo.translationSnap = vec3(translationSnap);

                    ImGui::SeparatorText("Manipulate");
                    ImGui::Checkbox("Object space", &guizmo.objectSpace);

                    ImGui::EndPopup();
                }
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

            // Center: play controls
            {
                auto playUvMin = ImVec2(0.2f, 0.2f);
                auto playUvMax = ImVec2(0.8f, 0.8f);

                beginGroup((region.x - groupWidth(2)) * 0.5f, 2);

                // While playing, the play button pauses and resumes
                bool running = isPlaying && !scene->paused;
                if (iconButton("Play button", running ? IconType::Pause : IconType::Play, false, scene.IsLoaded(),
                    !isPlaying ? "Play scene" : running ? "Pause playing" : "Resume playing", playUvMin, playUvMax)) {
                    if (isPlaying)
                        TogglePause();
                    else
                        StartPlaying();
                }
                if (iconButton("Stop button", IconType::Stop, false, scene.IsLoaded() && isPlaying,
                    "Stop playing (Ctrl+Q)", playUvMin, playUvMax))
                    StopPlaying();
            }

            // Right: viewport settings and visualization
            if (!isPlaying) {
                auto uvMin = ImVec2(0.1f, 0.1f);
                auto uvMax = ImVec2(0.9f, 0.9f);

                beginGroup(region.x - margin - groupWidth(2), 2);

                if (iconButton("Viewport settings button", IconType::Settings, false, scene.IsLoaded(),
                    "Viewport settings", uvMin, uvMax))
                    ImGui::OpenPopup("Viewport settings");
                if (iconButton("Visualization settings button", IconType::Visibility,
                    viewportPanel.visualization != ViewportVisualization::Lit, scene.IsLoaded(),
                    "Visualization", uvMin, uvMax))
                    ImGui::OpenPopup("Visualization settings");
            }

            if (ImGui::BeginPopup("Viewport settings")) {
                ImGui::SeparatorText("Overlays");
                ImGui::Checkbox("Show grid (G)", &showGrid);
                ImGui::Checkbox("Depth test bounding volumes", &depthTestBoundingVolumes);

                ImGui::SeparatorText("Movement");

                ImGui::DragFloat("Movement speed", &cameraMovementSpeed, 0.1f, 0.1f, 100.0f);
                ImGui::SetItemTooltip("The mouse wheel over the viewport changes the speed as well");
                ImGui::DragFloat("Rotation speed", &cameraRotationSpeed, 0.1f, 0.1f, 10.0f);

                auto& camera = cameraEntity.GetComponent<CameraComponent>();

                ImGui::SeparatorText("Editor camera");

                ImGui::DragFloat("Exposure", &camera.exposure, 0.1f, 0.01f, 180.0f);
                ImGui::DragFloat("Field of view", &camera.fieldOfView, 0.1f, 1.0f, 180.0f);

                ImGui::DragFloat("Near plane", &camera.nearPlane, 0.01f, 0.01f, 10.0f);
                ImGui::DragFloat("Far plane", &camera.farPlane, 1.0f, 1.0f, 20000.0f);

                ImGui::SeparatorText("Rendering scale");

                ImGui::DragFloat("Resolution scale##Rendering", &resolutionScale, 0.01f, 0.1f, 1.0f);

                ImGui::SeparatorText("Path tracer samples");
                // ImGui::Checkbox("Realtime", &Singletons::mainRenderer->pathTracingRenderer.realTime);
                ImGui::DragInt("Sample count", &Singletons::mainRenderer->pathTracingRenderer.realTimeSamplesPerFrame, 1, 1, 16);
                ImGui::DragFloat("Max clipping", &Singletons::mainRenderer->pathTracingRenderer.historyClipMax, 0.001f, 0.0f, 1.0f);

                ImGui::SeparatorText("Playing");
                ImGui::Checkbox("Play maximized", &playMaximized);
                ImGui::Checkbox("Show performance overlay", &perfOverlayMaximized);

                ImGui::EndPopup();
            }

            if (Singletons::renderTarget->GetScalingFactor() != resolutionScale)
                Singletons::renderTarget->SetScalingFactor(resolutionScale);

            auto menuItem = [&](const char* name, ViewportVisualization vis) {
                bool selected = viewportPanel.visualization == vis;
                ImGui::MenuItem(name, nullptr, &selected);
                if (selected)
                    viewportPanel.visualization = vis;
            };

            if (ImGui::BeginPopup("Visualization settings")) {
                ImGui::SeparatorText("Visualization");

                menuItem("Lit", ViewportVisualization::Lit);

                if (ImGui::BeginMenu("GBuffer")) {
                    menuItem("Base color", ViewportVisualization::GBufferBaseColor);
                    menuItem("Roughness/Metalness/Ao", ViewportVisualization::GBufferRoughnessMetalnessAo);
                    menuItem("Emissive color", ViewportVisualization::GBufferEmissive);
                    menuItem("Depth", ViewportVisualization::GBufferDepth);
                    menuItem("Normals", ViewportVisualization::GBufferNormals);
                    menuItem("Geometry normals", ViewportVisualization::GBufferGeometryNormals);
                    menuItem("Velocity", ViewportVisualization::GBufferVelocity);
                    menuItem("Material index", ViewportVisualization::GBufferMaterialIdx);
                    menuItem("Stencil", ViewportVisualization::GBufferStencil);
                    ImGui::EndMenu();
                }

                menuItem("Volumetrics", ViewportVisualization::Volumetrics);
                menuItem("Clouds", ViewportVisualization::Clouds);
                menuItem("Reflections", ViewportVisualization::Reflections);
                menuItem("SSS", ViewportVisualization::SSS);
                menuItem("SSGI", ViewportVisualization::SSGI);

                ImGui::EndPopup();
            }

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

            });

        viewportPanel.DrawOverlay([&]() {
            // Mouse wheel over the viewport changes the camera speed
            const auto& wheelIO = ImGui::GetIO();
            if (!isPlaying && ImGui::IsWindowHovered() && wheelIO.MouseWheel != 0.0f && !wheelIO.KeyCtrl) {
                cameraMovementSpeed = std::clamp(cameraMovementSpeed * std::pow(1.25f, wheelIO.MouseWheel), 0.1f, 100.0f);
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

                if (selectedEntity.IsValid() && selectionType == EntitySelectionType::Entity && selectedEntity.HasComponent<TransformComponent>()) {
                    auto& transform = selectedEntity.GetComponent<TransformComponent>();
                    auto mesh = selectedEntity.TryGetComponent<MeshComponent>();

                    auto globalDecomp = Common::MatrixDecomposition(transform.globalMatrix);
                    if (mesh)
                        guizmo.offset = (0.5f * (mesh->aabb.min + mesh->aabb.max)) - globalDecomp.translation;
                    guizmo.transform = transform.globalMatrix;
                }
                else if (selectedEntity.IsValid() && selectionType == EntitySelectionType::Spline && selectedEntity.HasComponent<SplineComponent>()) {
                    const auto& spline = selectedEntity.GetComponent<SplineComponent>();

                    auto splineControlIdx = entityPropPanel.splineComponentPanel.selectControlPointIdx;
                    const auto& controlPoint = spline.controlPoints[splineControlIdx];

                    guizmo.offset = vec3(0.0f);
                    guizmo.transform = globalMatrix * controlPoint.transform;
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

                if (ImGuizmo::IsUsing() && selectionType == EntitySelectionType::Entity && selectedEntity.HasComponent<TransformComponent>()) {
                    auto& transform = selectedEntity.GetComponent<TransformComponent>();
                    // Update both the local and global matrix, since e.g. the transform component
                    // panel recovers the local matrix from the global one (needs to do this after
                    // e.g. physics only updated the global matrix
                    auto prevGlobalMatrix = transform.globalMatrix;
                    auto invPrevGlobalMatrix = glm::inverse(transform.globalMatrix);
                    auto newGlobalMatrix = guizmo.transform;

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
                    auto& controlPoint = spline.controlPoints[splineControlIdx];

                    controlPoint.transform = glm::inverse(globalMatrix) * guizmo.transform;
                }

                const auto& io = ImGui::GetIO();
                
                auto windowPos = ImGui::GetWindowPos();
                auto mousePos = vec2(io.MousePos.x, io.MousePos.y);

                bool inViewport = mousePos.x > float(viewport->x) 
                    && mousePos.y > float(viewport->y)
                    && mousePos.x < float(viewport->x + viewport->width)
                    && mousePos.y < float(viewport->y + viewport->height);
                
                if (ImGui::IsKeyPressed(ImGuiKey_MouseRight, false) && inViewport && !lockSelection && !brushActive) {

                    PerformEntitySelection();

                }
            }
            });
           
    }

    void SceneWindow::RenderEntityBoundingVolumes(Scene::Entity entity) {

        if (!entity.IsValid())
            return;

        auto testDepth = depthTestBoundingVolumes;

        auto& wrapper = viewportPanel.primitiveBatchWrapper;

        if (entity.HasComponent<MeshComponent>()) {
            const auto& meshComponent = entity.GetComponent<MeshComponent>();
            auto aabb = meshComponent.aabb;
            wrapper.RenderLineAABB(aabb, vec3(1.0f, 1.0f, 0.0f), testDepth);
        }
        if (entity.HasComponent<AudioComponent>()) {
            const auto& audioComponent = entity.GetComponent<AudioComponent>();
            const auto transformComponent = entity.TryGetComponent<TransformComponent>();

            vec3 position = vec3(0.0f);
            if (transformComponent)
                position += transformComponent->Decompose().translation;

            // After this the audio will be cutoff
            float radius = powf(audioComponent.falloffFactor / audioComponent.cutoff, 1.0f / audioComponent.falloffPower);
            wrapper.RenderLineSphere(position, radius, vec3(0.0f, 1.0f, 0.0f), testDepth);
        }
        if (entity.HasComponent<AudioVolumeComponent>()) {
            const auto& audioVolumeComponent = entity.GetComponent<AudioVolumeComponent>();
            auto aabb = audioVolumeComponent.GetTransformedAABB();
            wrapper.RenderLineAABB(aabb, vec3(0.0f, 1.0f, 0.0f), testDepth);
        }
        if (entity.HasComponent<CameraComponent>()) {
            const auto& cameraComponent = entity.GetComponent<CameraComponent>();
            wrapper.RenderLineFrustum(cameraComponent.frustum, vec3(1.0f, 0.0f, 1.0f), testDepth);
        }
        if (entity.HasComponent<LightComponent>()) {
            const auto& lightComponent = entity.GetComponent<LightComponent>();
            if (lightComponent.shadow) {
                for (const auto& component : lightComponent.shadow->views)
                    wrapper.RenderLineFrustum(
                        Volume::Frustum(component.frustumMatrix), vec3(1.0f, 0.0f, 0.0f), testDepth);
            }
        }
        if (entity.HasComponent<TextComponent>()) {
            const auto& textComponent = entity.GetComponent<TextComponent>();
            auto rectangle = textComponent.GetRectangle();
            wrapper.RenderLineRectangle(rectangle,vec3(0.0f, 0.0f, 1.0f), testDepth);
        }
        if (entity.HasComponent<SplineComponent>()) {
            const auto& splineComponent = entity.GetComponent<SplineComponent>();
            auto transformComponent = entity.TryGetComponent<TransformComponent>();

            glm::mat4 globalMatrix = scene->GetGlobalMatrix(entity);

            SplinePoint lastSplinePoint = splineComponent.GetInterpolatedFromIndex(0, 0.0f);
            lastSplinePoint.position = vec3(globalMatrix * vec4(lastSplinePoint.position, 1.0f));

            if (!splineComponent.controlPoints.empty())
                wrapper.RenderLineSphere(lastSplinePoint.position, 0.025f, vec3(1.0f, 0.15f, 0.0f), testDepth);

            const int32_t linesPerSegment = 20;
            for (int32_t i = 0; i < int32_t(splineComponent.controlPoints.size()) - 1; i++) {
                for (int32_t j = 0; j < linesPerSegment; j++) {
                    auto t = float(j + 1) / float(linesPerSegment);
                    auto splinePoint = splineComponent.GetInterpolatedFromIndex(i, t);
                    splinePoint.position = vec3(globalMatrix * vec4(splinePoint.position, 1.0f));

                    wrapper.RenderLine(lastSplinePoint.position, splinePoint.position, 
                        vec3(1.0f, 0.15f, 0.0f), testDepth);

                    lastSplinePoint = splinePoint;
                }

                wrapper.RenderLineSphere(lastSplinePoint.position, 0.025f, vec3(1.0f, 0.15f, 0.0f), testDepth);
            }
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
            sceneHierarchyPanel.SelectEntity(entity);
        }
    }

    void SceneWindow::StartPlaying() {

        JobSystem::WaitAll();

        Singletons::blockingOperation->Block("Preparing to play. Please wait...", [&]() {
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
            scene->paused = false;
            // Unselect when starting the simulation/scene (otherwise some physics settings might not
            // be reverted after stopping
            sceneHierarchyPanel.ClearSelection();

            isPlaying = true;

            if (playMaximized) {
                Notifications::Push({ "To stop playing, press Ctrl + Q" });
            }
            });        

    }

    void SceneWindow::TogglePause() {

        if (!isPlaying)
            return;

        scene->paused = !scene->paused;

    }

    void SceneWindow::StopPlaying() {

        JobSystem::WaitAll();

        Singletons::blockingOperation->Block("Restoring scene state. Please wait...", [&]() {
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

                Serializer::SerializeScene(scene.Get(), "scenes/" + std::string(scene->name) + ".aescene", true, true);

                // Here we can finally reset the in-editing status, as it was now saved and we could reload from disk
                if (scene->terrain.IsLoaded())
                    scene->terrain->storage->inEditing = false;

                cameraEntity = Scene::Entity::Restore(scene.Get(), cameraState);

                cameraState.clear();

                Notifications::Push({ .message = "Saved scene " + scene->name });
            });       

    }

    void SceneWindow::RenderGrid() {

        if (!cameraEntity.IsValid())
            return;

        const auto& camera = cameraEntity.GetComponent<CameraComponent>();
        auto location = camera.GetLocation();

        // The spacing grows with the camera height (1, 10, 100, ...), such that the grid stays readable
        auto height = std::max(std::abs(location.y), 1.0f);
        auto spacing = std::pow(10.0f, std::floor(std::log10(height)));

        const int32_t halfLineCount = 50;
        auto extent = spacing * float(halfLineCount);
        auto center = glm::round(vec2(location.x, location.z) / spacing) * spacing;

        const auto minorColor = vec3(0.25f);
        const auto majorColor = vec3(0.45f);
        const auto xAxisColor = vec3(0.85f, 0.25f, 0.25f);
        const auto zAxisColor = vec3(0.25f, 0.45f, 0.95f);

        auto& wrapper = viewportPanel.primitiveBatchWrapper;
        for (int32_t i = -halfLineCount; i <= halfLineCount; i++) {
            auto offset = float(i) * spacing;

            // Line parallel to the z axis at this x position
            auto x = center.x + offset;
            auto xIndex = int64_t(std::round(x / spacing));
            auto xColor = std::abs(x) < 0.5f * spacing ? zAxisColor : (xIndex % 10 == 0 ? majorColor : minorColor);
            wrapper.RenderLine(vec3(x, 0.0f, center.y - extent), vec3(x, 0.0f, center.y + extent), xColor, true);

            // Line parallel to the x axis at this z position
            auto z = center.y + offset;
            auto zIndex = int64_t(std::round(z / spacing));
            auto zColor = std::abs(z) < 0.5f * spacing ? xAxisColor : (zIndex % 10 == 0 ? majorColor : minorColor);
            wrapper.RenderLine(vec3(center.x - extent, 0.0f, z), vec3(center.x + extent, 0.0f, z), zColor, true);
        }

    }

    Scene::Entity SceneWindow::CreateEntityFromTemplate(SceneHierarchyPanel::EntityTemplate type, Scene::Entity parent) {

        using Template = SceneHierarchyPanel::EntityTemplate;

        if (!scene.IsLoaded() || isPlaying)
            return Scene::Entity();

        if (!parent.IsValid())
            parent = scene->GetEntityByName("Root");

        const char* name = "Entity";
        switch (type) {
        case Template::PointLight: name = "Point light"; break;
        case Template::SpotLight: name = "Spot light"; break;
        case Template::DirectionalLight: name = "Directional light"; break;
        case Template::Camera: name = "Camera"; break;
        case Template::AudioSource: name = "Audio source"; break;
        case Template::Text: name = "Text"; break;
        default: break;
        }

        auto entity = scene->CreateEntity();
        entity.AddComponent<NameComponent>(name);

        if (!parent.HasComponent<HierarchyComponent>())
            parent.AddComponent<HierarchyComponent>();
        parent.GetComponent<HierarchyComponent>().AddChild(entity);

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
        default:
            break;
        }

        sceneHierarchyPanel.SelectEntity(entity);

        return entity;

    }

    void SceneWindow::FocusSelection() {

        if (!scene.IsLoaded() || !cameraEntity.IsValid())
            return;

        Volume::AABB bounds(vec3(std::numeric_limits<float>::max()), vec3(-std::numeric_limits<float>::max()));
        bool hasBounds = false;

        for (auto ecsEntity : sceneHierarchyPanel.selectedEntities) {
            Scene::Entity entity(ecsEntity, &scene->entityManager);
            if (!entity.IsValid())
                continue;

            // Meshes have proper world space bounds, everything else is treated as a small box around its origin
            auto meshComponent = entity.TryGetComponent<MeshComponent>();
            if (meshComponent) {
                bounds.Grow(meshComponent->aabb);
            }
            else {
                auto position = vec3(scene->GetGlobalMatrix(entity)[3]);
                bounds.Grow(Volume::AABB(position - vec3(0.5f), position + vec3(0.5f)));
            }
            hasBounds = true;
        }

        if (!hasBounds) {
            Notifications::Push({ .message = "Select an entity to focus it" });
            return;
        }

        auto& camera = cameraEntity.GetComponent<CameraComponent>();

        // Keep the view direction and back off until the bounding sphere fits into the field of view
        auto center = 0.5f * (bounds.min + bounds.max);
        auto radius = std::max(0.5f * glm::length(bounds.max - bounds.min), 0.5f);
        auto distance = radius / std::sin(glm::radians(camera.fieldOfView) * 0.5f);

        camera.location = center - camera.direction * distance;

    }

    void SceneWindow::ApplyBrush() {

        auto& terrainPanel = scenePropertiesPanel.terrainPanel;

        brushActive = terrainPanel.editingMode;
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

        bool mousePressed = ImGui::GetIO().MouseDown[ImGuiMouseButton_Right];
        bool brushTerrain = mousePressed && result.valid && terrainPanel.editingMode;

        if (!mousePressed)
            terrainFlattenHeight = Terrain::Terrain::invalidHeight;

        float brushRadius = terrainPanel.brushSize * scene->terrain->resolution;

        auto intersection = ray.Get(result.hitDistance);
        viewportPanel.primitiveBatchWrapper.RenderLineSphere(intersection, brushRadius, vec3(1.0f), true);

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
            }

            
        }

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
        }
        else if (selectionType == EntitySelectionType::Spline) {
            entityPropPanel.splineComponentPanel.selectControlPointIdx = selectedIdx;
        }
        
        sceneHierarchyPanel.selectedType = selectionType;

    }

}
