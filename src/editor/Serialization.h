#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "ui/windows/SceneWindow.h"
#include "ui/windows/ContentBrowserWindow.h"
#include "ui/windows/DrawerWindow.h"

namespace Atlas::Editor {

    class Serialization {

    public:
        static void SerializeConfig();

        static void DeserializeConfig();

        static void SerializeSceneWindow(const Ref<UI::SceneWindow>& sceneWindow);

        static Ref<UI::SceneWindow> DeserializeSceneWindow(ResourceHandle<Scene::Scene> handle);

        static void SerializeContentBrowserWindow(const UI::ContentBrowserWindow& contentBrowserWindow);

        static void DeserializeContentBrowserWindow(UI::ContentBrowserWindow& contentBrowserWindow);

        // Only the drawer state, the content browser has its own config
        static void SerializeDrawerWindow(const UI::DrawerWindow& drawerWindow);

        static void DeserializeDrawerWindow(UI::DrawerWindow& drawerWindow);

        // The .atlas directory of a project, without a project .config in the asset directory
        static void SetConfigDirectory(const std::string& directory);

    private:
        // Scenes are identified by their file, the internal names of two scenes can be the same
        static std::string GetSceneWindowConfigFilename(const ResourceHandle<Scene::Scene>& scene);

        static std::string GetDrawerWindowConfigFilename(const UI::DrawerWindow& drawerWindow);

        static void TryWriteToFile(const std::string& filename, json& j);

        // Empty if the file doesn't exist or is damaged, the defaults are used then
        static json TryReadJson(const std::string& filename);

        static std::string configPath;
        // What the config files contain, the state is saved regularly and most of it doesn't change
        static std::unordered_map<std::string, json> writtenConfigs;
        // The last save of them failed
        static std::unordered_set<std::string> failedConfigs;

    };

}