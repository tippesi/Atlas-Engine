#pragma once

#include <EngineInstance.h>
#include <input/Mouse.h>
#include <input/Keyboard.h>
#include <input/PlayerInput.h>
#include <input/Controller.h>
#include <input/Touch.h>
#include <loader/ModelImporter.h>
#include <ImguiExtension/ImguiWrapper.h>
#include <profiler/HitchRecorder.h>

#include <renderer/PathTracingRenderer.h>

#include "ui/windows/ProjectHubWindow.h"
#include "ui/StatusBar.h"
#include "EditorBridge.h"
#include "EditorCommands.h"
#include "EditorFunctions.h"
#include "Workspace.h"

#include <memory>
#include <optional>

#define WINDOW_FLAGS AE_WINDOW_RESIZABLE | AE_WINDOW_HIGH_DPI

namespace Atlas::Editor {

    class App : public EngineInstance {

    public:
        App() : EngineInstance("Atlas Engine Editor", 1920, 1080, WINDOW_FLAGS) {}

        virtual void LoadContent() final;

        virtual void UnloadContent() final;

        virtual void Update(float deltaTime) final;

        virtual void Render(float deltaTime) final;

        bool AllowQuit() final;

    private:
        // The commands of the project, the layout and the theme
        void RegisterCommands();

        // Activates the project of --project, the asset directory and the editor state are the ones of the project
        void LoadProject();

        // Opens the start scene of a project without opened scenes, a missing start scene is created
        void OpenStartScene();

        // Streaming limits for the engine, scene settings for the open scenes
        void ApplyProjectSettings();

        // Writes the recorded Lua bindings for the Lua language server into .atlas/lua of the project, a .luarc.json
        // which doesn't exist yet points other editors there
        void ExportLuaAnnotations();

        // The editor starts again with the project, an empty path shows the project hub. Resources, shaders and the
        // editor state of the old asset directory don't carry over this way
        void OpenProject(const std::string& path);

        void RenderSceneMaximized();

        void SetupMainDockspace(ImGuiID dsID);

        void RenderEmptyState(ImGuiID dsID);

        void ApplyTheme();

        // Follows the scale of the display unless the user chose one, the font is rasterized for it
        void UpdateUiScale();

        void RenderRecentScenesMenu();

        // Asks whether the scenes closed by the user or by quitting are saved or their changes discarded
        void RenderUnsavedChangesPopup();

        void AddUnsavedWindow(const Ref<UI::Window>& window);

        void SubscribeToResourceEvents();

        // Unloads the open project and loads nextProject
        void SwitchProject();

        // Saves the state of the windows and the config, a crash only loses the changes since the last time
        void SaveEditorState();

        void SetDefaultWindowResolution();

        Input::MouseHandler mouseHandler;
        Input::KeyboardHandler keyboardHandler;
        Input::PlayerInput playerInput;

        ImguiExtension::PerformanceGraphPanel performanceGraphPanel;

        Workspace workspace;
        EditorCommands editorCommands = EditorCommands(workspace);
        EditorFunctions editorFunctions = EditorFunctions(workspace);
        UI::StatusBar statusBar = UI::StatusBar(workspace);

        bool showShortcutsWindow = false;

        std::vector<ResourceHandle<Scene::Scene>> waitToLoadScenes;
        // Scenes opened by the user and not restored from the config
        std::vector<ResourceHandle<Scene::Scene>> userOpenedScenes;
        // Scenes, assets and code with unsaved changes the user wants to close, all of them before quitting
        std::vector<Ref<UI::Window>> unsavedWindows;
        bool quitAfterUnsavedChanges = false;
        bool quitConfirmed = false;

        float systemUiScale = 0.0f;
        float fontRasterSize = 0.0f;
        std::vector<std::string> emptyStateScenes;
        bool emptyStateVisible = false;

        bool resetDockspaceLayout = false;
        bool dockLoadingScenes = false;
        bool restoringScenes = true;

        static constexpr float editorStateSaveInterval = 60.0f;
        float lastEditorStateSaveTime = 0.0f;

        // Hitch settings of the config while --log-hitches overrides them, these are saved instead
        std::optional<HitchRecorder::Settings> configHitchSettings;

        // New projects copy their standard data and templates from the asset directory the editor started with
        std::string engineDataDirectory;
        std::unique_ptr<UI::ProjectHubWindow> projectHub;
        // Opened once the unsaved changes are saved or discarded, empty for the project hub. The editor switches in
        // the same process, the window and the debugger stay
        std::optional<std::string> nextProject;
        bool projectSwitchRequested = false;
        bool switchingProject = false;
        int32_t sceneCreateSubscription = 0;
        uint32_t dropSubscription = 0;
        // ImGui keeps the pointer to the file name
        std::string imguiIniPath;
        // Atlas Bridge commands for the open scene, only with a project
        std::unique_ptr<EditorBridge> bridge;

    };

}