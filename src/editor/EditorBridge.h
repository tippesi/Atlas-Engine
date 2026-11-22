#pragma once

#include "ui/windows/SceneWindow.h"

#include <McpServer.h>
#include <WindowShot.h>
#include <project/Project.h>

#include <functional>

namespace Atlas::Editor {

    // The MCP server of the editor (Bridge::McpServer): Lua in the scene of the active scene window, one undo step per
    // call, screenshots of the editor window, undo and the job tools of the bridge. Agents connect to
    // http://127.0.0.1:<port>/mcp, the first program which starts gets the port
    class EditorBridge {

    public:
        EditorBridge(const Ref<Project>& project, int32_t port, const std::function<Ref<UI::SceneWindow>()>& getSceneWindow);

        ~EditorBridge();

        // Called after Lua of the lua tool changed the project, with the project before
        std::function<void(const Project& before)> onProjectChange;

        // Fills the table Editor of the lua tool
        std::function<void(Scripting::Binder& editor)> addEditorFunctions;

        // Runs the tool calls which arrived since the last frame, on the main thread
        void Update();

        // After the interface is rendered, copies the window for a screenshot
        void AfterRender();

    private:
        json RunLua(const json& arguments, const Ref<UI::SceneWindow>& sceneWindow);

        // Without an open scene, e.g. while the start scene loads: the engine bindings, Project and Editor
        json RunLuaWithoutScene(const json& arguments);

        void ReportProjectChange(const std::optional<Project>& before, json& result);

        std::function<Ref<UI::SceneWindow>()> getSceneWindow;
        std::filesystem::path directory;

        Bridge::McpServer server;
        Bridge::WindowShot windowShot;

        // Screenshots finish a frame later, once the window was copied
        std::optional<Bridge::McpServer::Reply> pendingShot;
        std::string shotPath;
        int32_t shotCount = 0;
        bool copyRequested = false;
        bool copied = false;

    };

}
