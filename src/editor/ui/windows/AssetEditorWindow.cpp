#include "AssetEditorWindow.h"

#include "../../DataCreator.h"
#include "../../Singletons.h"
#include "../../Theme.h"

#include "Clock.h"

#include <imgui_internal.h>
#include <cstdarg>

namespace Atlas::Editor::UI {

    AssetEditorWindow::AssetEditorWindow(const std::string& name, bool show) : Window(name, show) {

        // Editors are opened and closed often, they always start with the default layout
        resetDockingLayout = true;

        RegisterViewportOverlay();

    }

    void AssetEditorWindow::Render() {

        ImGui::SetNextWindowSize(ImVec2(1280, 720), ImGuiCond_FirstUseEver);
        if (!Begin())
            return;

        // The layout depends on the asset, e.g. skinned meshes have more panels
        if (!IsReady()) {
            ImGui::TextDisabled("Loading...");
            End();
            return;
        }

        ImGuiID dsID = ImGui::GetID(dockSpaceNameID.c_str());

        if (!ImGui::DockBuilderGetNode(dsID) || resetDockingLayout) {
            // New windows start tiny, the panels would keep that size
            auto windowSize = ImGui::GetWindowSize();
            if (windowSize.x < 200.0f || windowSize.y < 200.0f) {
                End();
                return;
            }

            ImGui::DockBuilderRemoveNode(dsID);
            ImGui::DockBuilderAddNode(dsID, ImGuiDockNodeFlags_DockSpace);

            ImGui::DockBuilderSetNodeSize(dsID, windowSize);

            ImGuiID dockIdSide = 0, dockIdBottom = 0, dockIdMiddle = dsID, dockIdRight;
            if (HasSidePanel())
                ImGui::DockBuilderSplitNode(dockIdMiddle, ImGuiDir_Left, 0.18f, &dockIdSide, &dockIdMiddle);
            ImGui::DockBuilderSplitNode(dockIdMiddle, ImGuiDir_Right, 0.3f, &dockIdRight, &dockIdMiddle);
            if (HasBottomPanel())
                ImGui::DockBuilderSplitNode(dockIdMiddle, ImGuiDir_Down, GetBottomPanelSize(), &dockIdBottom, &dockIdMiddle);

            ImGui::DockBuilderDockWindow(viewportPanel.GetNameID(), dockIdMiddle);
            ImGui::DockBuilderDockWindow(propertiesPanel.GetNameID(), dockIdRight);
            DockPanels(dockIdSide, dockIdBottom);
            ImGui::DockBuilderFinish(dsID);

            resetDockingLayout = false;
        }

        // Panels which are alone in their node don't need a tab bar
        ImGui::DockSpace(dsID, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_AutoHideTabBar);

        inFocus = ImGui::IsWindowFocused() || propertiesPanel.isFocused || viewportPanel.isFocused || IsPanelFocused();
        if (inFocus)
            lastFocusTime = Clock::Get();

        if (!scene)
            CreatePreview();

        RenderPanels();

        ImGui::Begin(propertiesPanel.GetNameID());

        if (ImGui::IsDragDropActive() && ImGui::IsWindowHovered() && !ImGui::IsWindowFocused())
            ImGui::SetWindowFocus();

        propertiesPanel.isFocused = ImGui::IsWindowFocused();

        RenderProperties();

        ImGui::End();

        bool controlDown;
#ifdef AE_OS_MACOS
        controlDown = ImGui::IsKeyDown(ImGuiKey_LeftSuper);
#else
        controlDown = ImGui::IsKeyDown(ImGuiKey_LeftCtrl);
#endif
        bool typing = ImGui::GetIO().WantTextInput;
        if (inFocus && controlDown && ImGui::IsKeyPressed(ImGuiKey_S, false))
            Save();
        if (inFocus && history.HandleShortcuts())
            lastState = SaveState();
        // Only the window the user works in has edits, the last click might have moved the focus already
        if (inFocus || wasInFocus || lastState.is_null())
            UpdateHistory();
        wasInFocus = inFocus;
        auto viewportShortcuts = inFocus && !controlDown && !typing && AllowsViewportShortcuts();
        if (viewportShortcuts && ImGui::IsKeyPressed(ImGuiKey_F, false))
            FocusAsset();
        if (viewportShortcuts && ImGui::IsKeyPressed(ImGuiKey_G, false))
            showGrid = !showGrid;

        {
            auto& camera = cameraEntity.GetComponent<CameraComponent>();
            camera.aspectRatio = float(viewportPanel.viewport->width) / std::max(float(viewportPanel.viewport->height), 1.0f);

            // Path tracing needs history while ray tracing
            scene->rayTracingWorld->includeObjectHistory = Singletons::config->pathTrace;
            scene->Update(Clock::GetDelta(), cameraEntity);

            // The camera direction is only known after the first update
            if (focusRequested) {
                FocusAsset();
                focusRequested = false;
            }

            if (showGrid)
                viewportPanel.RenderGrid(camera);
            // The scene keeps the bounds of meshes up to date, e.g. with the pose of an animation
            if (showBounds) {
                auto bounds = scene->GetHierarchyBounds(entity).value_or(GetBounds());
                viewportPanel.primitiveBatchWrapper.GetBatch(true)->AddLineAABB(bounds, vec3(1.0f, 1.0f, 0.0f));
            }

            RenderOverlays();
        }

        viewportPanel.Render(scene);
        AcceptViewportDrop();

        End();

    }

    void AssetEditorWindow::FocusAsset() {

        if (!scene)
            return;

        auto& camera = cameraEntity.GetComponent<CameraComponent>();

        // Keep the view direction and back off until the bounding sphere fits into the field of view
        auto bounds = GetBounds();
        auto center = 0.5f * (bounds.min + bounds.max);
        auto radius = std::max(0.5f * glm::length(bounds.max - bounds.min), 0.1f);
        auto distance = radius / std::sin(glm::radians(camera.fieldOfView) * 0.5f);

        camera.location = center - camera.direction * distance;

    }

    void AssetEditorWindow::Undo() {

        if (history.Undo())
            lastState = SaveState();

    }

    void AssetEditorWindow::Redo() {

        if (history.Redo())
            lastState = SaveState();

    }

    void AssetEditorWindow::UpdateHistory() {

        auto interactionFinished = history.IsInteractionFinished();

        if (lastState.is_null()) {
            lastState = SaveState();
            return;
        }

        if (!interactionFinished)
            return;

        auto state = SaveState();
        if (state == lastState)
            return;

        auto before = lastState;
        history.Push({
            .name = "Edit " + Common::Path::GetFileName(GetAssetPath()),
            .undo = [this, before]() { RestoreState(before); },
            .redo = [this, state]() { RestoreState(state); }
        });
        lastState = state;

    }

    void AssetEditorWindow::RenderHeading(const char* saveTooltip) {

        auto& style = ImGui::GetStyle();
        auto path = GetAssetPath();

        // Heading with the save button on the right
        auto saveLabel = "Save";
        auto saveWidth = ImGui::CalcTextSize(saveLabel).x + 4.0f * style.FramePadding.x;
        auto headingStart = ImGui::GetCursorPos();

        // The name is cut off before the save button
        auto headingMin = ImGui::GetCursorScreenPos();
        ImGui::PushClipRect(headingMin, ImVec2(headingMin.x + ImGui::GetContentRegionAvail().x - saveWidth - style.ItemSpacing.x,
            headingMin.y + 2.0f * ImGui::GetTextLineHeight()), true);
        ImGui::SetWindowFontScale(1.5f);
        ImGui::TextUnformatted(Common::Path::GetFileName(path).c_str());
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopClipRect();
        ImGui::TextColored(Theme::TextMuted(), "%s", path.c_str());
        auto headingEnd = ImGui::GetCursorPos();

        ImGui::SetCursorPos(ImVec2(ImGui::GetWindowContentRegionMax().x - saveWidth, headingStart.y));
        Theme::PushPrimaryButton();
        if (ImGui::Button(saveLabel, ImVec2(saveWidth, 0.0f)))
            Save();
        Theme::PopPrimaryButton();
        ImGui::SetItemTooltip("%s", saveTooltip);
        ImGui::SetCursorPos(headingEnd);

        ImGui::Separator();

    }

    void AssetEditorWindow::CreatePreview() {

        auto bounds = GetBounds();
        auto extent = std::max(glm::length(bounds.max - bounds.min), 1.0f);
        scene = DataCreator::CreateScene(name, vec3(-4.0f * extent), vec3(4.0f * extent), 4);
        scene->irradianceVolume->SetAABB(Volume::AABB(vec3(-2.0f * extent), vec3(2.0f * extent)));

        // Light the side the camera looks at first
        auto lightSubset = scene->entityManager.GetSubset<LightComponent>();
        for (auto lightEntity : lightSubset)
            lightSubset.Get(lightEntity).properties.directional.direction = vec3(-0.5f, -1.0f, -1.0f);

        entity = CreatePreviewEntity();

        cameraEntity = scene->CreateEntity();
        auto& camera = cameraEntity.AddComponent<CameraComponent>(47.0f, 2.0f, 0.01f * extent, 100.0f * extent,
            vec3(0.0f), vec2(glm::pi<float>() + 0.5f, -0.3f));
        camera.isMain = true;

        cameraMovementSpeed = extent;
        focusRequested = true;

    }

    void AssetEditorWindow::RegisterViewportOverlay() {

        viewportPanel.DrawMenuBar([&]() {

            const float margin = ViewportPanel::toolbarMargin;
            auto region = ImGui::GetContentRegionAvail();

            // Mouse wheel over the viewport changes the camera speed, as in the scene window
            const auto& io = ImGui::GetIO();
            if (ImGui::IsWindowHovered() && io.MouseWheel != 0.0f && !io.KeyCtrl)
                cameraMovementSpeed = std::clamp(cameraMovementSpeed * std::pow(1.25f, io.MouseWheel), 0.01f, 500.0f);

            viewportPanel.BeginToolbarGroup(margin, 1);
            if (viewportPanel.ToolbarButton("Focus button", IconType::Mesh, false, scene != nullptr, "Focus (F)"))
                FocusAsset();

            auto uvMin = ImVec2(0.1f, 0.1f);
            auto uvMax = ImVec2(0.9f, 0.9f);
            viewportPanel.BeginToolbarGroup(region.x - margin - viewportPanel.GetToolbarGroupWidth(2), 2);
            if (viewportPanel.ToolbarButton("Viewport settings button", IconType::Settings, false, scene != nullptr,
                "Viewport settings", uvMin, uvMax))
                ImGui::OpenPopup("Viewport settings");
            if (viewportPanel.ToolbarButton("Visualization settings button", IconType::Visibility,
                viewportPanel.visualization != ViewportVisualization::Lit, scene != nullptr, "Visualization", uvMin, uvMax))
                ImGui::OpenPopup("Visualization settings");

            if (ImGui::BeginPopup("Viewport settings")) {
                ImGui::SeparatorText("Overlays");
                ImGui::Checkbox("Show grid (G)", &showGrid);
                ImGui::Checkbox("Show bounds", &showBounds);
                RenderViewportSettings();

                ImGui::SeparatorText("Camera");
                ImGui::DragFloat("Movement speed", &cameraMovementSpeed, 0.01f, 0.01f, 500.0f);
                ImGui::SetItemTooltip("The mouse wheel over the viewport changes the speed as well");
                if (cameraEntity.IsValid()) {
                    auto& camera = cameraEntity.GetComponent<CameraComponent>();
                    ImGui::DragFloat("Exposure", &camera.exposure, 0.1f, 0.01f, 180.0f);
                    ImGui::DragFloat("Field of view", &camera.fieldOfView, 0.1f, 1.0f, 180.0f);
                }

                ImGui::EndPopup();
            }

            viewportPanel.RenderVisualizationPopup();

            // Don't leave the cursor on the toolbar line
            ImGui::NewLine();

            });

    }

}
