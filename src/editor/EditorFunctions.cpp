#include "EditorFunctions.h"
#include "Workspace.h"
#include "Singletons.h"

namespace Atlas::Editor {

    void EditorFunctions::Add(Scripting::Binder& editor) {

        // { name, kind, path, shown, focused, unsaved } of the tabs and drawers
        editor.set_function("Windows", [this](sol::this_state state) {
            sol::state_view lua(state);
            sol::table windows = lua.create_table();
            auto add = [&](const UI::Window& window, const char* kind, const std::string& path) {
                windows.add(lua.create_table_with("name", window.name, "kind", kind, "path", path, "shown", window.show,
                    "focused", window.inFocus, "unsaved", window.hasUnsavedChanges));
                };
            for (const auto& window : workspace.sceneWindows)
                add(*window, "scene", UI::SceneWindow::GetScenePath(window->scene));
            for (const auto& window : workspace.assetEditorWindows)
                add(*window, "asset", window->GetAssetPath());
            for (const auto& window : workspace.codeEditorWindows)
                add(*window, "code", window->GetPath());
            add(workspace.projectSettingsWindow, "project settings", Singletons::project ? Singletons::project->path : "");
            add(workspace.contentBrowserWindow, "content browser", workspace.contentBrowserWindow.currentDirectory);
            add(workspace.logWindow, "log", "");
            add(workspace.profilerWindow, "profiler", "");
            return windows;
            });
        // Opens a file like a double click in the content browser: scenes, assets, scripts and shaders
        editor.set_function("Open", [this](const std::string& path) {
            if (!std::filesystem::exists(Loader::AssetLoader::GetFullPath(path)))
                throw std::runtime_error(path + " doesn't exist in the asset directory");
            auto error = workspace.Open(path);
            if (!error.empty())
                throw std::runtime_error(error);
            });
        // Shows a directory of the asset directory in the content browser, like a click on the folder
        editor.set_function("Browse", [this](const std::string& path) {
            auto fullPath = Loader::AssetLoader::GetFullPath(path);
            if (!std::filesystem::is_directory(fullPath))
                throw std::runtime_error(path + " isn't a directory in the asset directory");
            workspace.contentBrowserWindow.currentDirectory = Common::Path::Normalize(fullPath);
            workspace.contentBrowserWindow.show = true;
            workspace.contentBrowserWindow.requestFocus = true;
            });
        // Shows a window and gives it the focus, found by its name or path
        editor.set_function("Focus", [this](const std::string& nameOrPath) {
            std::vector<std::pair<UI::Window*, std::string>> windows;
            for (const auto& window : workspace.sceneWindows)
                windows.emplace_back(window.get(), UI::SceneWindow::GetScenePath(window->scene));
            for (const auto& window : workspace.assetEditorWindows)
                windows.emplace_back(window.get(), window->GetAssetPath());
            for (const auto& window : workspace.codeEditorWindows)
                windows.emplace_back(window.get(), window->GetPath());
            windows.emplace_back(&workspace.projectSettingsWindow, "");
            windows.emplace_back(&workspace.contentBrowserWindow, "");
            windows.emplace_back(&workspace.logWindow, "");
            windows.emplace_back(&workspace.profilerWindow, "");
            for (auto& [window, path] : windows) {
                if (window->name != nameOrPath && path != nameOrPath)
                    continue;
                window->show = true;
                window->requestFocus = true;
                return;
            }
            throw std::runtime_error("There is no window " + nameOrPath + ", Editor.Windows() lists them");
            });
        // The text of an open code window, also while it isn't saved
        editor.set_function("GetText", [this](const std::string& path) -> std::string {
            for (const auto& window : workspace.codeEditorWindows)
                if (window->GetPath() == path)
                    return window->GetText();
            throw std::runtime_error(path + " isn't open in a code window, Editor.Open(path) opens it");
            });
        // Replaces the text of a code window, opened if needed, as one undo step there. The user saves it
        editor.set_function("SetText", [this](const std::string& path, const std::string& text) {
            auto find = [&]() {
                return std::find_if(workspace.codeEditorWindows.begin(), workspace.codeEditorWindows.end(),
                    [&](const auto& window) { return window->GetPath() == path; });
                };
            if (find() == workspace.codeEditorWindows.end())
                workspace.Open(path);
            auto window = find();
            if (window == workspace.codeEditorWindows.end())
                throw std::runtime_error(path + " can't be opened in a code window");
            (*window)->SetText(text);
            });
        // Like the play and stop buttons of the active scene window, they take effect in the next frame
        editor.set_function("Play", [this]() {
            auto sceneWindow = workspace.GetActiveSceneWindow();
            if (!sceneWindow || !sceneWindow->scene.IsLoaded())
                throw std::runtime_error("There is no loaded scene window, Editor.Focus(path) activates one");
            if (sceneWindow->isPlaying)
                throw std::runtime_error(sceneWindow->name + " is already playing");
            if (!sceneWindow->StartPlaying())
                throw std::runtime_error(sceneWindow->name + " can't play, another operation is running");
            });
        editor.set_function("Stop", [this]() {
            auto sceneWindow = workspace.GetActiveSceneWindow();
            if (!sceneWindow || !sceneWindow->isPlaying)
                throw std::runtime_error("The active scene window isn't playing");
            sceneWindow->StopPlaying();
            });
        // Selects entities of the active scene window like clicks in the hierarchy, an entity or a list of them
        editor.set_function("Select", [this](sol::object entities) {
            auto sceneWindow = workspace.GetActiveSceneWindow();
            if (!sceneWindow || !sceneWindow->scene.IsLoaded())
                throw std::runtime_error("There is no loaded scene window, Editor.Focus(path) activates one");
            std::vector<Scene::Entity> selection;
            if (entities.is<Scene::Entity>())
                selection.push_back(entities.as<Scene::Entity>());
            else if (entities.is<sol::table>())
                for (const auto& [key, value] : entities.as<sol::table>())
                    selection.push_back(value.as<Scene::Entity>());
            if (!std::all_of(selection.begin(), selection.end(), [](const auto& entity) { return entity.IsValid(); }))
                throw std::runtime_error("Editor.Select got an invalid entity");
            auto& hierarchy = sceneWindow->sceneHierarchyPanel;
            hierarchy.ClearSelection();
            for (auto entity : selection) {
                hierarchy.SelectEntity(entity, true);
                hierarchy.RevealEntity(sceneWindow->scene.Get(), entity);
            }
            });
        // Moves the editor camera of the active scene window such that the selection is in view, like F
        editor.set_function("FocusSelection", [this]() {
            auto sceneWindow = workspace.GetActiveSceneWindow();
            if (!sceneWindow || !sceneWindow->scene.IsLoaded())
                throw std::runtime_error("There is no loaded scene window, Editor.Focus(path) activates one");
            sceneWindow->focusAfterUpdate = true;
            });
        // Runs a command of the command palette (Ctrl+P) by its name, e.g. "Light ranges", and returns
        // whether a toggle is checked afterwards
        editor.set_function("RunCommand", [this](const std::string& name) {
            auto command = Singletons::commandPalette->Find(name);
            if (!command)
                throw std::runtime_error("There is no command " + name + ", the command palette (Ctrl+P) lists them");
            if (command->isEnabled && !command->isEnabled())
                throw std::runtime_error(name + " isn't available right now");
            command->action();
            return command->isChecked && command->isChecked();
            });

    }

}
