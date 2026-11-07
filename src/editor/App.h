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

        void RenderStatusBar();

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

        UI::LogWindow logWindow = UI::LogWindow(true);
        UI::ContentBrowserWindow contentBrowserWindow = UI::ContentBrowserWindow(true);
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

        ImGuiID upperDockNodeID;

        bool resetDockspaceLayout = false;
        bool playingMaximized = false;

    };

}