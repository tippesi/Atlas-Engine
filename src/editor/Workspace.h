#pragma once

#include "Icons.h"
#include "UndoHistory.h"
#include "ui/windows/CodeEditorWindow.h"
#include "ui/windows/ContentBrowserWindow.h"
#include "ui/windows/GeometryBrushWindow.h"
#include "ui/windows/LogWindow.h"
#include "ui/windows/MaterialEditorWindow.h"
#include "ui/windows/MeshEditorWindow.h"
#include "ui/windows/ProfilerWindow.h"
#include "ui/windows/ProjectSettingsWindow.h"
#include "ui/windows/SceneWindow.h"

#include <string>
#include <utility>
#include <vector>

namespace Atlas::Editor {

    // The windows of the open project: scenes, assets and code as tabs, the drawers and the settings. It knows which of
    // them the user works in, opens files and keeps the windows in the layout
    class Workspace {

    public:
        struct Drawer {
            UI::DrawerWindow* window;
            IconType icon;
            const char* name;
            const char* command;
        };

        // Without unsaved changes, the next project opens its own windows
        void Close();

        Ref<UI::SceneWindow> GetActiveSceneWindow() const;

        bool IsPlayingMaximized() const;

        // Undo and redo act on the settings, code, asset editor or scene in front
        const UndoHistory* GetActiveHistory() const;

        void Undo();

        void Redo();

        bool CanUndo() const;

        bool CanRedo() const;

        // Opens meshes and materials in their editors and prefabs in a scene window, animations play in the last
        // focused mesh editor
        // Returns why the file can't be opened, empty if it was
        std::string Open(const std::string& path);

        bool IsWindowOpen(const Ref<UI::Window>& window) const;

        // Saves a scene, asset or code window
        void SaveWindow(const Ref<UI::Window>& window);

        // The name and file of a window, e.g. for the unsaved changes dialog
        std::pair<std::string, std::string> GetWindowFile(const Ref<UI::Window>& window) const;

        // Open windows and the project follow a renamed file or directory
        void RenamePaths(const std::string& oldPath, const std::string& newPath, bool updateReferences);

        // Windows opened by the user join the selected window as a tab, others the scenes
        void OpenAsTab(UI::Window& window) const;

        // Called before a window is rendered: an opened window becomes a focused tab next to the active window, windows
        // of the restored layout keep their place unless they float
        void DockAsTab(UI::Window& window);

        Drawer* FindDrawer(const std::string& name);

        void UpdateDrawers();

        // Pinned drawers share one area at the bottom of the layout, which is split off before the dockspace is updated
        void PrepareDrawerDocking(ImGuiID dsID);

        void DockPinnedDrawer(UI::DrawerWindow& drawer);

        UI::LogWindow logWindow = UI::LogWindow(false);
        UI::ContentBrowserWindow contentBrowserWindow = UI::ContentBrowserWindow(false);
        UI::ProfilerWindow profilerWindow = UI::ProfilerWindow(false);
        UI::GeometryBrushWindow geometryBrushWindow = UI::GeometryBrushWindow(false);
        UI::ProjectSettingsWindow projectSettingsWindow = UI::ProjectSettingsWindow(false);

        // The config decides which of them the status bar shows
        std::vector<Drawer> drawers = {
            { &contentBrowserWindow, IconType::Folder, "Content browser", "Window: Toggle content browser" },
            { &logWindow, IconType::Log, "Log", "Window: Toggle log" },
            { &profilerWindow, IconType::Profiler, "Profiler", "Window: Toggle profiler" }
        };

        std::vector<Ref<UI::SceneWindow>> sceneWindows;
        std::vector<Ref<UI::AssetEditorWindow>> assetEditorWindows;
        std::vector<Ref<UI::CodeEditorWindow>> codeEditorWindows;

        size_t activeSceneIdx = 0;
        Ref<UI::AssetEditorWindow> activeAssetEditorWindow;
        // The code window in front, its undo and search are those of the edit menu and the command palette
        Ref<UI::CodeEditorWindow> activeCodeEditorWindow;

    private:
        void DockNextToScenes(UI::Window& window);

        // Dock node of the asset editor or scene window that was focused last
        ImGuiID GetActiveDockNodeID() const;

        ImGuiID drawerDockNodeID = 0;

    };

}
