#include "EditorCommands.h"
#include "Workspace.h"
#include "Singletons.h"

#include "ui/panels/PopupPanels.h"

#include <ImGuizmo.h>

#include <map>

namespace Atlas::Editor {

    void EditorCommands::Register() {

        auto& config = Singletons::config;

        auto hasScene = [this]() {
            auto sceneWindow = workspace.GetActiveSceneWindow();
            return sceneWindow != nullptr && sceneWindow->scene.IsLoaded();
            };
        auto canEdit = [this, hasScene]() {
            return hasScene() && !workspace.GetActiveSceneWindow()->isPlaying;
            };

        // Where the shortcuts of a category work
        auto isSceneFocused = [this]() {
            auto sceneWindow = workspace.GetActiveSceneWindow();
            return sceneWindow && sceneWindow->inFocus;
            };
        auto isEditFocused = [isSceneFocused, canEdit]() { return isSceneFocused() && canEdit(); };
        auto isAssetFocused = [this]() { return workspace.activeAssetEditorWindow && workspace.activeAssetEditorWindow->inFocus; };
        auto isCodeFocused = [this]() { return workspace.activeCodeEditorWindow && workspace.activeCodeEditorWindow->inFocus; };
        auto getMaterialEditor = [this]() { return std::dynamic_pointer_cast<UI::MaterialEditorWindow>(workspace.activeAssetEditorWindow); };
        auto getMeshEditor = [this]() { return std::dynamic_pointer_cast<UI::MeshEditorWindow>(workspace.activeAssetEditorWindow); };

        const std::map<std::string, std::function<bool()>> focusFunctions = {
            { "Viewport", isEditFocused },
            { "Overlays", isEditFocused },
            { "Hierarchy", isSceneFocused },
            { "Properties", isSceneFocused },
            { "Asset editor", [this]() { return workspace.activeAssetEditorWindow && workspace.activeAssetEditorWindow->HasViewportFocus(); } },
            { "Material graph", [getMaterialEditor]() { auto window = getMaterialEditor(); return window && window->inFocus && window->graphPanel.isFocused; } },
            { "Animation timeline", [getMeshEditor]() { auto window = getMeshEditor(); return window && window->inFocus && window->timelinePanel.isFocused; } },
            { "Code", isCodeFocused }
        };

        auto add = [&](UI::Command command) {
            auto focusFunction = focusFunctions.find(command.category);
            if (!command.isFocused && focusFunction != focusFunctions.end())
                command.isFocused = focusFunction->second;
            Singletons::commandPalette->Register(command);
            };

        // File
        add({ .category = "File", .name = "New scene...",
            .action = []() { UI::PopupPanels::isNewScenePopupVisible = true; } });
        add({ .category = "File", .name = "Save project", .shortcut = ImGuiMod_Ctrl | ImGuiKey_S,
            .action = [this]() { workspace.projectSettingsWindow.Save(Singletons::project); },
            .isEnabled = [this]() { return workspace.projectSettingsWindow.hasUnsavedChanges; },
            .isFocused = [this]() { return workspace.projectSettingsWindow.inFocus; }, .whileTyping = true });
        add({ .category = "File", .name = "Save scene", .shortcut = ImGuiMod_Ctrl | ImGuiKey_S,
            .action = [this]() { workspace.GetActiveSceneWindow()->SaveScene(); }, .isEnabled = canEdit, .isFocused = isSceneFocused, .whileTyping = true });

        // Windows
        auto registerWindow = [&](const char* name, bool* show, ImGuiKeyChord shortcut = 0) {
            add({ .category = "Window", .name = std::string("Toggle ") + name, .shortcut = shortcut,
                .action = [show]() { *show = !*show; }, .isEnabled = [this]() { return !workspace.IsPlayingMaximized(); },
                .isChecked = [show]() { return *show; }, .whileTyping = true });
            };
        registerWindow("content browser", &workspace.contentBrowserWindow.show, ImGuiMod_Ctrl | ImGuiKey_B);
        registerWindow("log", &workspace.logWindow.show, ImGuiMod_Ctrl | ImGuiKey_J);
        auto registerPin = [&](const char* name, UI::DrawerWindow* drawer) {
            add({ .category = "Window", .name = std::string("Dock ") + name + " in layout",
                .action = [drawer]() { drawer->SetPinned(!drawer->pinned); },
                .isChecked = [drawer]() { return drawer->pinned; } });
            };
        registerPin("content browser", &workspace.contentBrowserWindow);
        registerPin("log", &workspace.logWindow);
        registerWindow("profiler", &workspace.profilerWindow.show);
        registerPin("profiler", &workspace.profilerWindow);
        registerWindow("project settings", &workspace.projectSettingsWindow.show);
        registerWindow("geometry brush", &workspace.geometryBrushWindow.show);

        // Renderer
        add({ .category = "Renderer", .name = "Toggle VSync",
            .action = [&config]() { config->vsync = !config->vsync; },
            .isChecked = [&config]() { return config->vsync; } });
        add({ .category = "Renderer", .name = "Toggle path tracer",
            .action = [&config]() { config->pathTrace = !config->pathTrace; },
            .isChecked = [&config]() { return config->pathTrace; } });

        // Viewport
        auto registerGizmoMode = [&](const char* name, ImGuiKeyChord shortcut, int32_t mode) {
            add({ .category = "Viewport", .name = name, .shortcut = shortcut,
                .action = [this, mode]() { workspace.GetActiveSceneWindow()->guizmo.mode = mode; },
                .isEnabled = canEdit,
                .isChecked = [this, mode]() { auto window = workspace.GetActiveSceneWindow(); return window && window->guizmo.mode == mode; } });
            };
        registerGizmoMode("Translate gizmo", ImGuiKey_1, ImGuizmo::OPERATION::TRANSLATE);
        registerGizmoMode("Rotate gizmo", ImGuiKey_2, ImGuizmo::OPERATION::ROTATE);
        registerGizmoMode("Scale gizmo", ImGuiKey_3, ImGuizmo::OPERATION::SCALE);

        add({ .category = "Viewport", .name = "Focus selection", .shortcut = ImGuiKey_F,
            .action = [this]() { workspace.GetActiveSceneWindow()->FocusSelection(); }, .isEnabled = canEdit });
        add({ .category = "Viewport", .name = "Drop selection to ground", .shortcut = ImGuiKey_End,
            .action = [this]() { workspace.GetActiveSceneWindow()->DropSelectionToGround(); }, .isEnabled = canEdit });
        add({ .category = "Properties", .name = "Search entity properties", .shortcut = ImGuiMod_Ctrl | ImGuiKey_F,
            .action = [this]() {
                auto& panel = workspace.GetActiveSceneWindow()->scenePropertiesPanel;
                panel.entityPropertiesPanel.focusSearch = true;
                ImGui::SetWindowFocus(panel.GetNameID());
                },
            .isEnabled = [this, hasScene]() { return hasScene() && workspace.GetActiveSceneWindow()->sceneHierarchyPanel.selectedEntity.IsValid(); } });
        auto isViewingCamera = [this, canEdit]() {
            return canEdit() && workspace.GetActiveSceneWindow()->viewedCameraEntity.IsValid();
            };
        add({ .category = "Viewport", .name = "View through selected camera",
            .action = [this]() { auto window = workspace.GetActiveSceneWindow(); window->ViewThroughCamera(window->sceneHierarchyPanel.selectedEntity); },
            .isEnabled = [this, canEdit]() {
                if (!canEdit())
                    return false;
                auto entity = workspace.GetActiveSceneWindow()->sceneHierarchyPanel.selectedEntity;
                return entity.IsValid() && entity.HasComponent<CameraComponent>();
                } });
        add({ .category = "Viewport", .name = "Apply view to camera",
            .action = [this]() { workspace.GetActiveSceneWindow()->ApplyViewToCamera(); }, .isEnabled = isViewingCamera });
        add({ .category = "Viewport", .name = "Back to editor camera",
            .action = [this]() { workspace.GetActiveSceneWindow()->ReturnToEditorCamera(); }, .isEnabled = isViewingCamera });
        add({ .category = "Viewport", .name = "Toggle grid", .shortcut = ImGuiKey_G,
            .action = [this]() { auto window = workspace.GetActiveSceneWindow(); window->showGrid = !window->showGrid; },
            .isEnabled = canEdit,
            .isChecked = [this]() { auto window = workspace.GetActiveSceneWindow(); return window && window->showGrid; } });
        add({ .category = "Viewport", .name = "Toggle physics",
            .action = [this]() { auto window = workspace.GetActiveSceneWindow(); window->showPhysics = !window->showPhysics; },
            .isEnabled = hasScene,
            .isChecked = [this]() { auto window = workspace.GetActiveSceneWindow(); return window && window->showPhysics; } });
        add({ .category = "Viewport", .name = "Toggle navigation mesh",
            .action = [this]() { auto& panel = workspace.GetActiveSceneWindow()->scenePropertiesPanel.navigationPanel; panel.showMesh = !panel.showMesh; },
            .isEnabled = hasScene,
            .isChecked = [this]() { auto window = workspace.GetActiveSceneWindow(); return window && window->scenePropertiesPanel.navigationPanel.showMesh; } });
        for (const auto& kind : UI::SceneOverlays::kinds) {
            auto bit = kind.bit;
            add({ .category = "Overlays", .name = kind.label,
                .shortcut = bit == UI::SceneOverlays::EntityIconsBit ? ImGuiKey_I : 0,
                .action = [this, bit]() { workspace.GetActiveSceneWindow()->overlays.shownKinds ^= bit; },
                .isEnabled = hasScene,
                .isChecked = [this, bit]() { auto window = workspace.GetActiveSceneWindow(); return window && (window->overlays.shownKinds & bit); } });
        }
        add({ .category = "Viewport", .name = "Toggle gizmo snapping",
            .action = [this]() { auto& guizmo = workspace.GetActiveSceneWindow()->guizmo; guizmo.snappingEnabled = !guizmo.snappingEnabled; },
            .isEnabled = canEdit,
            .isChecked = [this]() { auto window = workspace.GetActiveSceneWindow(); return window && window->guizmo.snappingEnabled; } });
        add({ .category = "Viewport", .name = "Toggle gizmo object space",
            .action = [this]() { auto& guizmo = workspace.GetActiveSceneWindow()->guizmo; guizmo.objectSpace = !guizmo.objectSpace; },
            .isEnabled = canEdit,
            .isChecked = [this]() { auto window = workspace.GetActiveSceneWindow(); return window && window->guizmo.objectSpace; } });

        auto registerVisualization = [&](const char* name, UI::ViewportVisualization visualization) {
            add({ .category = "Visualization", .name = name,
                .action = [this, visualization]() { workspace.GetActiveSceneWindow()->viewportPanel.visualization = visualization; },
                .isEnabled = hasScene,
                .isChecked = [this, visualization]() {
                    auto window = workspace.GetActiveSceneWindow();
                    return window && window->viewportPanel.visualization == visualization; } });
            };
        registerVisualization("Lit", UI::ViewportVisualization::Lit);
        registerVisualization("Base color", UI::ViewportVisualization::GBufferBaseColor);
        registerVisualization("Roughness/Metalness/Ao", UI::ViewportVisualization::GBufferRoughnessMetalnessAo);
        registerVisualization("Normals", UI::ViewportVisualization::GBufferNormals);
        registerVisualization("Depth", UI::ViewportVisualization::GBufferDepth);
        registerVisualization("Velocity", UI::ViewportVisualization::GBufferVelocity);
        registerVisualization("Reflections", UI::ViewportVisualization::Reflections);
        registerVisualization("Clouds", UI::ViewportVisualization::Clouds);
        registerVisualization("SSGI", UI::ViewportVisualization::SSGI);

        // Views, with Ctrl from the other side
        for (size_t i = 0; i < std::size(UI::SceneWindow::viewDirections); i++) {
            const ImGuiKeyChord shortcuts[] = { ImGuiKey_Keypad7, ImGuiKey_Keypad1, ImGuiKey_Keypad3, ImGuiMod_Ctrl | ImGuiKey_Keypad7,
                ImGuiMod_Ctrl | ImGuiKey_Keypad1, ImGuiMod_Ctrl | ImGuiKey_Keypad3 };
            const auto& direction = UI::SceneWindow::viewDirections[i];
            add({ .category = "Viewport", .name = std::string("Look from the ") + direction.name, .shortcut = shortcuts[i],
                .action = [this, rotation = direction.rotation]() { workspace.GetActiveSceneWindow()->SetViewRotation(rotation); },
                .isEnabled = canEdit });
        }
        add({ .category = "Viewport", .name = "Toggle orthographic view", .shortcut = ImGuiKey_Keypad5,
            .action = [this]() { auto window = workspace.GetActiveSceneWindow(); window->SetOrthographic(!window->orthographicView.active); },
            .isEnabled = canEdit,
            .isChecked = [this]() { auto window = workspace.GetActiveSceneWindow(); return window && window->orthographicView.active; } });
        for (int32_t i = 1; i <= UI::SceneWindow::maxBookmarks; i++) {
            auto key = ImGuiKeyChord(ImGuiKey_0) + i;
            add({ .category = "Viewport", .name = "Go to camera bookmark " + std::to_string(i), .shortcut = ImGuiMod_Ctrl | key,
                .action = [this, i]() { workspace.GetActiveSceneWindow()->GoToBookmark(i); },
                .isEnabled = [this, canEdit, i]() { return canEdit() && workspace.GetActiveSceneWindow()->bookmarks.contains(i); } });
            add({ .category = "Viewport", .name = "Set camera bookmark " + std::to_string(i), .shortcut = ImGuiMod_Ctrl | ImGuiMod_Shift | key,
                .action = [this, i]() { workspace.GetActiveSceneWindow()->SetBookmark(i); }, .isEnabled = canEdit });
        }

        // Hierarchy
        auto hasSelection = [this, hasScene]() {
            return hasScene() && !workspace.GetActiveSceneWindow()->sceneHierarchyPanel.selectedEntities.empty();
            };
        add({ .category = "Hierarchy", .name = "Duplicate selection", .shortcut = ImGuiMod_Ctrl | ImGuiKey_D,
            .action = [this]() { auto window = workspace.GetActiveSceneWindow(); window->sceneHierarchyPanel.DuplicateSelectedEntity(window->scene.Get()); },
            .isEnabled = hasSelection });
        add({ .category = "Hierarchy", .name = "Delete selection", .shortcut = ImGuiKey_Delete,
            .action = [this]() { auto window = workspace.GetActiveSceneWindow(); window->sceneHierarchyPanel.DeleteSelectedEntities(window->scene.Get()); },
            .isEnabled = hasSelection });
        add({ .category = "Hierarchy", .name = "Rename selected entity", .shortcut = ImGuiKey_F2,
            .action = [this]() { auto& panel = workspace.GetActiveSceneWindow()->sceneHierarchyPanel; panel.StartRename(panel.selectedEntity); },
            .isEnabled = [this, hasScene]() { return hasScene() && workspace.GetActiveSceneWindow()->sceneHierarchyPanel.selectedEntity.IsValid(); } });

        // Create
        auto registerCreate = [&](const char* name, UI::SceneHierarchyPanel::EntityTemplate type) {
            add({ .category = "Create", .name = name,
                .action = [this, type]() { workspace.GetActiveSceneWindow()->CreateEntityFromTemplate(type); },
                .isEnabled = canEdit });
            };
        registerCreate("Empty entity", UI::SceneHierarchyPanel::EntityTemplate::Empty);
        registerCreate("Point light", UI::SceneHierarchyPanel::EntityTemplate::PointLight);
        registerCreate("Spot light", UI::SceneHierarchyPanel::EntityTemplate::SpotLight);
        registerCreate("Directional light", UI::SceneHierarchyPanel::EntityTemplate::DirectionalLight);
        registerCreate("Camera", UI::SceneHierarchyPanel::EntityTemplate::Camera);
        registerCreate("Audio source", UI::SceneHierarchyPanel::EntityTemplate::AudioSource);
        registerCreate("Text", UI::SceneHierarchyPanel::EntityTemplate::Text);
        registerCreate("Fog volume", UI::SceneHierarchyPanel::EntityTemplate::FogVolume);

        // Scene
        auto isPlaying = [this, hasScene]() { return hasScene() && workspace.GetActiveSceneWindow()->isPlaying; };
        add({ .category = "Scene", .name = "Play",
            .action = [this]() { workspace.GetActiveSceneWindow()->StartPlaying(); }, .isEnabled = canEdit });
        add({ .category = "Scene", .name = "Pause or resume playing",
            .action = [this]() { workspace.GetActiveSceneWindow()->TogglePause(); },
            .isEnabled = isPlaying });
        add({ .category = "Scene", .name = "Stop playing", .shortcut = ImGuiMod_Ctrl | ImGuiKey_Q,
            .action = [this]() { workspace.GetActiveSceneWindow()->StopPlaying(); },
            .isEnabled = isPlaying, .whileTyping = true });
        add({ .category = "Scene", .name = "Play from view", .shortcut = ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_P,
            .action = [this]() { workspace.GetActiveSceneWindow()->StartPlaying(true); }, .isEnabled = canEdit });
        add({ .category = "Scene", .name = "Step one frame", .shortcut = ImGuiKey_F10,
            .action = [this]() { workspace.GetActiveSceneWindow()->StepFrame(); }, .isEnabled = isPlaying });

        // Undo of the asset editor or scene in front, code and text fields have their own
        auto isHistoryFocused = [this, isEditFocused, isAssetFocused]() {
            return isEditFocused() || isAssetFocused() || workspace.projectSettingsWindow.inFocus;
            };
        add({ .category = "Edit", .name = "Undo", .shortcut = ImGuiMod_Ctrl | ImGuiKey_Z,
            .action = [this]() { workspace.Undo(); },
            .isEnabled = [this]() { return workspace.CanUndo(); }, .isFocused = isHistoryFocused, .repeat = true });
        add({ .category = "Edit", .name = "Redo", .shortcut = ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z, .alternative = ImGuiMod_Ctrl | ImGuiKey_Y,
            .action = [this]() { workspace.Redo(); },
            .isEnabled = [this]() { return workspace.CanRedo(); }, .isFocused = isHistoryFocused, .repeat = true });

        // Material graph of the material editor in front
        auto hasMaterialGraph = [getMaterialEditor]() {
            auto window = getMaterialEditor();
            return window && window->material.IsLoaded() && window->material->HasGraph();
            };
        add({ .category = "Material graph", .name = "Add node", .shortcut = ImGuiKey_Space,
            .action = [getMaterialEditor]() { getMaterialEditor()->graphPanel.OpenAddMenu(); }, .isEnabled = hasMaterialGraph });
        add({ .category = "Material graph", .name = "Frame nodes", .shortcut = ImGuiKey_F,
            .action = [getMaterialEditor]() { getMaterialEditor()->graphPanel.FrameView(); }, .isEnabled = hasMaterialGraph });
        add({ .category = "Material graph", .name = "Arrange nodes",
            .action = [getMaterialEditor]() { auto window = getMaterialEditor(); window->graphPanel.Arrange(*window->GetGraph().Get()); },
            .isEnabled = hasMaterialGraph });
        add({ .category = "Material graph", .name = "Show generated code",
            .action = [getMaterialEditor]() { getMaterialEditor()->graphPanel.ShowGeneratedCode(); }, .isEnabled = hasMaterialGraph });
        add({ .category = "Material graph", .name = "Select all nodes", .shortcut = ImGuiMod_Ctrl | ImGuiKey_A,
            .action = [getMaterialEditor]() { auto window = getMaterialEditor(); window->graphPanel.SelectAll(*window->GetGraph().Get()); },
            .isEnabled = hasMaterialGraph });
        add({ .category = "Material graph", .name = "Delete selected nodes", .shortcut = ImGuiKey_Delete, .alternative = ImGuiKey_Backspace,
            .action = [getMaterialEditor]() { auto window = getMaterialEditor(); window->graphPanel.DeleteSelection(*window->GetGraph().Get()); },
            .isEnabled = hasMaterialGraph });
        add({ .category = "Material graph", .name = "Copy selected nodes", .shortcut = ImGuiMod_Ctrl | ImGuiKey_C,
            .action = [getMaterialEditor]() { auto window = getMaterialEditor(); window->graphPanel.CopySelection(*window->GetGraph().Get()); },
            .isEnabled = hasMaterialGraph });
        add({ .category = "Material graph", .name = "Paste nodes", .shortcut = ImGuiMod_Ctrl | ImGuiKey_V,
            .action = [getMaterialEditor]() { auto window = getMaterialEditor(); window->graphPanel.Paste(*window->GetGraph().Get()); },
            .isEnabled = hasMaterialGraph });
        add({ .category = "Material graph", .name = "Duplicate selected nodes", .shortcut = ImGuiMod_Ctrl | ImGuiKey_D,
            .action = [getMaterialEditor]() { auto window = getMaterialEditor(); window->graphPanel.DuplicateSelection(*window->GetGraph().Get()); },
            .isEnabled = hasMaterialGraph });
        add({ .category = "Material graph", .name = "Frame selected nodes", .shortcut = ImGuiMod_Ctrl | ImGuiKey_G,
            .action = [getMaterialEditor]() { auto window = getMaterialEditor(); window->graphPanel.FrameSelection(*window->GetGraph().Get()); },
            .isEnabled = hasMaterialGraph });
        add({ .category = "Material graph", .name = "Create subgraph from selection", .shortcut = ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_G,
            .action = [getMaterialEditor]() { getMaterialEditor()->graphPanel.RequestSubgraph(); },
            .isEnabled = [getMaterialEditor, hasMaterialGraph]() {
                return hasMaterialGraph() && getMaterialEditor()->graphPanel.CanCreateSubgraph(getMaterialEditor()->GetGraph()); } });
        add({ .category = "Material graph", .name = "Dissolve selected subgraphs",
            .action = [getMaterialEditor]() { auto window = getMaterialEditor(); window->graphPanel.DissolveSelection(*window->GetGraph().Get()); },
            .isEnabled = [getMaterialEditor, hasMaterialGraph]() {
                return hasMaterialGraph() && getMaterialEditor()->graphPanel.HasSelectedSubgraph(*getMaterialEditor()->GetGraph().Get()); } });
        add({ .category = "Material graph", .name = "Toggle maximized", .shortcut = ImGuiKey_F11,
            .action = [getMaterialEditor]() { getMaterialEditor()->graphPanel.ToggleMaximized(); }, .isEnabled = hasMaterialGraph });

        // Code window in front
        auto hasCodeWindow = [this]() { return workspace.activeCodeEditorWindow != nullptr; };
        add({ .category = "Code", .name = "Save file", .shortcut = ImGuiMod_Ctrl | ImGuiKey_S,
            .action = [this]() { workspace.activeCodeEditorWindow->Save(); }, .isEnabled = hasCodeWindow, .whileTyping = true });

        // The code being edited, also of a material graph, or the code window in front
        auto getCodeEditor = [this]() -> UI::CodeEditor* {
            if (auto codeEditor = UI::CodeEditor::GetFocused())
                return codeEditor;
            return workspace.activeCodeEditorWindow ? &workspace.activeCodeEditorWindow->codeEditor : nullptr;
            };
        auto registerCode = [&](const char* name, ImGuiKeyChord shortcut, ImGuiKeyChord alternative, void (UI::CodeEditor::* action)(), bool repeat) {
            add({ .category = "Code", .name = name, .shortcut = shortcut, .alternative = alternative,
                .action = [getCodeEditor, action]() { (getCodeEditor()->*action)(); },
                .isEnabled = [getCodeEditor]() { return getCodeEditor() != nullptr; },
                .isFocused = []() { return UI::CodeEditor::GetFocused() != nullptr; }, .repeat = repeat, .whileTyping = true });
            };
        registerCode("Undo", ImGuiMod_Ctrl | ImGuiKey_Z, 0, &UI::CodeEditor::Undo, true);
        registerCode("Redo", ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z, ImGuiMod_Ctrl | ImGuiKey_Y, &UI::CodeEditor::Redo, true);
        registerCode("Find", ImGuiMod_Ctrl | ImGuiKey_F, 0, &UI::CodeEditor::OpenSearch, false);
        registerCode("Replace", ImGuiMod_Ctrl | ImGuiKey_H, 0, &UI::CodeEditor::OpenReplace, false);
        registerCode("Find next", ImGuiKey_F3, 0, &UI::CodeEditor::FindNextMatch, true);
        registerCode("Find previous", ImGuiMod_Shift | ImGuiKey_F3, 0, &UI::CodeEditor::FindPreviousMatch, true);

        // Asset editor in front
        auto hasAssetEditor = [this]() { return workspace.activeAssetEditorWindow != nullptr; };
        add({ .category = "Asset editor", .name = "Save asset", .shortcut = ImGuiMod_Ctrl | ImGuiKey_S,
            .action = [this]() { workspace.activeAssetEditorWindow->Save(); workspace.activeAssetEditorWindow->hasUnsavedChanges = false; },
            .isEnabled = hasAssetEditor, .isFocused = isAssetFocused, .whileTyping = true });
        add({ .category = "Asset editor", .name = "Focus asset", .shortcut = ImGuiKey_F,
            .action = [this]() { workspace.activeAssetEditorWindow->FocusAsset(); }, .isEnabled = hasAssetEditor });
        add({ .category = "Asset editor", .name = "Toggle grid", .shortcut = ImGuiKey_G,
            .action = [this]() { workspace.activeAssetEditorWindow->showGrid = !workspace.activeAssetEditorWindow->showGrid; }, .isEnabled = hasAssetEditor });

        // Animation timeline of the mesh editor in front
        add({ .category = "Animation timeline", .name = "Save animation",
            .action = [getMeshEditor]() { getMeshEditor()->timelinePanel.saveRequested = true; },
            .isEnabled = [getMeshEditor]() { return getMeshEditor() != nullptr; } });
        add({ .category = "Animation timeline", .name = "Delete selected key", .shortcut = ImGuiKey_Delete,
            .action = [getMeshEditor]() { getMeshEditor()->timelinePanel.deleteKeyRequested = true; },
            .isEnabled = [getMeshEditor]() { auto window = getMeshEditor(); return window && window->timelinePanel.HasSelectedKey(); } });

        // Help
        add({ .category = "Help", .name = "Command palette", .shortcut = ImGuiMod_Ctrl | ImGuiKey_P,
            .action = []() { Singletons::commandPalette->Open(); }, .whileTyping = true });

    }

    void EditorCommands::RenderShortcutsWindow(bool* show) {

        if (!*show)
            return;

        auto viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(38.0f * ImGui::GetFontSize(), 0.8f * viewport->WorkSize.y), ImGuiCond_Appearing);

        auto flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
        if (ImGui::Begin("Keyboard shortcuts", show, flags)) {
            ImGui::TextDisabled("Click on a shortcut to change it");
            Singletons::commandPalette->RenderShortcuts();

            const std::pair<const char*, const char*> mouseRows[] = {
                { "Left mouse drag", "Rotate the camera" },
                { "W / A / S / D / Q / E", "Move the camera" },
                { "Hold Ctrl", "Lock camera movement" },
                { "Mouse wheel", "Change the camera speed, zoom in an orthographic view" },
                { "Right click", "Select an entity or its icon (Shift to add)" },
                { "Hold right mouse", "Paint with the active terrain brush" },
                { "Double click", "Focus an entity, open a folder or scene" },
                { "Drag and drop", "Reparent, or create a prefab in the content browser" },
                { "Ctrl + mouse wheel", "Size of the icons in the content browser, zoom of the timeline" },
                { "Drag a pin", "Connect material nodes, drag an input to move its link" },
                { "Alt+Click on a link", "Remove a link between material nodes" },
                { "Esc", "Close the open drawer" }
            };
            ImGui::SeparatorText("Mouse");
            if (ImGui::BeginTable("Mouse", 2, ImGuiTableFlags_SizingStretchProp)) {
                for (const auto& [keys, description] : mouseRows) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(keys);
                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("%s", description);
                }
                ImGui::EndTable();
            }
        }
        ImGui::End();

    }

}
