#include "Serializer.h"
#include "Singletons.h"
#include "FileImporter.h"

#include "common/SerializationHelper.h"
#include "scene/EntitySerializer.h"

#include "Log.h"
#include "loader/AssetLoader.h"

namespace Atlas::Editor {

    const std::string Serializer::configPath = ".config/";
    const std::string Serializer::configFilename = configPath + "config.json";

    void Serializer::SerializeConfig() {

        const auto& config = Singletons::config;

        std::vector<std::string> scenePaths;
        for (const auto& scene : config->openedScenes)
            scenePaths.push_back(scene.GetResource()->path);

        json j = {
            { "darkMode", config->darkMode },
            { "pathTrace", config->pathTrace },
            { "scenes", scenePaths }
        };

        TryWriteToFile(configFilename, to_string(j));

    }

    void Serializer::DeserializeConfig() {

       auto serialized = TryReadFromFile(configFilename);

        if (serialized.empty())
            return;

        json j = json::parse(serialized);

        const auto& config = Singletons::config;

        std::vector<std::string> scenePaths;

        if (j.contains("darkMode"))
            j.at("darkMode").get_to(config->darkMode);
        if (j.contains("pathTrace"))
            j.at("pathTrace").get_to(config->pathTrace);
        if (j.contains("scenes"))
            j.at("scenes").get_to(scenePaths);

        // No need to add it to the config, will be done through resource events
        for (const auto& scenePath : scenePaths)
            FileImporter::ImportFile(scenePath);

    }

    void Serializer::SerializeSceneWindow(const Ref<UI::SceneWindow>& sceneWindow) {

        if (!sceneWindow->scene.IsLoaded())
            return;

        json camera;
        std::set<ECS::Entity> insertedEntites;
        Scene::EntityToJson(camera, sceneWindow->cameraEntity, sceneWindow->scene.Get(), insertedEntites);

        json j = {
            { "cameraMovementSpeed", sceneWindow->cameraMovementSpeed },
            { "cameraRotationSpeed", sceneWindow->cameraRotationSpeed },
            { "camera", camera }
        };

        auto filename = configPath + sceneWindow->scene->name + "WindowConfig.json";

        TryWriteToFile(filename, to_string(j));

    }

    Ref<UI::SceneWindow> Serializer::DeserializeSceneWindow(ResourceHandle<Scene::Scene> handle) {

        if (!handle.IsLoaded())
            return nullptr;

        auto filename = configPath + handle->name + "WindowConfig.json";

        auto sceneWindow = CreateRef<UI::SceneWindow>(handle, true);

        auto serialized = TryReadFromFile(filename);

        if (serialized.empty())
            return sceneWindow;

        json j = json::parse(serialized);

        json camera;

        if (j.contains("cameraMovementSpeed"))
            j.at("cameraMovementSpeed").get_to(sceneWindow->cameraMovementSpeed);
        if (j.contains("cameraRotationSpeed"))
            j.at("cameraRotationSpeed").get_to(sceneWindow->cameraRotationSpeed);
        if (j.contains("camera"))
            j.at("camera").get_to(camera);

        sceneWindow->cameraEntity = sceneWindow->scene->CreateEntity();
        Scene::EntityFromJson(camera, sceneWindow->cameraEntity, sceneWindow->scene.Get());

        // When closing the application while playing the entity is saved in the wrong state
        sceneWindow->cameraEntity.GetComponent<CameraComponent>().isMain = true;

        return sceneWindow;

    }

    void Serializer::TryWriteToFile(const std::string& filename, const std::string& content) {

        auto fileStream = Loader::AssetLoader::WriteFile(filename, std::ios::out | std::ios::binary);

        if (!fileStream.is_open()) {
            Log::Error("Couldn't write config file");
            return;
        }

        fileStream << content;

        fileStream.close();

    }

    std::string Serializer::TryReadFromFile(const std::string& filename) {

        Loader::AssetLoader::UnpackFile(filename);
        auto path = Loader::AssetLoader::GetFullPath(filename);

        auto fileStream = Loader::AssetLoader::ReadFile(path, std::ios::in | std::ios::binary);

        if (!fileStream.is_open()) {
            return std::string();
        }

        std::string content((std::istreambuf_iterator<char>(fileStream)),
            std::istreambuf_iterator<char>());

        fileStream.close();

        return content;

    }

}