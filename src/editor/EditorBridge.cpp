#include "EditorBridge.h"
#include "Notifications.h"

#include <LuaEdit.h>
#include <LuaJob.h>
#include <McpTools.h>
#include <Processes.h>
#include <SceneTools.h>
#include <project/ProjectSerializer.h>
#include <scripting/LuaHost.h>

#include <optional>

namespace Atlas::Editor {

    EditorBridge::EditorBridge(const Ref<Project>& project, int32_t port, const std::function<Ref<UI::SceneWindow>()>& getSceneWindow)
        : getSceneWindow(getSceneWindow), directory(std::filesystem::path(project->GetDirectory()) / ".atlas" / "bridge"),
        server("atlas-editor", Bridge::McpTools::GetInstructions("editor with the project " + project->name +
            " open, changes show up while the user watches")) {

        // Jobs run in AtlasBridge next to the editor, paths of agents are relative to the project
        auto executable = Bridge::GetExecutablePath();
        auto bridgeExecutable = executable.parent_path() / ("AtlasBridge" + executable.extension().string());
        Bridge::McpTools::AddJobTools(server, bridgeExecutable, project->GetDirectory());

        server.AddTool({ .name = "lua",
            .description = "Runs Lua in the scene open in the editor, the change is visible right away and one undo step (Ctrl+Z) "
                "there. The scene isn't saved, the user saves it. While the scene plays, changes have no undo step and are "
                "discarded when playing stops. Without an open scene (e.g. while it loads) there are only Atlas, Glm, Project "
                "and Editor, the value of a return is the result. Functions which can't do what they are asked raise an "
                "error, the call fails with it. Project is the open project (Project.settings, Project:ApplySettings(), "
                "Project:Save()), changes of it are applied and one undo step in the project settings, the user saves them. "
                "Editor has the windows: Editor.Windows(), Editor.Open(path), Editor.Focus(nameOrPath), Editor.GetText(path) "
                "and Editor.SetText(path, text) of code windows (one undo step there, saved by the user). An open material "
                "graph editor shows graph changes of the script with an undo step. " + Bridge::McpTools::GetLuaHelp() + "\nUse Bridge functions for bulk changes, "
                "a loop over a million entities with SetPosition stays one cheap undo step.",
            .inputSchema = Bridge::McpTools::ObjectSchema({
                { "code", { { "type", "string" }, { "description", "Lua code" } } },
                { "name", { { "type", "string" }, { "description", "Name of the undo step" } } } }, { "code" }),
            .call = [this](const json& arguments, const Bridge::McpServer::Reply& reply) {
                auto result = RunLua(arguments, this->getSceneWindow());
                reply(Bridge::McpServer::TextContent(result.dump(2), result.value("exitCode", 2) != 0));
                } });

        server.AddTool({ .name = "screenshot", .description = "The editor window as the user sees it, as an image",
            .call = [this](const json&, const Bridge::McpServer::Reply& reply) {
                if (pendingShot) {
                    reply(Bridge::McpServer::TextContent("A screenshot is already being taken", true));
                    return;
                }
                std::filesystem::create_directories(directory);
                shotPath = (directory / ("screenshot-" + std::to_string(shotCount++ % 8) + ".png")).string();
                pendingShot = reply;
                copyRequested = true;
                } });

        server.AddTool({ .name = "undo",
            .description = "Undoes (or with redo: true redoes) the last step of the scene open in the editor, e.g. a lua call",
            .inputSchema = Bridge::McpTools::ObjectSchema({ { "redo", { { "type", "boolean" } } } }),
            .call = [this](const json& arguments, const Bridge::McpServer::Reply& reply) {
                auto sceneWindow = this->getSceneWindow();
                if (!sceneWindow || sceneWindow->isPlaying) {
                    reply(Bridge::McpServer::TextContent("Undo needs an open scene which doesn't play", true));
                    return;
                }
                auto redo = arguments.value("redo", false);
                auto& history = sceneWindow->history.history;
                auto step = redo ? history.GetRedoName() : history.GetUndoName();
                if (!(redo ? history.CanRedo() : history.CanUndo())) {
                    reply(Bridge::McpServer::TextContent(std::string("Nothing to ") + (redo ? "redo" : "undo"), true));
                    return;
                }
                if (redo)
                    sceneWindow->Redo();
                else
                    sceneWindow->Undo();
                reply(Bridge::McpServer::TextContent((redo ? "Redone: " : "Undone: ") + step));
                } });

        server.AddTool({ .name = "status", .description = "Which program serves the tools (the editor), its project, open scene and selected entities",
            .call = [this, project](const json&, const Bridge::McpServer::Reply& reply) {
                auto sceneWindow = this->getSceneWindow();
                auto hasScene = sceneWindow && sceneWindow->scene.IsValid();
                // The selected entities as "Name (#id)", Bridge.Find takes the #id
                json selected = json::array();
                if (hasScene && sceneWindow->scene.IsLoaded()) {
                    for (auto ecsEntity : sceneWindow->sceneHierarchyPanel.selectedEntities)
                        selected.push_back(Bridge::GetEntityLabel(Scene::Entity(ecsEntity, &sceneWindow->scene->entityManager)));
                }
                json status = { { "program", "editor" }, { "project", project->name }, { "projectFile", project->path },
                    { "scene", hasScene ? UI::SceneWindow::GetScenePath(sceneWindow->scene) : "" },
                    { "loaded", hasScene && sceneWindow->scene.IsLoaded() }, { "playing", sceneWindow && sceneWindow->isPlaying },
                    { "selected", selected } };
                reply(Bridge::McpServer::TextContent(status.dump(2)));
                } });

        std::string error;
        if (server.Start(port, error))
            Log::Message("MCP server of the editor on http://127.0.0.1:" + std::to_string(port) + "/mcp");
        else
            Notifications::Push({ .message = "No MCP server for agents: " + error, .type = NotificationType::Warning });

    }

    EditorBridge::~EditorBridge() {

        // Closing the server waits for the requests, a screenshot of a later frame won't come anymore
        if (pendingShot)
            (*pendingShot)(Bridge::McpServer::TextContent("The editor closed before the screenshot", true));

    }

    void EditorBridge::Update() {

        if (copied) {
            copied = false;
            auto reply = std::move(*pendingShot);
            pendingShot.reset();
            reply(windowShot.Save() ? Bridge::McpServer::ImageContent(shotPath) :
                Bridge::McpServer::TextContent("The window wasn't copied", true));
        }

        server.Update();

    }

    void EditorBridge::AfterRender() {

        if (!copyRequested)
            return;
        copyRequested = false;

        if (windowShot.Copy(shotPath)) {
            copied = true;
            return;
        }
        (*pendingShot)(Bridge::McpServer::TextContent("The window can't be copied, the swap chain isn't readable", true));
        pendingShot.reset();

    }

    json EditorBridge::RunLua(const json& arguments, const Ref<UI::SceneWindow>& sceneWindow) {

        if (!sceneWindow || !sceneWindow->scene.IsLoaded())
            return RunLuaWithoutScene(arguments);

        auto scene = sceneWindow->scene.Get();
        auto code = arguments.value("code", "");
        auto name = arguments.value("name", "Bridge script");

        // Changes while playing are discarded when playing stops, they don't need an undo step
        auto& history = sceneWindow->history;
        auto journal = !sceneWindow->isPlaying;

        Bridge::EditHooks hooks;
        hooks.selection = [sceneWindow]() {
            std::vector<Scene::Entity> entities;
            for (auto ecsEntity : sceneWindow->sceneHierarchyPanel.selectedEntities)
                entities.emplace_back(ecsEntity, &sceneWindow->scene->entityManager);
            return entities;
            };
        hooks.camera = [sceneWindow]() -> json {
            if (!sceneWindow->cameraEntity.IsValid())
                return nullptr;
            const auto& camera = sceneWindow->cameraEntity.GetComponent<CameraComponent>();
            auto location = camera.GetLocation();
            return { { "location", { location.x, location.y, location.z } },
                { "direction", { camera.direction.x, camera.direction.y, camera.direction.z } } };
            };
        hooks.extendEnvironment = [this](sol::environment& environment) {
            if (!addEditorFunctions)
                return;
            Scripting::Binder editor(sol::state_view(environment.lua_state()).create_table(), "Editor");
            addEditorFunctions(editor);
            environment["Editor"] = editor.GetTable();
            };
        if (journal) {
            hooks.beforeChange = [&history](Scene::Entity entity) { history.BeforeEntityChange(entity); };
            hooks.beforeTransformChange = [&history](Scene::Entity entity) { history.BeforeTransformChange(entity); };
            hooks.created = [&history](Scene::Entity entity) { history.EntityCreated(entity); };
            hooks.beforeDestroy = [&history](Scene::Entity entity) { history.BeforeEntityDestroy(entity); };
            // Edit operations don't report what they touch, the whole scene is saved before the first of them
            hooks.beforeOperation = [&history]() { history.BeforeSceneChange(); };
            history.BeginScriptChange(scene);
        }

        // The project is compared as it is saved, a change becomes an undo step of the project settings
        auto projectBefore = Project::active ? std::optional<Project>(*Project::active) : std::nullopt;

        json result;
        try {
            // Materials the operations change stay loaded with the change, they are saved with the scene
            Bridge::EditState state;
            Bridge::LuaEdit edit(scene, state, hooks);
            result = { { "status", "passed" }, { "exitCode", 0 }, { "results", edit.Run(code, name) } };
        }
        catch (const std::exception& exception) {
            result = { { "status", "error" }, { "exitCode", 2 }, { "reason", exception.what() } };
        }

        ReportProjectChange(projectBefore, result);

        if (journal) {
            auto changed = history.EndScriptChange(name);
            result["undoStep"] = changed > 0 ? json(name) : json(nullptr);
            result["changedEntities"] = changed;
            // The user decides when the scene is saved, closing the editor asks about the change
            if (changed > 0)
                sceneWindow->hasUnsavedChanges = true;
        }

        return result;

    }

    json EditorBridge::RunLuaWithoutScene(const json& arguments) {

        auto code = arguments.value("code", "");
        auto name = arguments.value("name", "Bridge script");
        auto projectBefore = Project::active ? std::optional<Project>(*Project::active) : std::nullopt;

        Scripting::LuaHost host;
        auto& lua = host.GetState();
        if (addEditorFunctions) {
            Scripting::Binder editor(lua.create_table(), "Editor");
            addEditorFunctions(editor);
            lua["Editor"] = editor.GetTable();
        }

        json result;
        auto scriptResult = lua.safe_script(code, sol::script_pass_on_error, "=" + name);
        if (scriptResult.valid()) {
            json results = json::object();
            if (scriptResult.return_count() > 0)
                results["value"] = Bridge::LuaJob::ToJson(scriptResult.get<sol::object>());
            result = { { "status", "passed" }, { "exitCode", 0 }, { "results", results } };
        }
        else {
            sol::error error = scriptResult;
            result = { { "status", "error" }, { "exitCode", 2 }, { "reason", "Lua " + name + ": " + error.what() } };
        }
        result["scene"] = nullptr;

        ReportProjectChange(projectBefore, result);
        return result;

    }

    void EditorBridge::ReportProjectChange(const std::optional<Project>& before, json& result) {

        if (!before || json(*before) == json(*Project::active))
            return;

        result["projectChanged"] = true;
        if (onProjectChange)
            onProjectChange(*before);

    }

}
