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

#include "ui/windows/ContentBrowserWindow.h"
#include "ui/windows/SceneWindow.h"
#include "ui/windows/MeshEditorWindow.h"
#include "ui/windows/MaterialEditorWindow.h"
#include "ui/windows/LogWindow.h"
#include "ui/windows/ProfilerWindow.h"
#include "ui/windows/GeometryBrushWindow.h"
#include "ui/windows/ProjectHubWindow.h"
#include "ui/windows/ProjectSettingsWindow.h"
#include "ui/windows/CodeEditorWindow.h"
#include "ui/popups/CommandPalette.h"
#include "EditorBridge.h"

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
        struct Drawer {
            UI::DrawerWindow* window;
            IconType icon;
            const char* name;
            const char* shortcut;
        };

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

        float GetStatusChipWidth(const char* text, float trailingWidth = 0.0f);

        float GetNotificationBadgeWidth(int32_t unreadCount);

        void RenderNotificationButton();

        void RenderStatusBar();

        // Undo and redo act on the asset editor or scene in front
        void Undo();

        void Redo();

        bool CanUndo() const;

        bool CanRedo() const;

        const UndoHistory* GetActiveHistory() const;

        bool RenderDrawerButton(UI::DrawerWindow& drawer, IconType icon, const char* label, const char* shortcut,
            int32_t errorCount = 0, int32_t warningCount = 0);

        // Chooses and orders the drawers of the status bar, opened by a right click on it
        void RenderStatusBarMenu();

        // Clickable items of the status bar carry a background, such that they stand out from plain information
        bool RenderStatusChip(const char* id, IconType icon, const char* text, ImVec4 color,
            ImVec4 background, ImVec4 backgroundHovered, float maxWidth = FLT_MAX, float trailingWidth = 0.0f);

        float GetCapCenteredTextY(float centerY);

        void UpdateDrawers();

        // Pinned drawers share one area at the bottom of the layout, which is split off before the dockspace is updated
        void PrepareDrawerDocking(ImGuiID dsID);

        void DockPinnedDrawer(UI::DrawerWindow& drawer);

        Drawer* FindDrawer(const std::string& name);

        void RenderRecentScenesMenu();

        void RenderShortcutsWindow();

        // Asks whether the scenes closed by the user or by quitting are saved or their changes discarded
        void RenderUnsavedChangesPopup();

        // Saves a scene, asset or code window
        void SaveWindow(const Ref<UI::Window>& window);

        // The name and file of a window for the unsaved changes dialog
        std::pair<std::string, std::string> GetWindowFile(const Ref<UI::Window>& window) const;

        bool IsWindowOpen(const Ref<UI::Window>& window) const;

        void AddUnsavedWindow(const Ref<UI::Window>& window);

        void RegisterCommands();

        // Opens meshes and materials in their editors and prefabs in a scene window, animations play in the last
        // focused mesh editor
        // Returns why the file can't be opened, empty if it was
        std::string OpenInAssetEditor(const std::string& path);

        // Windows opened by the user join the selected window as a tab, others the scenes
        void DockNextToScenes(UI::Window& window);

        // Dock node of the asset editor or scene window that was focused last
        ImGuiID GetActiveDockNodeID() const;

        void OpenAsTab(UI::Window& window) const;

        // Called before a window is rendered: an opened window becomes a focused tab next to the active window, windows
        // of the restored layout keep their place unless they float
        void DockAsTab(UI::Window& window);

        Ref<UI::SceneWindow> GetActiveSceneWindow() const;

        void SubscribeToResourceEvents();

        // Unloads the open project and loads nextProject
        void SwitchProject();

        // The table Editor of the Lua of the MCP server: windows, focus, opening files and the text of code windows
        void AddEditorFunctions(Scripting::Binder& editor);

        void SetDefaultWindowResolution();

        Input::MouseHandler mouseHandler;
        Input::KeyboardHandler keyboardHandler;
        Input::PlayerInput playerInput;

        UI::LogWindow logWindow = UI::LogWindow(false);
        UI::ContentBrowserWindow contentBrowserWindow = UI::ContentBrowserWindow(false);
        UI::ProfilerWindow profilerWindow = UI::ProfilerWindow(false);
        UI::GeometryBrushWindow geometryBrushWindow = UI::GeometryBrushWindow(false);

        // The config decides which of them the status bar shows
        std::vector<Drawer> drawers = {
            { &contentBrowserWindow, IconType::Folder, "Content browser", "Ctrl+B" },
            { &logWindow, IconType::Log, "Log", "Ctrl+J" },
            { &profilerWindow, IconType::Profiler, "Profiler", "" }
        };

        ImguiExtension::PerformanceGraphPanel performanceGraphPanel;

        UI::CommandPalette commandPalette;
        bool showShortcutsWindow = false;

        std::vector<ResourceHandle<Scene::Scene>> waitToLoadScenes;
        // Scenes opened by the user and not restored from the config
        std::vector<ResourceHandle<Scene::Scene>> userOpenedScenes;
        std::vector<Ref<UI::SceneWindow>> sceneWindows;
        std::vector<Ref<UI::AssetEditorWindow>> assetEditorWindows;
        std::vector<Ref<UI::CodeEditorWindow>> codeEditorWindows;
        Ref<UI::AssetEditorWindow> activeAssetEditorWindow;
        // The code window in front, its undo and search are those of the edit menu and the command palette
        Ref<UI::CodeEditorWindow> activeCodeEditorWindow;

        // Scenes, assets and code with unsaved changes the user wants to close, all of them before quitting
        std::vector<Ref<UI::Window>> unsavedWindows;
        bool quitAfterUnsavedChanges = false;
        bool quitConfirmed = false;

        size_t activeSceneIdx = 0;

        ImGuiID drawerDockNodeID = 0;

        float drawerSpringTime = 0.0f;
        // Background job count shown in the status bar, updated twice a second
        float backgroundJobTime = 0.0f;
        float backgroundJobRefreshTime = 0.0f;
        int32_t backgroundJobPeak = 0;
        int32_t backgroundJobShown = 0;

        float systemUiScale = 0.0f;
        float fontRasterSize = 0.0f;
        bool notificationsOpen = false;

        std::vector<std::string> emptyStateScenes;
        bool emptyStateVisible = false;

        bool resetDockspaceLayout = false;
        bool dockLoadingScenes = false;
        bool restoringScenes = true;
        bool playingMaximized = false;

        // Hitch settings of the config while --log-hitches overrides them, these are saved instead
        std::optional<HitchRecorder::Settings> configHitchSettings;

        // New projects copy their standard data and templates from the asset directory the editor started with
        std::string engineDataDirectory;
        std::unique_ptr<UI::ProjectHubWindow> projectHub;
        UI::ProjectSettingsWindow projectSettingsWindow = UI::ProjectSettingsWindow(false);
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