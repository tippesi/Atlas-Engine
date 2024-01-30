#pragma once

#include <EngineInstance.h>
#include <input/Mouse.h>
#include <input/Keyboard.h>
#include <input/Controller.h>
#include <input/Touch.h>
#include <loader/ModelLoader.h>
#include <ImguiExtension/ImguiWrapper.h>

#include <renderer/PathTracingRenderer.h>

#include "ui/windows/ContentBrowserWindow.h"
#include "ui/windows/SceneWindow.h"
#include "ui/windows/LogWindow.h"

#define WINDOW_FLAGS AE_WINDOW_RESIZABLE | AE_WINDOW_HIGH_DPI

namespace Atlas::Editor {

    class App : public Atlas::EngineInstance {

        template<class T>
        using Ref = Atlas::Ref<T>;

    public:
        App() : EngineInstance("Atlas Engine Editor", 1920, 1080, WINDOW_FLAGS) {}

        virtual void LoadContent() final;

        virtual void UnloadContent() final;

        virtual void Update(float deltaTime) final;

        virtual void Render(float deltaTime) final;

    private:
        void SetupMainDockspace(ImGuiID dsID);

        void SubscribeToResourceEvents();

        UI::LogWindow logWindow;
        UI::ContentBrowserWindow contentBrowserWindow;

        std::vector<UI::SceneWindow> sceneWindows;
        std::vector<std::string> windowsToAddToNode;

        ImGuiID upperDockNodeID;

        // Enable right now, with that all new scenes will be docked correctly (but layout will be reset)
        bool resetDockspaceLayout = true;

    };

}