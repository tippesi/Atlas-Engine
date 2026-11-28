#include "RecentProjects.h"

#include <common/SerializationHelper.h>
#include <loader/AssetLoader.h>

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <fstream>

namespace Atlas::Editor {

    std::vector<RecentProjects::Entry> RecentProjects::Load() {

        std::vector<Entry> entries;

        std::ifstream stream(GetFilename(), std::ios::binary);
        if (!stream.is_open())
            return entries;

        auto j = json::parse(stream, nullptr, false);
        if (j.is_discarded() || !j.contains("projects"))
            return entries;

        for (const auto& project : j["projects"]) {
            Entry entry;
            try_get_json(project, "path", entry.path);
            try_get_json(project, "name", entry.name);
            try_get_json(project, "lastOpened", entry.lastOpened);
            entries.push_back(entry);
        }

        return entries;

    }

    void RecentProjects::Add(const Project& project) {

        auto entries = Load();
        std::erase_if(entries, [&](const Entry& entry) { return entry.path == project.path; });

        auto now = std::chrono::system_clock::now().time_since_epoch();
        entries.insert(entries.begin(), Entry {
            .path = project.path,
            .name = project.name,
            .lastOpened = std::chrono::duration_cast<std::chrono::seconds>(now).count()
            });

        Save(entries);

    }

    void RecentProjects::Remove(const std::string& path) {

        auto entries = Load();
        std::erase_if(entries, [&](const Entry& entry) { return entry.path == path; });
        Save(entries);

    }

    void RecentProjects::Save(const std::vector<Entry>& entries) {

        json projects = json::array();
        for (const auto& entry : entries)
            projects.push_back({ { "path", entry.path }, { "name", entry.name }, { "lastOpened", entry.lastOpened } });

        json j = { { "projects", projects } };
        Loader::AssetLoader::WriteJson(GetFilename(), j, Loader::JsonFormat::Indented);

    }

    std::string RecentProjects::GetFilename() {

        static const std::string filename = []() {
            auto prefPath = SDL_GetPrefPath("AtlasEngine", "Editor");
            std::string directory = prefPath ? prefPath : "";
            SDL_free(prefPath);
            return directory + "projects.json";
            }();
        return filename;

    }

}
