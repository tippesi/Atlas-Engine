#pragma once

#include <string>

#include "ui/windows/SceneWindow.h"
#include "ui/windows/ContentBrowserWindow.h"
#include "ui/windows/LogWindow.h"

namespace Atlas::Editor {

    class Serialization {

    public:
        static void SerializeConfig();

        static void DeserializeConfig();

        static void SerializeSceneWindow(const Ref<UI::SceneWindow>& sceneWindow);

        static Ref<UI::SceneWindow> DeserializeSceneWindow(ResourceHandle<Scene::Scene> handle);

        static void SerializeContentBrowserWindow(const UI::ContentBrowserWindow& contentBrowserWindow);

        static void DeserializeContentBrowserWindow(UI::ContentBrowserWindow& contentBrowserWindow);

        static void SerializeLogWindow(const UI::LogWindow& logWindow);

        static void DeserializeLogWindow(UI::LogWindow& logWindow);

    private:
        static void TryWriteToFile(const std::string& filename, const std::string& content);

        static std::string TryReadFromFile(const std::string& filename);

        static const std::string configFilename;
        static const std::string configPath;
        static const std::string contentBrowserWindowFilename;
        static const std::string logWindowFilename;

    };

}