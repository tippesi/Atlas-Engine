#pragma once

#include "../System.h"
#include "../common/Ref.h"
#include "ProjectSettings.h"

#include <string>

namespace Atlas {

    namespace Scripting {
        class LuaHost;
    }

    namespace Scene {
        class Scene;
    }

    // A game or an application on top of the engine: its asset directory, the Lua script it starts with and its start
    // scene. The engine works without a project, applications using one activate it before they load any asset
    class Project {

    public:
        // Reads a .aeproject file, returns nullptr if it can't be read
        static Ref<Project> Load(const std::string& path);

        // Copies the template project into directory/name and saves it as directory/name/name.aeproject. The paths
        // listed in the template.json of the template are copied from the standard data directory first
        static Ref<Project> Create(const std::string& directory, const std::string& name,
            const std::string& templateDirectory, const std::string& standardDataDirectory);

        bool Save();

        // The asset directory of the project becomes the asset directory of the engine, Lua scripts see the project as
        // Project. Without a project none is active and the asset directory stays
        static void Activate(const Ref<Project>& project);

        // The settings of the project which the engine keeps elsewhere: the streaming limits
        void ApplySettings() const;

        // The settings which scenes keep for the renderer and their files, every scene of the project gets them
        void ApplyTo(Scene::Scene& scene) const;

        // Projects saved before they had the scene settings take them from the first scene they load, returns true if
        // they were taken and the project has to be saved
        bool AdoptSceneSettings(const Scene::Scene& scene);

        std::string GetDirectory() const;

        std::string GetAssetDirectory() const;

        std::string name;
        // Relative to the project file
        std::string assetDirectory = ".";
        // Relative to the asset directory, empty if the project has no main script
        std::string mainScript;
        // Relative to the asset directory
        std::string startScene;

        ProjectSettings settings;
        // Whether the project file has the scene settings or still has to take them from a scene
        bool hasSceneSettings = true;

        // Absolute path of the project file
        std::string path;
        // Version of the engine the project was saved with, empty for files older than the version field
        std::string engineVersion;

        // The activated project, nullptr without one
        static Ref<Project> active;

    };

}
