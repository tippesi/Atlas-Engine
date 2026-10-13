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
#include "ui/windows/LogWindow.h"
#include "ui/windows/ProfilerWindow.h"
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

        Ref<UI::SceneWindow> GetActiveSceneWindow() const;

        void SubscribeToResourceEvents();

        void SetDefaultWindowResolution();

        Input::MouseHandler mouseHandler;
        Input::KeyboardHandler keyboardHandler;
        Input::PlayerInput playerInput;

        UI::LogWindow logWindow = UI::LogWindow(true);
        UI::ContentBrowserWindow contentBrowserWindow = UI::ContentBrowserWindow(true);
        UI::ProfilerWindow profilerWindow = UI::ProfilerWindow(false);
        UI::GeometryBrushWindow geometryBrushWindow = UI::GeometryBrushWindow(false);

        ImguiExtension::PerformanceGraphPanel performanceGraphPanel;

        UI::CommandPalette commandPalette;
        bool showShortcutsWindow = false;

        std::vector<ResourceHandle<Scene::Scene>> waitToLoadScenes;
        std::vector<Ref<UI::SceneWindow>> sceneWindows;

        size_t activeSceneIdx = 0;

        ImGuiID upperDockNodeID;

        bool resetDockspaceLayout = false;
        bool playingMaximized = false;

    };

}