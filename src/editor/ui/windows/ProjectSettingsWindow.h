#pragma once

#include "Window.h"
#include "../../Icons.h"
#include "../../UndoHistory.h"

#include <project/Project.h>
#include <streaming/StreamingManager.h>

#include <functional>
#include <vector>

namespace Atlas::Editor::UI {

    // Settings stored in the .aeproject file on pages of a list: the project and the files of its scenes, the rendering
    // and the streaming limits. The search finds them by their name, section and keywords on all pages. Changes are applied
    // right away and can be undone, the user saves them (Ctrl+S, File > Save project)
    class ProjectSettingsWindow : public Window {

    public:
        explicit ProjectSettingsWindow(bool show);

        void Render(const Ref<Project>& project);

        // A change made elsewhere, e.g. by Lua: applied with onChange, unsaved and one undo step back to before
        void RecordChange(const Ref<Project>& project, const Project& before, const std::string& name);

        void Save(const Ref<Project>& project);

        UndoHistory history;

        // Called after a change, e.g. to apply the settings to the open scenes
        std::function<void(const Project&)> onChange;

    private:
        struct Setting {
            const char* page;
            const char* section;
            const char* name;
            // More words the search finds the setting with
            const char* keywords;
            std::function<void(Project&)> render;
        };

        void AddProjectSettings();

        void AddStreamingSettings();

        bool Matches(const Setting& setting) const;

        struct Page {
            const char* name;
            IconType icon;
            std::vector<const char*> sections;
        };

        std::vector<Setting> settings;
        std::vector<Page> pages;
        int32_t selectedPage = 0;

        std::string search;
        Streaming::StreamingManager::Stats streamingStats;

        // The project as it was when a field was activated, the undo step restores it
        Project editStart;

    };

}
