#pragma once

#include <string>

#include "ui/windows/SceneWindow.h"

namespace Atlas::Editor {

    class Serialization {

    public:
        static void SerializeConfig();

        static void DeserializeConfig();

        static void SerializeSceneWindow(const Ref<UI::SceneWindow>& sceneWindow);

        static Ref<UI::SceneWindow> DeserializeSceneWindow(ResourceHandle<Scene::Scene> handle);

    private:
        static void TryWriteToFile(const std::string& filename, const std::string& content);

        static std::string TryReadFromFile(const std::string& filename);

        static const std::string configFilename;

        static const std::string configPath;

    };

}