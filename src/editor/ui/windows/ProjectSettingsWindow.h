#pragma once

#include "Window.h"
#include "../../UndoHistory.h"

#include <project/Project.h>
#include <streaming/StreamingManager.h>

#include <functional>
#include <vector>

namespace Atlas::Editor::UI {

    // Settings stored in the .aeproject file, searchable by their name, category and keywords: the project, the
    // rendering and files of its scenes and the streaming limits. Changes are applied right away and can be undone, the
    // user saves them (Ctrl+S, File > Save project)
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
            const char* category;
            const char* name;
            // More words the search finds the setting with
            const char* keywords;
            std::function<void(Project&)> render;
        };

        void AddProjectSettings();

        void AddStreamingSettings();

        bool Matches(const Setting& setting) const;

        std::vector<Setting> settings;
        std::vector<const char*> categories;

        std::string search;
        Streaming::StreamingManager::Stats streamingStats;

        // The project as it was when a field was activated, the undo step restores it
        Project editStart;

    };

}
