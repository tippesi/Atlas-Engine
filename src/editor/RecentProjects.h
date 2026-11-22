#pragma once

#include <project/Project.h>

#include <string>
#include <vector>

namespace Atlas::Editor {

    // The projects the user opened, newest first. Stored per user and not with a project
    class RecentProjects {

    public:
        struct Entry {
            std::string path;
            std::string name;
            // Seconds since the epoch
            int64_t lastOpened = 0;
        };

        static std::vector<Entry> Load();

        static void Add(const Project& project);

        static void Remove(const std::string& path);

    private:
        static void Save(const std::vector<Entry>& entries);

        static std::string GetFilename();

    };

}
