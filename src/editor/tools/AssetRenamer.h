#pragma once

#include "common/SerializationHelper.h"
#include "scene/Scene.h"

#include <string>
#include <unordered_map>

namespace Atlas::Editor {

    // Gives what is loaded the new path of a renamed file or directory, it's saved with it. Jobs read the paths, none
    // may run
    class AssetRenamer {

    public:
        static void RenameResources(const std::string& oldPath, const std::string& newPath);

        // Scripts can have paths of assets in their properties
        static void RenameInScene(Scene::Scene& scene, const std::string& oldPath, const std::string& newPath);

        // The state of an undo step with the paths its assets have now
        static json UpdatePaths(json state);

        static void Shutdown() { renamedPaths.clear(); }

    private:
        static void AddRenamedPaths(const std::string& oldPath, const std::string& newPath);

        // The current paths of the files renamed since the project was opened, by their earlier lower case paths
        static std::unordered_map<std::string, std::string> renamedPaths;

    };

}
