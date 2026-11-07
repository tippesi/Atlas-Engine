#pragma once

#include "Window.h"

#include <streaming/StreamingManager.h>

#include <functional>
#include <vector>

namespace Atlas::Editor::UI {

    // Settings of the engine which aren't stored with a scene, searchable by their name, category and keywords
    class SettingsWindow : public Window {

    public:
        explicit SettingsWindow(bool show);

        void Render();

    private:
        struct Setting {
            const char* category;
            const char* name;
            // More words the search finds the setting with
            const char* keywords;
            std::function<void()> render;
        };

        void AddStreamingSettings();

        bool Matches(const Setting& setting) const;

        std::vector<Setting> settings;
        std::vector<const char*> categories;

        std::string search;
        Streaming::StreamingManager::Stats streamingStats;

    };

}
