#pragma once

#include <EngineInstance.h>
#include <input/Mouse.h>
#include <input/Keyboard.h>
#include <input/PlayerInput.h>
#include <input/Controller.h>
#include <input/Touch.h>
#include <loader/ModelImporter.h>
#include <ImguiExtension/ImguiWrapper.h>

#include <renderer/PathTracingRenderer.h>

#include "ui/windows/ContentBrowserWindow.h"
#include "ui/windows/SceneWindow.h"
#include "ui/windows/MeshEditorWindow.h"
#include "ui/windows/MaterialEditorWindow.h"
#include "ui/windows/LogWindow.h"
#include "ui/windows/ProfilerWindow.h"
#include "ui/windows/SettingsWindow.h"
#include "ui/windows/GeometryBrushWindow.h"
#include "ui/popups/CommandPalette.h"

#define WINDOW_FLAGS AE_WINDOW_RESIZABLE | AE_WINDOW_HIGH_DPI

namespace Atlas::Editor {

    class App : public EngineInstance {

    public:
        App() : EngineInstance("Atlas Engine Editor", 1920, 1080, WINDOW_FLAGS) {}

        virtual void LoadContent() final;

        virtual void UnloadContent() final;

        virtual void Update(float deltaTime) final;

        virtual void Render(float deltaTime) final;

    private:
        void RenderSceneMaximized();
        
        void SetupMainDockspace(ImGuiID dsID);

        void RenderEmptyState(ImGuiID dsID);

        void ApplyTheme();

        void RenderStatusBar();

        // Undo and redo act on the asset editor or scene in front
        void Undo();

        void Redo();

        bool CanUndo() const;

        bool CanRedo() const;

        const UndoHistory* GetActiveHistory() const;

        bool RenderDrawerButton(UI::DrawerWindow& drawer, IconType icon, const char* label, const char* shortcut,
            int32_t errorCount = 0, int32_t warningCount = 0);

        // Clickable items of the status bar carry a background, such that they stand out from plain information
        bool RenderStatusChip(const char* id, IconType icon, const char* text, ImVec4 color,
            ImVec4 background, ImVec4 backgroundHovered, float maxWidth = FLT_MAX, float trailingWidth = 0.0f);

        float GetCapCenteredTextY(float centerY);

        void UpdateDrawers();

        // Pinned drawers share one area at the bottom of the layout, which is split off before the dockspace is updated
        void PrepareDrawerDocking(ImGuiID dsID);

        void DockPinnedDrawer(UI::DrawerWindow& drawer);

        void RenderRecentScenesMenu();

        void RenderShortcutsWindow();

        void RegisterCommands();

        // Opens meshes and materials in their editors and prefabs in a scene window, animations play in the last
        // focused mesh editor
        void OpenInAssetEditor(const std::string& path);

        // Asset editors and prefabs open as a tab next to the scenes
        void DockNextToScenes(UI::Window& window);

        Ref<UI::SceneWindow> GetActiveSceneWindow() const;

        void SubscribeToResourceEvents();

        void SetDefaultWindowResolution();

        Input::MouseHandler mouseHandler;
        Input::KeyboardHandler keyboardHandler;
        Input::PlayerInput playerInput;

        UI::LogWindow logWindow = UI::LogWindow(false);
        UI::ContentBrowserWindow contentBrowserWindow = UI::ContentBrowserWindow(false);
        UI::ProfilerWindow profilerWindow = UI::ProfilerWindow(false);
        UI::SettingsWindow settingsWindow = UI::SettingsWindow(false);
        UI::GeometryBrushWindow geometryBrushWindow = UI::GeometryBrushWindow(false);

        ImguiExtension::PerformanceGraphPanel performanceGraphPanel;

        UI::CommandPalette commandPalette;
        bool showShortcutsWindow = false;

        std::vector<ResourceHandle<Scene::Scene>> waitToLoadScenes;
        std::vector<Ref<UI::SceneWindow>> sceneWindows;
        std::vector<Ref<UI::AssetEditorWindow>> assetEditorWindows;
        Ref<UI::AssetEditorWindow> activeAssetEditorWindow;

        size_t activeSceneIdx = 0;

        ImGuiID drawerDockNodeID = 0;

        float drawerSpringTime = 0.0f;
        float backgroundJobTime = 0.0f;

        std::vector<std::string> emptyStateScenes;
        bool emptyStateVisible = false;

        bool resetDockspaceLayout = false;
        bool dockLoadingScenes = false;
        bool playingMaximized = false;

    };

}