#pragma once

#include "../../RecentProjects.h"

#include <SDL3/SDL.h>

#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace Atlas::Editor::UI {

    // Shown instead of the editor when it starts without a project: the recent projects, creating a project from a
    // template, opening a project file and adding a folder without one as a project
    class ProjectHubWindow {

    public:
        ProjectHubWindow(const std::string& standardDataDirectory, const std::string& templatesDirectory);

        // Covers the whole window, returns the path of the project file to open
        std::optional<std::string> Render(SDL_Window* window);

        // Asks for the folder and the template of a new project, also while a project is open
        void OpenNewProjectPopup();

        // Returns the project file of the new project once it was created
        std::optional<std::string> RenderNewProjectPopup(SDL_Window* window);

    private:
        struct Template {
            std::string directory;
            std::string name;
            std::string description;
        };

        enum class DialogType {
            OpenProject = 0,
            AddFolder,
            Location
        };

        struct DialogRequest {
            ProjectHubWindow* window;
            DialogType type;
        };

        void LoadEntries();

        void LoadTemplates();

        // Uses the project file of the folder or creates one, the assets of folders with a data directory are in there
        std::string AddFolder(const std::string& folder);

        void ShowDialog(SDL_Window* window, DialogType type);

        static void DialogCallback(void* userData, const char* const* files, int32_t filter);

        std::string standardDataDirectory;
        std::string templatesDirectory;

        std::vector<RecentProjects::Entry> entries;
        std::vector<bool> entryExists;
        std::vector<Template> templates;

        std::string search;
        std::string newName;
        std::string newLocation;
        int32_t templateIndex = 0;
        bool newProjectPopupRequested = false;
        std::string error;

        DialogRequest dialogRequests[3];
        // The system dialogs answer from another thread
        std::mutex dialogMutex;
        std::optional<std::pair<DialogType, std::string>> dialogResult;

        // The result of a file dialog: a project to open, or the location of a new project
        std::optional<std::string> TakeDialogResult();

    };

}
